// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

// Register the viewport bottom of the nearest scrollable ancestor.
class DeferredShapingViewportScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingViewportScope>;

 public:
  DeferredShapingViewportScope(LocalFrameView& view,
                               const LayoutView& layout_view);
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

// Register the "minimum top" position of the box which is being laid out.
// A DeferredShapingMinimumTopScope instance is movable, and not copyable.
class DeferredShapingMinimumTopScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingMinimumTopScope>;

 public:
  // |input_node| - Source of LocalFrameView. It's ok to specify any layout
  //                input node if it is associated to the same LocalFrameView.
  // |minimum_top| - The value to be set to CurrentMinimumTop().
  DeferredShapingMinimumTopScope(const NGLayoutInputNode input_node,
                                 LayoutUnit minimum_top)
      : view_(input_node.GetLayoutBox()->GetFrameView()),
        previous_value_(view_->CurrentMinimumTop()) {
    view_->SetCurrentMinimumTop(PassKey(), minimum_top);
  }

  // |input_node| - Source of LocalFrameView. It's ok to specify any layout
  //                input node if it is associated to the same LocalFrameView.
  // |delta| - The value to be added to CurrentMinimumTop().
  [[nodiscard]] static DeferredShapingMinimumTopScope CreateDelta(
      const NGLayoutInputNode input_node,
      LayoutUnit delta) {
    auto& view = *input_node.GetLayoutBox()->GetFrameView();
    return DeferredShapingMinimumTopScope(input_node,
                                          view.CurrentMinimumTop() + delta);
  }

  DeferredShapingMinimumTopScope(DeferredShapingMinimumTopScope&& other)
      : view_(other.view_), previous_value_(other.previous_value_) {
    other.view_ = nullptr;
  }

  ~DeferredShapingMinimumTopScope() {
    if (view_)
      view_->SetCurrentMinimumTop(PassKey(), previous_value_);
  }

  DeferredShapingMinimumTopScope(const DeferredShapingMinimumTopScope&) =
      delete;
  DeferredShapingMinimumTopScope& operator=(
      const DeferredShapingMinimumTopScope&) = delete;

 private:
  LocalFrameView* view_;
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

// We can see logs with |--v=N| or |--vmodule=deferred_shaping=N| where N is a
// verbose level, as well as the |--enable-logging=stderr| CLI argument.
#define DEFERRED_SHAPING_VLOG(verbose_level) \
  LAZY_STREAM(                               \
      VLOG_STREAM(verbose_level),            \
      ((verbose_level) <= ::logging::GetVlogLevel("deferred_shaping")))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_
