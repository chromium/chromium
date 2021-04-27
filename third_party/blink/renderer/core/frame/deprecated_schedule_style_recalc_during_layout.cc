// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecated_schedule_style_recalc_during_layout.h"


namespace blink {

DeprecatedScheduleStyleRecalcDuringLayout::
    DeprecatedScheduleStyleRecalcDuringLayout(DocumentLifecycle& lifecycle)
    : lifecycle_(lifecycle),
      deprecated_transition_(DocumentLifecycle::kInPerformLayout,
                             DocumentLifecycle::kVisualUpdatePending),
      was_in_perform_layout_(lifecycle.GetState() ==
                             DocumentLifecycle::kInPerformLayout) {}

DeprecatedScheduleStyleRecalcDuringLayout::
    ~DeprecatedScheduleStyleRecalcDuringLayout() {
  // This block of code is intended to restore the state machine to the
  // proper state. The style recalc will still have been schedule, however.
  if (was_in_perform_layout_ &&
      lifecycle_.GetState() != DocumentLifecycle::kInPerformLayout) {
    DCHECK_EQ(lifecycle_.GetState(), DocumentLifecycle::kVisualUpdatePending);
    lifecycle_.AdvanceTo(DocumentLifecycle::kInPerformLayout);
  }
}

}  // namespace blink
