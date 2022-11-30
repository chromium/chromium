/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp.idl modified Mon Feb 11 15:48:41 2013. */

#ifndef PPAPI_C_PPP_H_
#define PPAPI_C_PPP_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb.h"

/**
 * @file
 * This file defines three functions that your module must
 * implement to interact with the browser.
 */



#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb.h"

#if __GNUC__ >= 4
#define PP_EXPORT __attribute__ ((visibility("default")))
#elif defined(_MSC_VER)
#define PP_EXPORT __declspec(dllexport)
#endif

/* {PENDING: undefine PP_EXPORT?} */

/* We don't want name mangling for these external functions.  We only need
 * 'extern "C"' if we're compiling with a C++ compiler.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PPP_InitializeModule() is the entry point for a module and is called by the
 * browser when your module loads. Your code must implement this function.
 *
 * Failure indicates to the browser that this module can not be used. In this
 * case, the module will be unloaded and ShutdownModule will NOT be called.
 *
 * @param[in] module A handle to your module. Generally you should store this
 * value since it will be required for other API calls.
 * @param[in] get_browser_interface A pointer to the function that you can
 * use to query for browser interfaces. Generally you should store this value
 * for future use.
 *
 * @return <code>PP_OK</code> on success. Any other value on failure.
 */
PP_EXPORT int32_t PPP_InitializeModule(PP_Module module,
                                       PPB_GetInterface get_browser_interface);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PPP_ShutdownModule() is <strong>sometimes</strong> called before the module
 * is unloaded. It is not recommended that you implement this function.
 *
 * There is no practical use of this function for third party modules. Its
 * existence is because of some internal use cases inside Chrome.
 *
 * Since your module runs in a separate process, there's no need to free
 * allocated memory. There is also no need to free any resources since all of
 * resources associated with an instance will be force-freed when that instance
 * is deleted.
 *
 * <strong>Note:</strong> This function will always be skipped on untrusted
 * (Native Client) implementations. This function may be skipped on trusted
 * implementations in certain circumstances when Chrome does "fast shutdown"
 * of a web page.
 */
PP_EXPORT void PPP_ShutdownModule(void);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PPP_GetInterface() is called by the browser to query the module for
 * interfaces it supports.
 *
 * Your module must implement the <code>PPP_Instance</code> interface or it
 * will be unloaded. Other interfaces are optional.
 *
 * This function is called from within browser code whenever an interface is
 * needed. This means your plugin could be reentered via this function if you
 * make a browser call and it needs an interface. Furthermore, you should not
 * make any other browser calls from within your implementation to avoid
 * reentering the browser.
 *
 * As a result, your implementation of this should merely provide a lookup
 * from the requested name to an interface pointer, via something like a big
 * if/else block or a map, and not do any other work.
 *
 * @param[in] interface_name A pointer to a "PPP" (plugin) interface name.
 * Interface names are null-terminated ASCII strings.
 *
 * @return A pointer for the interface or <code>NULL</code> if the interface is
 * not supported.
 */
PP_EXPORT const void* PPP_GetInterface(const char* interface_name);
/**
 * @}
 */

#ifdef __cplusplus
}  /* extern "C" */
#endif


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * Defines the type of the <code>PPP_InitializeModule</code> function.
 */
typedef int32_t (*PP_InitializeModule_Func)(
    PP_Module module,
    PPB_GetInterface get_browser_interface);

/**
 * Defines the type of the <code>PPP_ShutdownModule</code> function.
 */
typedef void (*PP_ShutdownModule_Func)(void);

/**
 * Defines the type of the <code>PPP_ShutdownModule</code> function.
 */
typedef const void* (*PP_GetInterface_Func)(const char* interface_name);
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_H_ */

