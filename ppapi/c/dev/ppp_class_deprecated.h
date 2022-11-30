/* Copyright 2010 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_CLASS_DEPRECATED_H_
#define PPAPI_C_DEV_PPP_CLASS_DEPRECATED_H_

#include "ppapi/c/dev/deprecated_bool.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

/**
 * @file
 * Defines the PPP_Class_Deprecated struct.
 *
 * @addtogroup PPP
 * @{
 */

struct PP_Var;

/**
 * Interface for the plugin to implement JavaScript-accessible objects.
 *
 * This interface has no interface name. Instead, the plugin passes a pointer
 * to this interface to PPB_Var_Deprecated.CreateObject that corresponds to the
 * object being implemented.
 *
 * See the PPB_Var_Deprecated interface for more information on these functions.
 * This interface just allows you to implement the "back end" of those
 * functions, so most of the contract is specified in that interface.
 *
 * See
 *   http://code.google.com/p/ppapi/wiki/InterfacingWithJavaScript
 * for general information on using and implementing vars.
 */
struct PPP_Class_Deprecated {
  /**
   * |name| is guaranteed to be an integer or string type var. Exception is
   * guaranteed non-NULL. An integer is used for |name| when implementing
   * array access into the object. This test should only return true for
   * properties that are not methods.  Use HasMethod() to handle methods.
   */
  bool (*HasProperty)(void* object,
                      struct PP_Var name,
                      struct PP_Var* exception);

  /**
   * |name| is guaranteed to be a string-type. Exception is guaranteed non-NULL.
   * If the method does not exist, return false and don't set the exception.
   * Errors in this function will probably not occur in general usage, but
   * if you need to throw an exception, still return false.
   */
  bool (*HasMethod)(void* object,
                    struct PP_Var name,
                    struct PP_Var* exception);

  /**
   * |name| is guaranteed to be a string-type or an integer-type var. Exception
   * is guaranteed non-NULL. An integer is used for |name| when implementing
   * array access into the object. If the property does not exist, set the
   * exception and return a var of type Void. A property does not exist if
   * a call HasProperty() for the same |name| would return false.
   */
  struct PP_Var (*GetProperty)(void* object,
                               struct PP_Var name,
                               struct PP_Var* exception);

  /**
   * Exception is guaranteed non-NULL.
   *
   * This should include all enumerable properties, including methods. Be sure
   * to set |*property_count| to 0 and |properties| to NULL in all failure
   * cases, these should never be unset when calling this function. The
   * pointers passed in are guaranteed not to be NULL, so you don't have to
   * NULL check them.
   *
   * If you have any properties, allocate the property array with
   * PPB_Core.MemAlloc(sizeof(PP_Var) * property_count) and add a reference
   * to each property on behalf of the caller. The caller is responsible for
   * Release()ing each var and calling PPB_Core.MemFree on the property pointer.
   */
  void (*GetAllPropertyNames)(void* object,
                              uint32_t* property_count,
                              struct PP_Var** properties,
                              struct PP_Var* exception);

  /**
   * |name| is guaranteed to be an integer or string type var. Exception is
   * guaranteed non-NULL.
   */
  void (*SetProperty)(void* object,
                      struct PP_Var name,
                      struct PP_Var value,
                      struct PP_Var* exception);

  /**
   * |name| is guaranteed to be an integer or string type var. Exception is
   * guaranteed non-NULL.
   */
  void (*RemoveProperty)(void* object,
                         struct PP_Var name,
                         struct PP_Var* exception);

  // TODO(brettw) need native array access here.

  /**
   * |name| is guaranteed to be a string type var. Exception is guaranteed
   * non-NULL
   */
  struct PP_Var (*Call)(void* object,
                        struct PP_Var method_name,
                        uint32_t argc,
                        struct PP_Var* argv,
                        struct PP_Var* exception);

  /** Exception is guaranteed non-NULL. */
  struct PP_Var (*Construct)(void* object,
                             uint32_t argc,
                             struct PP_Var* argv,
                             struct PP_Var* exception);

  /**
   * Called when the reference count of the object reaches 0. Normally, plugins
   * would free their internal data pointed to by the |object| pointer.
   */
  void (*Deallocate)(void* object);
};

/**
 * @}
 * End addtogroup PPP
 */
#endif  // PPAPI_C_DEV_PPP_CLASS_DEPRECATED_H_

