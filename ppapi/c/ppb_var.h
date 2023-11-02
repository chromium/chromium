/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_var.idl modified Thu Apr 10 14:54:41 2014. */

#ifndef PPAPI_C_PPB_VAR_H_
#define PPAPI_C_PPB_VAR_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_INTERFACE_1_0 "PPB_Var;1.0"
#define PPB_VAR_INTERFACE_1_1 "PPB_Var;1.1"
#define PPB_VAR_INTERFACE_1_2 "PPB_Var;1.2"
#define PPB_VAR_INTERFACE PPB_VAR_INTERFACE_1_2

/**
 * @file
 * This file defines the <code>PPB_Var</code> struct.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * PPB_Var API
 */
struct PPB_Var_1_2 {
  /**
   * AddRef() adds a reference to the given var. If this is not a refcounted
   * object, this function will do nothing so you can always call it no matter
   * what the type.
   *
   * @param[in] var A <code>PP_Var</code> that will have a reference added.
   */
  void (*AddRef)(struct PP_Var var);
  /**
   * Release() removes a reference to given var, deleting it if the internal
   * reference count becomes 0. If the <code>PP_Var</code> is of type
   * <code>PP_VARTYPE_RESOURCE</code>,
   * it will implicitly release a reference count on the
   * <code>PP_Resource</code> (equivalent to PPB_Core::ReleaseResource()).
   *
   * If the given var is not a refcounted object, this function will do nothing
   * so you can always call it no matter what the type.
   *
   * @param[in] var A <code>PP_Var</code> that will have a reference removed.
   */
  void (*Release)(struct PP_Var var);
  /**
   * VarFromUtf8() creates a string var from a string. The string must be
   * encoded in valid UTF-8 and is NOT NULL-terminated, the length must be
   * specified in <code>len</code>. It is an error if the string is not
   * valid UTF-8.
   *
   * If the length is 0, the <code>*data</code> pointer will not be dereferenced
   * and may be <code>NULL</code>. Note, however if length is 0, the
   * "NULL-ness" will not be preserved, as VarToUtf8() will never return
   * <code>NULL</code> on success, even for empty strings.
   *
   * The resulting object will be a refcounted string object. It will be
   * AddRef'ed for the caller. When the caller is done with it, it should be
   * Released.
   *
   * On error (basically out of memory to allocate the string, or input that
   * is not valid UTF-8), this function will return a Null var.
   *
   * @param[in] data A string
   * @param[in] len The length of the string.
   *
   * @return A <code>PP_Var</code> structure containing a reference counted
   * string object.
   */
  struct PP_Var (*VarFromUtf8)(const char* data, uint32_t len);
  /**
   * VarToUtf8() converts a string-type var to a char* encoded in UTF-8. This
   * string is NOT NULL-terminated. The length will be placed in
   * <code>*len</code>. If the string is valid but empty the return value will
   * be non-NULL, but <code>*len</code> will still be 0.
   *
   * If the var is not a string, this function will return NULL and
   * <code>*len</code> will be 0.
   *
   * The returned buffer will be valid as long as the underlying var is alive.
   * If the instance frees its reference, the string will be freed and the
   * pointer will be to arbitrary memory.
   *
   * @param[in] var A PP_Var struct containing a string-type var.
   * @param[in,out] len A pointer to the length of the string-type var.
   *
   * @return A char* encoded in UTF-8.
   */
  const char* (*VarToUtf8)(struct PP_Var var, uint32_t* len);
  /**
   * Converts a resource-type var to a <code>PP_Resource</code>.
   *
   * @param[in] var A <code>PP_Var</code> struct containing a resource-type var.
   *
   * @return A <code>PP_Resource</code> retrieved from the var, or 0 if the var
   * is not a resource. The reference count of the resource is incremented on
   * behalf of the caller.
   */
  PP_Resource (*VarToResource)(struct PP_Var var);
  /**
   * Creates a new <code>PP_Var</code> from a given resource. Implicitly adds a
   * reference count on the <code>PP_Resource</code> (equivalent to
   * PPB_Core::AddRefResource(resource)).
   *
   * @param[in] resource A <code>PP_Resource</code> to be wrapped in a var.
   *
   * @return A <code>PP_Var</code> created for this resource, with type
   * <code>PP_VARTYPE_RESOURCE</code>. The reference count of the var is set to
   * 1 on behalf of the caller.
   */
  struct PP_Var (*VarFromResource)(PP_Resource resource);
};

typedef struct PPB_Var_1_2 PPB_Var;

struct PPB_Var_1_0 {
  void (*AddRef)(struct PP_Var var);
  void (*Release)(struct PP_Var var);
  struct PP_Var (*VarFromUtf8)(PP_Module module,
                               const char* data,
                               uint32_t len);
  const char* (*VarToUtf8)(struct PP_Var var, uint32_t* len);
};

struct PPB_Var_1_1 {
  void (*AddRef)(struct PP_Var var);
  void (*Release)(struct PP_Var var);
  struct PP_Var (*VarFromUtf8)(const char* data, uint32_t len);
  const char* (*VarToUtf8)(struct PP_Var var, uint32_t* len);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VAR_H_ */

