// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_PAYMENT_REQUEST_TOKEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_PAYMENT_REQUEST_TOKEN_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// The |PaymentRequestToken| class represents the state of a delegated payment
// request capability in a |Frame|.  This constitutes the
// payment-request-specific part of the general Capability Delegation mechanism
// where a sender |Frame| calls JS |postMessage| with a feature-specific
// parameter to trigger a transient delegation of the feature to the receiving
// |Frame|.
//
// Design doc:
// https://docs.google.com/document/d/1IYN0mVy7yi4Afnm2Y0uda0JH8L2KwLgaBqsMVLMYXtk
class BLINK_COMMON_EXPORT PaymentRequestToken {
 public:
  PaymentRequestToken();

  // Activate the transient state.
  void Activate();

  // Returns the transient state; |true| if this object was recently activated.
  bool IsActive() const;

  // Consumes the transient activation state if available, and returns |true| if
  // successfully consumed.
  bool ConsumeIfActive();

 private:
  base::TimeTicks transient_state_expiry_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_PAYMENT_REQUEST_TOKEN_H_
