// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_

#include "third_party/blink/renderer/core/layout/deferred_shaping_controller.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

// Register the viewport bottom of the nearest scrollable ancestor.
class DeferredShapingViewportScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingViewportScope>;

 public:
  explicit DeferredShapingViewportScope(const LayoutView& layout_view);
  ~DeferredShapingViewportScope() {
    ds_controller_.SetCurrentViewportBottom(PassKey(), previous_value_);
  }

  DeferredShapingViewportScope(DeferredShapingViewportScope&&) = delete;
  DeferredShapingViewportScope(const DeferredShapingViewportScope&) = delete;
  DeferredShapingViewportScope& operator=(const DeferredShapingViewportScope&) =
      delete;

 private:
  DeferredShapingController& ds_controller_;
  const LayoutUnit previous_value_;
};

// --------------------------------------------------------------------------

// Register the "minimum top" position of the box which is being laid out.
// A DeferredShapingMinimumTopScope instance is movable, and not copyable.
class DeferredShapingMinimumTopScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingMinimumTopScope>;

 public:
  // |input_node| - Source of LayoutView. It's ok to specify any layout
  //                input node if it is associated to the same LayoutView.
  // |minimum_top| - The value to be set to CurrentMinimumTop().
  DeferredShapingMinimumTopScope(const NGLayoutInputNode input_node,
                                 LayoutUnit minimum_top)
      : controller_(&DeferredShapingController::From(input_node)),
        previous_value_(controller_->CurrentMinimumTop()) {
    controller_->SetCurrentMinimumTop(PassKey(), minimum_top);
  }

  // |input_node| - Source of LayoutView. It's ok to specify any layout
  //                input node if it is associated to the same LayoutView.
  // |delta| - The value to be added to CurrentMinimumTop().
  [[nodiscard]] static DeferredShapingMinimumTopScope CreateDelta(
      const NGLayoutInputNode input_node,
      LayoutUnit delta) {
    return DeferredShapingMinimumTopScope(
        input_node,
        DeferredShapingController::From(input_node).CurrentMinimumTop() +
            delta);
  }

  DeferredShapingMinimumTopScope(DeferredShapingMinimumTopScope&& other)
      : controller_(other.controller_), previous_value_(other.previous_value_) {
    other.controller_ = nullptr;
  }

  ~DeferredShapingMinimumTopScope() {
    if (controller_)
      controller_->SetCurrentMinimumTop(PassKey(), previous_value_);
  }

  DeferredShapingMinimumTopScope(const DeferredShapingMinimumTopScope&) =
      delete;
  DeferredShapingMinimumTopScope& operator=(
      const DeferredShapingMinimumTopScope&) = delete;

 private:
  DeferredShapingController* controller_;
  const LayoutUnit previous_value_;
};

// --------------------------------------------------------------------------

// Disable Deferred Shaping while an instance of this class is alive.
class DeferredShapingDisallowScope {
  STACK_ALLOCATED();
  using PassKey = base::PassKey<DeferredShapingDisallowScope>;

 public:
  explicit DeferredShapingDisallowScope(const LayoutView& view,
                                        bool disable = true)
      : controller_(view.GetDeferredShapingController()),
        previous_value_(controller_.AllowDeferredShaping()) {
    if (disable)
      controller_.SetAllowDeferredShaping(PassKey(), false);
  }

  ~DeferredShapingDisallowScope() {
    controller_.SetAllowDeferredShaping(PassKey(), previous_value_);
  }

  DeferredShapingDisallowScope(DeferredShapingDisallowScope&&) = delete;
  DeferredShapingDisallowScope(const DeferredShapingDisallowScope&) = delete;
  DeferredShapingDisallowScope& operator=(const DeferredShapingDisallowScope&) =
      delete;

 private:
  DeferredShapingController& controller_;
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
