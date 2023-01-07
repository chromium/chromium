// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/transient_allow_popup.h"

namespace content {

TransientAllowPopup::TransientAllowPopup() = default;

void TransientAllowPopup::Activate() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

void TransientAllowPopup::Deactivate() {
  transient_state_expiry_time_ = base::TimeTicks();
}

bool TransientAllowPopup::IsActive() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

}  // namespace content
