// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/transient_allow_fullscreen.h"

namespace blink {

TransientAllowFullscreen::TransientAllowFullscreen() = default;

void TransientAllowFullscreen::Activate() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

bool TransientAllowFullscreen::IsActive() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

}  // namespace blink
