// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/fullscreen_request_token.h"

namespace blink {

FullscreenRequestToken::FullscreenRequestToken() = default;

void FullscreenRequestToken::Activate() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

bool FullscreenRequestToken::IsActive() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

bool FullscreenRequestToken::ConsumeIfActive() {
  if (!IsActive())
    return false;
  transient_state_expiry_time_ = base::TimeTicks();
  return true;
}

}  // namespace blink
