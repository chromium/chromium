// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

// Register the viewport bottom of the nearest scrollable ancestor.
class DeferredShapingViewportScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingViewportScope>;

 public:
  DeferredShapingViewportScope(LocalFrameView& view, LayoutUnit viewport_bottom)
      : view_(view), previous_value_(view.CurrentViewportBottom()) {
    view_.SetCurrentViewportBottom(PassKey(), viewport_bottom);
  }

  ~DeferredShapingViewportScope() {
    view_.SetCurrentViewportBottom(PassKey(), previous_value_);
  }

  DeferredShapingViewportScope(DeferredShapingViewportScope&&) = delete;
  DeferredShapingViewportScope(const DeferredShapingViewportScope&) = delete;
  DeferredShapingViewportScope& operator=(const DeferredShapingViewportScope&) =
      delete;

 private:
  LocalFrameView& view_;
  const LayoutUnit previous_value_;
};

// --------------------------------------------------------------------------

// Disable Deferred Shaping while an instance of this class is alive.
class DeferredShapingDisallowScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingDisallowScope>;

 public:
  explicit DeferredShapingDisallowScope(LocalFrameView& view,
                                        bool disable = true)
      : view_(view), previous_value_(view.AllowDeferredShaping()) {
    if (disable)
      view_.SetAllowDeferredShaping(PassKey(), false);
  }

  ~DeferredShapingDisallowScope() {
    view_.SetAllowDeferredShaping(PassKey(), previous_value_);
  }

  DeferredShapingDisallowScope(DeferredShapingDisallowScope&&) = delete;
  DeferredShapingDisallowScope(const DeferredShapingDisallowScope&) = delete;
  DeferredShapingDisallowScope& operator=(const DeferredShapingDisallowScope&) =
      delete;

 private:
  LocalFrameView& view_;
  const bool previous_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_
