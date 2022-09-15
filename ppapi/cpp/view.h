// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIEW_H_
#define PPAPI_CPP_VIEW_H_

#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

/// @file
/// This file defines the API for getting the state of a the view for an
/// instance.

namespace pp {

/// This class represents the state of the view for an instance and contains
/// functions for retrieving the current state of that view.
class View : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>View</code> object.
  View();

  /// Creates a View resource, taking and holding an additional reference to
  /// the given resource handle.
  explicit View(PP_Resource view_resource);

  /// GetRect() retrieves the rectangle of the module instance associated
  /// with a view changed notification relative to the upper-left of the browser
  /// viewport. This position changes when the page is scrolled.
  ///
  /// The returned rectangle may not be inside the visible portion of the
  /// viewport if the module instance is scrolled off the page. Therefore, the
  /// position may be negative or larger than the size of the page. The size
  /// will always reflect the size of the module were it to be scrolled
  /// entirely into view.
  ///
  /// In general, most modules will not need to worry about the position of the
  ///module instance in the viewport, and only need to use the size.
  ///
  /// @return The rectangle of the instance. The default return value for
  /// an invalid View is the empty rectangle.
  Rect GetRect() const;

  /// IsFullscreen() returns whether the instance is currently
  /// displaying in fullscreen mode.
  ///
  /// @return <code>true</code> if the instance is in full screen mode,
  /// or <code>false</code> if it's not or the resource is invalid.
  bool IsFullscreen() const;

  /// IsVisible() determines whether the module instance might be visible to
  /// the user. For example, the Chrome window could be minimized or another
  /// window could be over it. In both of these cases, the module instance
  /// would not be visible to the user, but IsVisible() will return true.
  ///
  /// Use the result to speed up or stop updates for invisible module
  /// instances.
  ///
  /// This function performs the duties of GetRect() (determining whether the
  /// module instance is scrolled into view and the clip rectangle is nonempty)
  /// and IsPageVisible() (whether the page is visible to the user).
  ///
  /// @return <code>true</code> if the instance might be visible to the
  /// user, <code>false</code> if it is definitely not visible.
  bool IsVisible() const;

  /// IsPageVisible() determines if the page that contains the module instance
  /// is visible. The most common cause of invisible pages is that
  /// the page is in a background tab in the browser.
  ///
  /// Most applications should use IsVisible() instead of this function since
  /// the module instance could be scrolled off of a visible page, and this
  /// function will still return true. However, depending on how your module
  /// interacts with the page, there may be certain updates that you may want
  /// to perform when the page is visible even if your specific module instance
  /// is not visible.
  ///
  /// @return <code>true</code> if the instance might be visible to the
  /// user, <code>false</code> if it is definitely not visible.
  bool IsPageVisible() const;

  /// GetClipRect() returns the clip rectangle relative to the upper-left corner
  /// of the module instance. This rectangle indicates the portions of the
  /// module instance that are scrolled into view.
  ///
  /// If the module instance is scrolled off the view, the return value will be
  /// (0, 0, 0, 0). This clip rectangle does <i>not</i> take into account page
  /// visibility. Therefore, if the module instance is scrolled into view, but
  /// the page itself is on a tab that is not visible, the return rectangle will
  /// contain the visible rectangle as though the page were visible. Refer to
  /// IsPageVisible() and IsVisible() if you want to account for page
  /// visibility.
  ///
  /// Most applications will not need to worry about the clip rectangle. The
  /// recommended behavior is to do full updates if the module instance is
  /// visible, as determined by IsVisible(), and do no updates if it is not
  /// visible.
  ///
  /// However, if the cost for computing pixels is very high for your
  /// application, or the pages you're targeting frequently have very large
  /// module instances with small visible portions, you may wish to optimize
  /// further. In this case, the clip rectangle will tell you which parts of
  /// the module to update.
  ///
  /// Note that painting of the page and sending of view changed updates
  /// happens asynchronously. This means when the user scrolls, for example,
  /// it is likely that the previous backing store of the module instance will
  /// be used for the first paint, and will be updated later when your
  /// application generates new content with the new clip. This may cause
  /// flickering at the boundaries when scrolling. If you do choose to do
  /// partial updates, you may want to think about what color the invisible
  /// portions of your backing store contain (be it transparent or some
  /// background color) or to paint a certain region outside the clip to reduce
  /// the visual distraction when this happens.
  ///
  /// @return The rectangle representing the visible part of the module
  /// instance. If the resource is invalid, the empty rectangle is returned.
  Rect GetClipRect() const;

  /// GetDeviceScale returns the scale factor between device pixels and DIPs
  /// (also known as logical pixels or UI pixels on some platforms). This allows
  /// the developer to render their contents at device resolution, even as
  /// coordinates / sizes are given in DIPs through the API.
  ///
  /// Note that the coordinate system for Pepper APIs is DIPs. Also note that
  /// one DIP might not equal one CSS pixel - when page scale/zoom is in effect.
  ///
  /// @return A <code>float</code> value representing the number of device
  /// pixels per DIP.
  float GetDeviceScale() const;

  /// GetCSSScale returns the scale factor between DIPs and CSS pixels. This
  /// allows proper scaling between DIPs - as sent via the Pepper API - and CSS
  /// pixel coordinates used for Web content.
  ///
  /// @return A <code>float</code> value representing the number of DIPs per CSS
  /// pixel.
  float GetCSSScale() const;

  /// GetScrollOffset returns the scroll offset of the window containing the
  /// plugin.
  ///
  /// @return A <code>Point</code> which is set to the value of the scroll
  /// offset in CSS pixels.
  Point GetScrollOffset() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_VIEW_H_
