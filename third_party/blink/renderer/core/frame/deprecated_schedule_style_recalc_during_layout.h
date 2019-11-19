// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DEPRECATED_SCHEDULE_STYLE_RECALC_DURING_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DEPRECATED_SCHEDULE_STYLE_RECALC_DURING_LAYOUT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DeprecatedScheduleStyleRecalcDuringLayout {
  STACK_ALLOCATED();

 public:
  explicit DeprecatedScheduleStyleRecalcDuringLayout(DocumentLifecycle&);
  ~DeprecatedScheduleStyleRecalcDuringLayout();

 private:
  DocumentLifecycle& lifecycle_;
  DocumentLifecycle::DeprecatedTransition deprecated_transition_;
  bool was_in_perform_layout_;
  DISALLOW_COPY_AND_ASSIGN(DeprecatedScheduleStyleRecalcDuringLayout);
};

}  // namespace blink

#endif
