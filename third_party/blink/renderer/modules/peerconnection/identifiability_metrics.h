// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_IDENTIFIABILITY_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_IDENTIFIABILITY_METRICS_H_

#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_capabilities.h"

namespace blink {

void IdentifiabilityAddRTCRtpCapabilitiesToBuilder(
    IdentifiableTokenBuilder& builder,
    const RTCRtpCapabilities& capabilities);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_IDENTIFIABILITY_METRICS_H_
