/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_testing_private.idl modified Fri May  1 13:14:52 2015. */

#ifndef PPAPI_C_PRIVATE_PPB_TESTING_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_TESTING_PRIVATE_H_

#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_TESTING_PRIVATE_INTERFACE_1_0 "PPB_Testing_Private;1.0"
#define PPB_TESTING_PRIVATE_INTERFACE PPB_TESTING_PRIVATE_INTERFACE_1_0

/**
 * @file
 * This file contains interface functions used for unit testing. Do not use in
 * production code. They are not guaranteed to be available in normal plugin
 * environments so you should not depend on them.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Testing_Private_1_0 {
  /**
   * Reads the bitmap data out of the backing store for the given
   * DeviceContext2D and into the given image. If the data was successfully
   * read, it will return PP_TRUE.
   *
   * This function should not generally be necessary for normal plugin
   * operation. If you want to update portions of a device, the expectation is
   * that you will either regenerate the data, or maintain a backing store
   * pushing updates to the device from your backing store via PaintImageData.
   * Using this function will introduce an extra copy which will make your
   * plugin slower. In some cases, this may be a very expensive operation (it
   * may require slow cross-process transitions or graphics card readbacks).
   *
   * Data will be read into the image starting at |top_left| in the device
   * context, and proceeding down and to the right for as many pixels as the
   * image is large. If any part of the image bound would fall outside of the
   * backing store of the device if positioned at |top_left|, this function
   * will fail and return PP_FALSE.
   *
   * The image format must be of the format
   * PPB_ImageData.GetNativeImageDataFormat() or this function will fail and
   * return PP_FALSE.
   *
   * The returned image data will represent the current status of the backing
   * store. This will not include any paint, scroll, or replace operations
   * that have not yet been flushed; these operations are only reflected in
   * the backing store (and hence ReadImageData) until after a Flush()
   * operation has completed.
   */
  PP_Bool (*ReadImageData)(PP_Resource device_context_2d,
                           PP_Resource image,
                           const struct PP_Point* top_left);
  /**
   * Runs a nested run loop. The plugin will be reentered from this call.
   * This function is used for unit testing the API. The normal pattern is to
   * issue some asynchronous call that has a callback. Then you call
   * RunMessageLoop which will suspend the plugin and go back to processing
   * messages, giving the asynchronous operation time to complete. In your
   * callback, you save the data and call QuitMessageLoop, which will then
   * pop back up and continue with the test. This avoids having to write a
   * complicated state machine for simple tests for asynchronous APIs.
   */
  void (*RunMessageLoop)(PP_Instance instance);
  /**
   * Posts a quit message for the outermost nested run loop. Use this to
   * exit and return back to the caller after you call RunMessageLoop.
   */
  void (*QuitMessageLoop)(PP_Instance instance);
  /**
   * Returns the number of live objects (resources + strings + objects)
   * associated with this plugin instance. Used for detecting leaks. Returns
   * (uint32_t)-1 on failure.
   */
  uint32_t (*GetLiveObjectsForInstance)(PP_Instance instance);
  /**
   * Returns PP_TRUE if the plugin is running out-of-process, PP_FALSE
   * otherwise.
   */
  PP_Bool (*IsOutOfProcess)(void);
  /**
   * Passes the input event to the browser, which sends it back to the
   * plugin. The plugin should implement PPP_InputEvent and register for
   * the input event type.
   *
   * This method sends an input event through the browser just as if it had
   * come from the user. If the browser determines that it is an event for the
   * plugin, it will be sent to be handled by the plugin's PPP_InputEvent
   * interface. When generating mouse events, make sure the position is within
   * the plugin's area on the page. When generating a keyboard event, make sure
   * the plugin is focused.
   *
   * Note that the browser may generate extra input events in order to
   * maintain certain invariants, such as always having a "mouse enter" event
   * before any other mouse event. Furthermore, the event the plugin receives
   * after sending a simulated event will be slightly different from the
   * original event. The browser may change the timestamp, add modifiers, and
   * slightly alter the mouse position, due to coordinate transforms it
   * performs.
   */
  void (*SimulateInputEvent)(PP_Instance instance, PP_Resource input_event);
  /**
   * Returns the URL for the document. This is a safe way to retrieve
   * window.location.href.
   * If the canonicalized URL is valid, the method will parse the URL
   * and fill in the components structure. This pointer may be NULL
   * to specify that no component information is necessary.
   */
  struct PP_Var (*GetDocumentURL)(PP_Instance instance,
                                  struct PP_URLComponents_Dev* components);
  /**
   * Fetches up to |array_size| active PP_Vars in the tracker. Returns the
   * number of vars in the tracker. The active vars are written to |live_vars|
   * contiguously starting at index 0. The vars are not in any particular order.
   * If the number of live vars is greater than |array_size|, then an arbitrary
   * subset of |array_size| vars is written to |live_vars|. The reference count
   * of the returned PP_Vars will *not* be affected by this call.
   */
  uint32_t (*GetLiveVars)(struct PP_Var live_vars[], uint32_t array_size);
  /**
   * Sets the threshold size at which point we switch from transmitting
   * array buffers in IPC messages to using shared memory. This is only used
   * for testing purposes where we need to transmit small buffers using shmem
   * (in order to have fast tests). Passing a value of 0 resets the threshold
   * to its default. The threshold is in bytes.
   */
  void (*SetMinimumArrayBufferSizeForShmem)(PP_Instance instance,
                                            uint32_t threshold);
  /**
   * Run the V8 garbage collector for tests.
   */
  void (*RunV8GC)(PP_Instance instance);
};

typedef struct PPB_Testing_Private_1_0 PPB_Testing_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_TESTING_PRIVATE_H_ */

