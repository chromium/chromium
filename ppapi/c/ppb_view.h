/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_view.idl modified Fri Sep  5 11:32:22 2014. */

#ifndef PPAPI_C_PPB_VIEW_H_
#define PPAPI_C_PPB_VIEW_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_VIEW_INTERFACE_1_0 "PPB_View;1.0"
#define PPB_VIEW_INTERFACE_1_1 "PPB_View;1.1"
#define PPB_VIEW_INTERFACE_1_2 "PPB_View;1.2"
#define PPB_VIEW_INTERFACE PPB_VIEW_INTERFACE_1_2

/**
 * @file
 * This file defines the <code>PPB_View</code> struct representing the state
 * of the view of an instance.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * <code>PPB_View</code> represents the state of the view of an instance.
 * You will receive new view information using
 * <code>PPP_Instance.DidChangeView</code>.
 */
struct PPB_View_1_2 {
  /**
   * IsView() determines if the given resource is a valid
   * <code>PPB_View</code> resource. Note that <code>PPB_ViewChanged</code>
   * resources derive from <code>PPB_View</code> and will return true here
   * as well.
   *
   * @param resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return <code>PP_TRUE</code> if the given resource supports
   * <code>PPB_View</code> or <code>PP_FALSE</code> if it is an invalid
   * resource or is a resource of another type.
   */
  PP_Bool (*IsView)(PP_Resource resource);
  /**
   * GetRect() retrieves the rectangle of the module instance associated
   * with a view changed notification relative to the upper-left of the browser
   * viewport. This position changes when the page is scrolled.
   *
   * The returned rectangle may not be inside the visible portion of the
   * viewport if the module instance is scrolled off the page. Therefore, the
   * position may be negative or larger than the size of the page. The size will
   * always reflect the size of the module were it to be scrolled entirely into
   * view.
   *
   * In general, most modules will not need to worry about the position of the
   * module instance in the viewport, and only need to use the size.
   *
   * @param resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @param rect A <code>PP_Rect</code> receiving the rectangle on success.
   *
   * @return Returns <code>PP_TRUE</code> if the resource was valid and the
   * viewport rectangle was filled in, <code>PP_FALSE</code> if not.
   */
  PP_Bool (*GetRect)(PP_Resource resource, struct PP_Rect* rect);
  /**
   * IsFullscreen() returns whether the instance is currently
   * displaying in fullscreen mode.
   *
   * @param resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return <code>PP_TRUE</code> if the instance is in full screen mode,
   * or <code>PP_FALSE</code> if it's not or the resource is invalid.
   */
  PP_Bool (*IsFullscreen)(PP_Resource resource);
  /**
   * IsVisible() determines whether the module instance might be visible to
   * the user. For example, the Chrome window could be minimized or another
   * window could be over it. In both of these cases, the module instance
   * would not be visible to the user, but IsVisible() will return true.
   *
   * Use the result to speed up or stop updates for invisible module
   * instances.
   *
   * This function performs the duties of GetRect() (determining whether the
   * module instance is scrolled into view and the clip rectangle is nonempty)
   * and IsPageVisible() (whether the page is visible to the user).
   *
   * @param resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return <code>PP_TRUE</code> if the instance might be visible to the
   * user, <code>PP_FALSE</code> if it is definitely not visible.
   */
  PP_Bool (*IsVisible)(PP_Resource resource);
  /**
   * IsPageVisible() determines if the page that contains the module instance
   * is visible. The most common cause of invisible pages is that
   * the page is in a background tab in the browser.
   *
   * Most applications should use IsVisible() instead of this function since
   * the module instance could be scrolled off of a visible page, and this
   * function will still return true. However, depending on how your module
   * interacts with the page, there may be certain updates that you may want to
   * perform when the page is visible even if your specific module instance is
   * not visible.
   *
   * @param resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return <code>PP_TRUE</code> if the instance is plausibly visible to the
   * user, <code>PP_FALSE</code> if it is definitely not visible.
   */
  PP_Bool (*IsPageVisible)(PP_Resource resource);
  /**
   * GetClipRect() returns the clip rectangle relative to the upper-left corner
   * of the module instance. This rectangle indicates the portions of the module
   * instance that are scrolled into view.
   *
   * If the module instance is scrolled off the view, the return value will be
   * (0, 0, 0, 0). This clip rectangle does <i>not</i> take into account page
   * visibility. Therefore, if the module instance is scrolled into view, but
   * the page itself is on a tab that is not visible, the return rectangle will
   * contain the visible rectangle as though the page were visible. Refer to
   * IsPageVisible() and IsVisible() if you want to account for page
   * visibility.
   *
   * Most applications will not need to worry about the clip rectangle. The
   * recommended behavior is to do full updates if the module instance is
   * visible, as determined by IsVisible(), and do no updates if it is not
   * visible.
   *
   * However, if the cost for computing pixels is very high for your
   * application, or the pages you're targeting frequently have very large
   * module instances with small visible portions, you may wish to optimize
   * further. In this case, the clip rectangle will tell you which parts of
   * the module to update.
   *
   * Note that painting of the page and sending of view changed updates
   * happens asynchronously. This means when the user scrolls, for example,
   * it is likely that the previous backing store of the module instance will
   * be used for the first paint, and will be updated later when your
   * application generates new content with the new clip. This may cause
   * flickering at the boundaries when scrolling. If you do choose to do
   * partial updates, you may want to think about what color the invisible
   * portions of your backing store contain (be it transparent or some
   * background color) or to paint a certain region outside the clip to reduce
   * the visual distraction when this happens.
   *
   * @param resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @param clip Output argument receiving the clip rect on success.
   *
   * @return Returns <code>PP_TRUE</code> if the resource was valid and the
   * clip rect was filled in, <code>PP_FALSE</code> if not.
   */
  PP_Bool (*GetClipRect)(PP_Resource resource, struct PP_Rect* clip);
  /**
   * GetDeviceScale returns the scale factor between device pixels and Density
   * Independent Pixels (DIPs, also known as logical pixels or UI pixels on
   * some platforms). This allows the developer to render their contents at
   * device resolution, even as coordinates / sizes are given in DIPs through
   * the API.
   *
   * Note that the coordinate system for Pepper APIs is DIPs. Also note that
   * one DIP might not equal one CSS pixel - when page scale/zoom is in effect.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return A <code>float</code> value representing the number of device pixels
   * per DIP. If the resource is invalid, the value will be 0.0.
   */
  float (*GetDeviceScale)(PP_Resource resource);
  /**
   * GetCSSScale returns the scale factor between DIPs and CSS pixels. This
   * allows proper scaling between DIPs - as sent via the Pepper API - and CSS
   * pixel coordinates used for Web content.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return css_scale A <code>float</code> value representing the number of
   * DIPs per CSS pixel. If the resource is invalid, the value will be 0.0.
   */
  float (*GetCSSScale)(PP_Resource resource);
  /**
   * GetScrollOffset returns the scroll offset of the window containing the
   * plugin.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @param[out] offset A <code>PP_Point</code> which will be set to the value
   * of the scroll offset in CSS pixels.
   *
   * @return Returns <code>PP_TRUE</code> if the resource was valid and the
   * offset was filled in, <code>PP_FALSE</code> if not.
   */
  PP_Bool (*GetScrollOffset)(PP_Resource resource, struct PP_Point* offset);
};

typedef struct PPB_View_1_2 PPB_View;

struct PPB_View_1_0 {
  PP_Bool (*IsView)(PP_Resource resource);
  PP_Bool (*GetRect)(PP_Resource resource, struct PP_Rect* rect);
  PP_Bool (*IsFullscreen)(PP_Resource resource);
  PP_Bool (*IsVisible)(PP_Resource resource);
  PP_Bool (*IsPageVisible)(PP_Resource resource);
  PP_Bool (*GetClipRect)(PP_Resource resource, struct PP_Rect* clip);
};

struct PPB_View_1_1 {
  PP_Bool (*IsView)(PP_Resource resource);
  PP_Bool (*GetRect)(PP_Resource resource, struct PP_Rect* rect);
  PP_Bool (*IsFullscreen)(PP_Resource resource);
  PP_Bool (*IsVisible)(PP_Resource resource);
  PP_Bool (*IsPageVisible)(PP_Resource resource);
  PP_Bool (*GetClipRect)(PP_Resource resource, struct PP_Rect* clip);
  float (*GetDeviceScale)(PP_Resource resource);
  float (*GetCSSScale)(PP_Resource resource);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VIEW_H_ */

