// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEFERRED_SHAPING_H_

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

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
