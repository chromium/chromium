// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_PAGE_VIEWPORT_STATE_H_
#define IOS_WEB_WEB_STATE_PAGE_VIEWPORT_STATE_H_

#import <Foundation/Foundation.h>

namespace web {

// Class to describe the viewport width or height.
class ViewportLength {
 public:
  // Default constructor.  Initializes values to false and NAN.
  explicit ViewportLength();
  // Constructor with the dimension value specified by the viewport tag.
  // Possible valid values include "device-width", "device-height", or numerical
  // strings.
  ViewportLength(NSString* value);
  ~ViewportLength();

  // Accessors.
  bool use_device_length() const { return use_device_length_; }
  double value() const { return value_; }

 private:
  // Whether the dimension is specified to use the device height/width.
  bool use_device_length_;
  // The hardcoded dimension value.  Will be NAN if unspecified or if
  // `use_device_length_` is true.
  double value_;
};

// Class that describes the viewport meta tag of a page.  The parsed values for
// this class correspond with the actual contents of viewport tag within the
// page's HTML.  The zoom scale values are often different than those recorded
// in a page's PageZoomState, which correspond to the web view's scroll view
// zooming properties.
class PageViewportState {
 public:
  // Default constructor.  Initializes all bools to false and doubles to NAN.
  explicit PageViewportState();
  // Constructor with a viewport tag's content string.
  explicit PageViewportState(NSString* viewport_content);
  ~PageViewportState();

  // Parses `viewport_content` and updates properties accordingly.
  void UpdateWithViewportContent(NSString* const viewport_content);

  // Accessors.
  bool viewport_tag_present() const { return viewport_tag_present_; }
  bool user_scalable() const { return user_scalable_; }
  const ViewportLength& width() const { return width_; }
  const ViewportLength& height() const { return height_; }
  double minimum_zoom_scale() const { return minimum_zoom_scale_; }
  double maximum_zoom_scale() const { return maximum_zoom_scale_; }
  double initial_zoom_scale() const { return initial_zoom_scale_; }

 private:
  // Whether the page has a viewport meta tag.
  bool viewport_tag_present_;
  // Whether the viewport tag allows user scalability.  Defaults to true if the
  // value is not found in the viewport tag's content.
  bool user_scalable_;
  // The specified width and height.
  ViewportLength width_;
  ViewportLength height_;
  // The minimum, maximum, and initial zoom scales.  These values are not always
  // the same as those reported in PageZoomState, as web views render various
  // viewport values differently.  These values will be NAN if they aren't
  // present in the extracted viewport tag.
  double minimum_zoom_scale_;
  double maximum_zoom_scale_;
  double initial_zoom_scale_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_PAGE_VIEWPORT_STATE_H_
