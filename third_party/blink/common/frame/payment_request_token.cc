// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/payment_request_token.h"

namespace blink {

namespace {

// This represents the time-span of the activate state from the moment a
// postMessage with the delegated payment token is received.  The receiver
// |Frame| is able to use the delegated token only once within this time-span.
//
// The time-span should be just long enough to allow brief async script calls.
// The exact value here came from |TransientAllowFullscreen|.
//
// TODO(mustaq): Revisit the value after we have a spec for it.
constexpr base::TimeDelta kActivationLifespan = base::Seconds(1);

}  // namespace

PaymentRequestToken::PaymentRequestToken() = default;

void PaymentRequestToken::Activate() {
  transient_state_expiry_time_ = base::TimeTicks::Now() + kActivationLifespan;
}

bool PaymentRequestToken::IsActive() const {
  return base::TimeTicks::Now() <= transient_state_expiry_time_;
}

bool PaymentRequestToken::ConsumeIfActive() {
  if (!IsActive())
    return false;
  transient_state_expiry_time_ = base::TimeTicks();
  return true;
}

}  // namespace blink
