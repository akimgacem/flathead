/*
 * props.c -- Property access and prototype/scope traversals
 *
 * Copyright (c) 2012 Nick Reynolds
 *  
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "props.h"

// ----------------------------------------------------------------------------
// Get a property
// ----------------------------------------------------------------------------

/**
 * Lookup a property on an object, resolve the value, and return it.
 */
JSValue *
fh_get(JSValue *obj, char *name)
{
  // We can't read properties from undefined.
  if (obj->type == T_UNDEF)
    fh_error(NULL, E_TYPE, "Cannot read property '%s' of undefined", name);

  // But we'll happily return undefined if a property doesn't exist.
  JSProp *prop = fh_get_prop(obj, name);
  return prop ? prop->ptr : JSUNDEF();
}

/**
 * Same as `fh_get`, but recurse the scope chain.
 */
JSValue *
fh_get_rec(JSValue *obj, char *name) 
{
  JSProp *prop = fh_get_prop_rec(obj, name);
  return prop ? prop->ptr : JSUNDEF();
}

/**
 * Same as `fh_get`, but recurse the prototype chain (if one exists).
 */
JSValue *
fh_get_proto(JSValue *obj, char *name)
{
  JSProp *prop = fh_get_prop_proto(obj, name);
  JSValue *val = prop ? prop->ptr : JSUNDEF();
  // Store a ref to the instance for natively define methods.
  if (val->type == T_FUNCTION) {
    val->function.instance = obj;
  }
  return val;
}

/**
 * Lookup a property on an object and return it.
 */
JSProp *
fh_get_prop(JSValue *obj, char *name)
{
  JSProp *prop = NULL;
  if (obj->object.map)
    HASH_FIND_STR(obj->object.map, name, prop);
  return prop;
}

JSProp *
fh_get_prop_rec(JSValue *obj, char *name)
{
  JSProp *prop = fh_get_prop(obj, name);
  if (prop == NULL && obj->object.parent != NULL)
    return fh_get_prop_rec(obj->object.parent, name);
  return prop;
}

JSProp *
fh_get_prop_proto(JSValue *obj, char *name)
{
  JSProp *prop = fh_get_prop(obj, name);
  if (prop == NULL && obj->proto != NULL)
    return fh_get_prop_proto(obj->proto, name);
  return prop;
}


// ----------------------------------------------------------------------------
// Set a property
// ----------------------------------------------------------------------------

/**
 * Set a property on an object using the provided name and value, and the 
 * default property flags.
 */
void
fh_set(JSValue *obj, char *name, JSValue *val)
{
  fh_set_prop(obj, name, val, P_DEFAULT);
}

/**
 * Set a property on an object using the provided name, value, and property
 * flags.
 */
void
fh_set_prop(JSValue *obj, char *name, JSValue *val, JSPropFlags flags)
{
  bool add = false;
  JSProp *prop = fh_get_prop(obj, name);
  if (prop == NULL) {
    prop = fh_new_prop(P_DEFAULT);
    add = true;
  }

  prop->writable = flags & P_WRITE;
  prop->configurable = flags & P_CONF;
  prop->enumerable = flags & P_ENUM;

  prop->name = malloc((strlen(name) + 1) * sizeof(char));
  strcpy(prop->name, name);
  prop->ptr = val;
  prop->circular = prop->ptr == obj ? 1 : 0; // Do we have a circular reference?

  // Don't add if it already exists (bad things happen).
  if (add)
    HASH_ADD_KEYPTR(hh, obj->object.map, prop->name, strlen(prop->name), prop);
}

/*
 * Set a property on the given object, or -- if not defined -- the closest
 * parent scope on which the name is already defined.
 */
void
fh_set_rec(JSValue *obj, char *name, JSValue *val)
{
  JSValue *scope_to_set = obj;
  JSValue *parent = NULL;

  // Try and find the property in a parent scope.
  JSProp *prop = fh_get_prop(obj, name);
  while(prop == NULL) {
    if (obj->object.parent == NULL) break;
    parent = obj->object.parent;
    prop = fh_get_prop(parent, name);
    obj = parent;
  }
  if (prop != NULL && parent != NULL)
    scope_to_set = parent;

  fh_set(scope_to_set, name, val);
}

// ----------------------------------------------------------------------------
// Delete a property
// ----------------------------------------------------------------------------

/**
 * Find and delete a property from an object by name.
 */
void
fh_del_prop(JSValue *obj, char *name)
{
  JSProp *deletee = fh_get_prop(obj, name);
  if (deletee != NULL)
    HASH_DEL(obj->object.map, deletee);
}