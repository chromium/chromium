/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppp_instance_private.idl modified Thu Mar 28 10:22:54 2013. */

#ifndef PPAPI_C_PRIVATE_PPP_INSTANCE_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPP_INSTANCE_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPP_INSTANCE_PRIVATE_INTERFACE_0_1 "PPP_Instance_Private;0.1"
#define PPP_INSTANCE_PRIVATE_INTERFACE PPP_INSTANCE_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the PPP_InstancePrivate structure; a series of functions
 * that a trusted plugin may implement to provide capabilities only available
 * to trusted plugins.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The PPP_Instance_Private interface contains pointers to a series of
 * functions that may be implemented in a trusted plugin to provide capabilities
 * that aren't possible in untrusted modules.
 */
struct PPP_Instance_Private_0_1 {
  /**
   * GetInstanceObject returns a PP_Var representing the scriptable object for
   * the given instance. Normally this will be a PPP_Class_Deprecated object
   * that exposes methods and properties to JavaScript.
   *
   * On Failure, the returned PP_Var should be a "void" var.
   *
   * The returned PP_Var should have a reference added for the caller, which
   * will be responsible for Release()ing that reference.
   *
   * @param[in] instance A PP_Instance identifying the instance from which the
   *            instance object is being requested.
   * @return A PP_Var containing scriptable object.
   */
  struct PP_Var (*GetInstanceObject)(PP_Instance instance);
};

typedef struct PPP_Instance_Private_0_1 PPP_Instance_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPP_INSTANCE_PRIVATE_H_ */

