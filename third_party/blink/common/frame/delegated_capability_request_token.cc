// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/delegated_capability_request_token.h"

namespace blink {

DelegatedCapabilityRequestToken::DelegatedCapabilityRequestToken() = default;

void DelegatedCapabilityRequestToken::Activate() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

bool DelegatedCapabilityRequestToken::IsActive() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

bool DelegatedCapabilityRequestToken::ConsumeIfActive() {
  if (!IsActive()) {
    return false;
  }
  transient_state_expiry_time_ = base::TimeTicks();
  return true;
}

}  // namespace blink
