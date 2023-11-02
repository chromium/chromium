/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_instance.idl modified Thu Apr 25 13:07:47 2013. */

#ifndef PPAPI_C_PPP_INSTANCE_H_
#define PPAPI_C_PPP_INSTANCE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_INSTANCE_INTERFACE_1_0 "PPP_Instance;1.0"
#define PPP_INSTANCE_INTERFACE_1_1 "PPP_Instance;1.1"
#define PPP_INSTANCE_INTERFACE PPP_INSTANCE_INTERFACE_1_1

/**
 * @file
 * This file defines the <code>PPP_Instance</code> structure - a series of
 * pointers to methods that you must implement in your module.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_Instance</code> interface contains pointers to a series of
 * functions that you must implement in your module. These functions can be
 * trivial (simply return the default return value) unless you want your module
 * to handle events such as change of focus or input events (keyboard/mouse)
 * events.
 */
struct PPP_Instance_1_1 {
  /**
   * DidCreate() is a creation handler that is called when a new instance is
   * created. This function is called for each instantiation on the page,
   * corresponding to one \<embed\> tag on the page.
   *
   * Generally you would handle this call by initializing the information
   * your module associates with an instance and creating a mapping from the
   * given <code>PP_Instance</code> handle to this data. The
   * <code>PP_Instance</code> handle will be used in subsequent calls to
   * identify which instance the call pertains to.
   *
   * It's possible for more than one instance to be created in a single module.
   * This means that you may get more than one <code>OnCreate</code> without an
   * <code>OnDestroy</code> in between, and should be prepared to maintain
   * multiple states associated with each instance.
   *
   * If this function reports a failure (by returning <code>PP_FALSE</code>),
   * the instance will be deleted.
   *
   * @param[in] instance A new <code>PP_Instance</code> identifying one
   * instance of a module. This is an opaque handle.
   *
   * @param[in] argc The number of arguments contained in <code>argn</code>
   * and <code>argv</code>.
   *
   * @param[in] argn An array of argument names.  These argument names are
   * supplied in the \<embed\> tag, for example:
   * <code>\<embed id="nacl_module" dimensions="2"\></code> will produce two
   * argument names: "id" and "dimensions."
   *
   * @param[in] argv An array of argument values.  These are the values of the
   * arguments listed in the \<embed\> tag, for example
   * <code>\<embed id="nacl_module" dimensions="2"\></code> will produce two
   * argument values: "nacl_module" and "2".  The indices of these values match
   * the indices of the corresponding names in <code>argn</code>.
   *
   * @return <code>PP_TRUE</code> on success or <code>PP_FALSE</code> on
   * failure.
   */
  PP_Bool (*DidCreate)(PP_Instance instance,
                       uint32_t argc,
                       const char* argn[],
                       const char* argv[]);
  /**
   * DidDestroy() is an instance destruction handler. This function is called
   * in many cases (see below) when a module instance is destroyed. It will be
   * called even if DidCreate() returned failure.
   *
   * Generally you will handle this call by deallocating the tracking
   * information and the <code>PP_Instance</code> mapping you created in the
   * DidCreate() call. You can also free resources associated with this
   * instance but this isn't required; all resources associated with the deleted
   * instance will be automatically freed when this function returns.
   *
   * The instance identifier will still be valid during this call, so the module
   * can perform cleanup-related tasks. Once this function returns, the
   * <code>PP_Instance</code> handle will be invalid. This means that you can't
   * do any asynchronous operations like network requests, file writes or
   * messaging from this function since they will be immediately canceled.
   *
   * <strong>Note:</strong> This function will always be skipped on untrusted
   * (Native Client) implementations. This function may be skipped on trusted
   * implementations in certain circumstances when Chrome does "fast shutdown"
   * of a web page. Fast shutdown will happen in some cases when all module
   * instances are being deleted, and no cleanup functions will be called.
   * The module will just be unloaded and the process terminated.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   */
  void (*DidDestroy)(PP_Instance instance);
  /**
   * <code>DidChangeView() is called when the position, size, or other view
   * attributes of the instance has changed.
   */
  void (*DidChangeView)(PP_Instance instance, PP_Resource view);
  /**
   * DidChangeFocus() is called when an instance has gained or lost focus.
   * Having focus means that keyboard events will be sent to the instance.
   * An instance's default condition is that it will not have focus.
   *
   * The focus flag takes into account both browser tab and window focus as
   * well as focus of the plugin element on the page. In order to be deemed
   * to have focus, the browser window must be topmost, the tab must be
   * selected in the window, and the instance must be the focused element on
   * the page.
   *
   * <strong>Note:</strong>Clicks on instances will give focus only if you
   * handle the click event. Return <code>true</code> from
   * <code>HandleInputEvent</code> in <code>PPP_InputEvent</code> (or use
   * unfiltered events) to signal that the click event was handled. Otherwise,
   * the browser will bubble the event and give focus to the element on the page
   * that actually did end up consuming it. If you're not getting focus, check
   * to make sure you're either requesting them via
   * <code>RequestInputEvents()<code> (which implicitly marks all input events
   * as consumed) or via <code>RequestFilteringInputEvents()</code> and
   * returning true from your event handler.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * receiving the input event.
   *
   * @param[in] has_focus Indicates the new focused state of the instance.
   */
  void (*DidChangeFocus)(PP_Instance instance, PP_Bool has_focus);
  /**
   * HandleDocumentLoad() is called after initialize for a full-frame
   * instance that was instantiated based on the MIME type of a DOMWindow
   * navigation. This situation only applies to modules that are pre-registered
   * to handle certain MIME types. If you haven't specifically registered to
   * handle a MIME type or aren't positive this applies to you, your
   * implementation of this function can just return <code>PP_FALSE</code>.
   *
   * The given <code>url_loader</code> corresponds to a
   * <code>PPB_URLLoader</code> instance that is already opened. Its response
   * headers may be queried using <code>PPB_URLLoader::GetResponseInfo</code>.
   * The reference count for the URL loader is not incremented automatically on
   * behalf of the module. You need to increment the reference count yourself
   * if you are going to keep a reference to it.
   *
   * This method returns <code>PP_FALSE</code> if the module cannot handle the
   * data. In response to this method, the module should call
   * ReadResponseBody() to read the incoming data.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * that should do the load.
   *
   * @param[in] url_loader An open <code>PPB_URLLoader</code> instance.
   *
   * @return <code>PP_TRUE</code> if the data was handled,
   * <code>PP_FALSE</code> otherwise.  If you return false, the load will be
   * canceled for you.
   */
  PP_Bool (*HandleDocumentLoad)(PP_Instance instance, PP_Resource url_loader);
};

typedef struct PPP_Instance_1_1 PPP_Instance;

struct PPP_Instance_1_0 {
  PP_Bool (*DidCreate)(PP_Instance instance,
                       uint32_t argc,
                       const char* argn[],
                       const char* argv[]);
  void (*DidDestroy)(PP_Instance instance);
  void (*DidChangeView)(PP_Instance instance,
                        const struct PP_Rect* position,
                        const struct PP_Rect* clip);
  void (*DidChangeFocus)(PP_Instance instance, PP_Bool has_focus);
  PP_Bool (*HandleDocumentLoad)(PP_Instance instance, PP_Resource url_loader);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_INSTANCE_H_ */

