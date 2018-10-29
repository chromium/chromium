// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLL_INTO_VIEW_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLL_INTO_VIEW_PARAMS_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_float_rect.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/scroll/scroll_alignment.h"  // nogncheck
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"  // nogncheck
#endif

namespace blink {
// This struct contains the required information for propagating some stages
// of scrolling process to cross process frames. Specifically for various types
// of programmatic scrolling such as scrolling an element into view, recursive
// scrolling across multiple processes is accommodated through passing the state
// using this struct to the browser and then to the target (parent) process.
struct WebScrollIntoViewParams {
  // Public variant of ScrollAlignmentBehavior.
  enum AlignmentBehavior {
    kNoScroll = 0,
    kCenter,
    kTop,
    kBottom,
    kLeft,
    kRight,
    kClosestEdge,
    kLastAlignmentBehavior = kClosestEdge
  };

  // A public wrapper around ScrollAlignment. The default value matches
  // kAlignCenterIfNeeded.
  struct Alignment {
    Alignment() = default;
#if INSIDE_BLINK
    BLINK_EXPORT Alignment(const ScrollAlignment&);
#endif
    AlignmentBehavior rect_visible = kNoScroll;
    AlignmentBehavior rect_hidden = kCenter;
    AlignmentBehavior rect_partial = kClosestEdge;
  };

  // Scroll type set through 'scroll-type' CSS property.
  enum Type {
    kUser = 0,
    kProgrammatic,
    kClamping,
    kCompositor,
    kAnchoring,
    kSequenced,
    kLastType = kSequenced,
  };

  // The scroll behavior set through 'scroll-behavior' CSS property.
  enum Behavior {
    kAuto = 0,
    kInstant,
    kSmooth,
    kLastBehavior = kSmooth,
  };

  Alignment align_x;
  Alignment align_y;
  Type type = kProgrammatic;
  bool make_visible_in_visual_viewport = true;
  Behavior behavior = kAuto;
  bool is_for_scroll_sequence = false;

  // If true, once the root frame scrolls into view it will zoom into the scroll
  // rect.
  bool zoom_into_rect = false;

  // The following bounds are normalized to the scrolling rect, i.e., to
  // retrieve the approximate bounds in root layer's document, the relative
  // bounds should be scaled by the width and height of the scrolling rect in x
  // and y coordinates respectively (and then offset by the rect's location).
  WebFloatRect relative_element_bounds = WebFloatRect();
  WebFloatRect relative_caret_bounds = WebFloatRect();

  // If true, avoid recursing the ScrollIntoView into the layout viewport of
  // the main frame. This is so that we can then do a smooth page scale
  // animation that zooms both layout and visual viewport into a focused
  // editable element.
  bool stop_at_main_frame_layout_viewport = false;

  WebScrollIntoViewParams() = default;
#if INSIDE_BLINK
  BLINK_EXPORT WebScrollIntoViewParams(
      ScrollAlignment,
      ScrollAlignment,
      ScrollType scroll_type = kProgrammaticScroll,
      bool make_visible_in_visual_viewport = true,
      ScrollBehavior scroll_behavior = kScrollBehaviorAuto,
      bool is_for_scroll_sequence = false,
      bool zoom_into_rect = false);

  BLINK_EXPORT ScrollAlignment GetScrollAlignmentX() const;

  BLINK_EXPORT ScrollAlignment GetScrollAlignmentY() const;

  BLINK_EXPORT ScrollType GetScrollType() const;

  BLINK_EXPORT ScrollBehavior GetScrollBehavior() const;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLL_INTO_VIEW_PARAMS_H_
