// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_PAGE_DISPLAY_STATE_H_
#define IOS_WEB_PUBLIC_UI_PAGE_DISPLAY_STATE_H_

#import <UIKit/UIKit.h>

namespace web {

// Class used to represent the scrolling state of a webview.
class PageScrollState {
 public:
  // Default constructor.  Initializes scroll values to NAN.
  PageScrollState();
  // Constructor with initial values.
  PageScrollState(const CGPoint& content_offset,
                  const UIEdgeInsets& content_inset);
  ~PageScrollState();

  // The scroll offset is valid if its content inset and offset contain only
  // non-NAN values.
  bool IsValid() const;

  // Returns the content offset that produces an equivalent scroll offset when
  // applied to a UIScrollView whose contentInset is |content_inset|.
  CGPoint GetEffectiveContentOffsetForContentInset(
      UIEdgeInsets content_inset) const;

  // Accessors for scroll offsets and zoom scale.
  const CGPoint& content_offset() const { return content_offset_; }
  CGPoint& content_offset() { return content_offset_; }
  void set_content_offset(const CGPoint& content_offset) {
    content_offset_ = content_offset;
  }
  const UIEdgeInsets& content_inset() const { return content_inset_; }
  UIEdgeInsets& content_inset() { return content_inset_; }
  void set_content_inset(const UIEdgeInsets& content_inset) {
    content_inset_ = content_inset;
  }

  // Comparator operators.
  bool operator==(const PageScrollState& other) const;
  bool operator!=(const PageScrollState& other) const;

 private:
  // The content offset and content inset of the web view scroll view.
  CGPoint content_offset_;
  UIEdgeInsets content_inset_;
};

// Class used to represent the scrolling offset and the zoom scale of a webview.
class PageZoomState {
 public:
  // Default constructor.  Initializes scroll offsets and zoom scales to NAN.
  PageZoomState();
  // Constructor with initial values.
  PageZoomState(CGFloat minimum_zoom_scale,
                CGFloat maximum_zoom_scale,
                CGFloat zoom_scale);
  ~PageZoomState();

  // Non-legacy zoom scales are valid if all three values are non-NAN and the
  // zoom scale is within the minimum and maximum scales.  Legacy-format
  // PageScrollStates are considered valid if the minimum and maximum scales
  // are NAN and the zoom scale is greater than zero.
  bool IsValid() const;

  // Returns the allowed zoom scale range for this scroll state.
  CGFloat GetMinMaxZoomDifference() const {
    return maximum_zoom_scale_ - minimum_zoom_scale_;
  }

  // Accessors.
  CGFloat minimum_zoom_scale() const { return minimum_zoom_scale_; }
  void set_minimum_zoom_scale(CGFloat minimum_zoom_scale) {
    minimum_zoom_scale_ = minimum_zoom_scale;
  }
  CGFloat maximum_zoom_scale() const { return maximum_zoom_scale_; }
  void set_maximum_zoom_scale(CGFloat maximum_zoom_scale) {
    maximum_zoom_scale_ = maximum_zoom_scale;
  }
  CGFloat zoom_scale() const { return zoom_scale_; }
  void set_zoom_scale(CGFloat zoom_scale) { zoom_scale_ = zoom_scale; }

  // Comparator operators.
  bool operator==(const PageZoomState& other) const;
  bool operator!=(const PageZoomState& other) const;

 private:
  // The minimumZoomScale value of the page's UIScrollView.
  CGFloat minimum_zoom_scale_;
  // The maximumZoomScale value of the page's UIScrollView.
  CGFloat maximum_zoom_scale_;
  // The zoomScale value of the page's UIScrollView.
  CGFloat zoom_scale_;
};

// Class used to represent the scroll offset and zoom scale of a webview.
class PageDisplayState {
 public:
  // Default constructor.  Initializes scroll offsets and zoom scales to NAN.
  PageDisplayState();
  // Constructor with initial values.
  PageDisplayState(const PageScrollState& scroll_state,
                   const PageZoomState& zoom_state);
  PageDisplayState(const CGPoint& content_offset,
                   const UIEdgeInsets& content_inset,
                   CGFloat minimum_zoom_scale,
                   CGFloat maximum_zoom_scale,
                   CGFloat zoom_scale);
  PageDisplayState(NSDictionary* serialization);
  ~PageDisplayState();

  // PageScrollStates cannot be applied until the scroll offset and zoom scale
  // are both valid.
  bool IsValid() const;

  // Accessors.
  const PageScrollState& scroll_state() const { return scroll_state_; }
  PageScrollState& scroll_state() { return scroll_state_; }
  void set_scroll_state(const PageScrollState& scroll_state) {
    scroll_state_ = scroll_state;
  }
  const PageZoomState& zoom_state() const { return zoom_state_; }
  PageZoomState& zoom_state() { return zoom_state_; }
  void set_zoom_state(const PageZoomState& zoom_state) {
    zoom_state_ = zoom_state;
  }

  // Comparator operators.
  bool operator==(const PageDisplayState& other) const;
  bool operator!=(const PageDisplayState& other) const;

  // Returns a serialized representation of the PageDisplayState.
  NSDictionary* GetSerialization() const;

  // Returns a description string for the PageDisplayState.
  NSString* GetDescription() const;

 private:
  // The scroll state for the page's UIScrollView.
  PageScrollState scroll_state_;
  // The zoom state for the page's UIScrollView.
  PageZoomState zoom_state_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_PAGE_DISPLAY_STATE_H_
