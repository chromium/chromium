// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

void FrameWidget::RequestAnimationAfterDelay(cc::BeginMainFrameReason,
                                             const base::TimeDelta& delay,
                                             bool urgent) {
  RequestAnimationAfterDelay(delay, urgent);
}

FrameWidget::~FrameWidget() = default;

}  // namespace blink
