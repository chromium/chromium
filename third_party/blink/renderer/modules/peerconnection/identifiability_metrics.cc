// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/identifiability_metrics.h"

#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_codec_capability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_capability.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

void IdentifiabilityAddRTCRtpCapabilitiesToBuilder(
    IdentifiableTokenBuilder& builder,
    const RTCRtpCapabilities& capabilities) {
  if (capabilities.hasCodecs()) {
    for (const auto& codec : capabilities.codecs()) {
      if (codec->hasMimeType()) {
        builder.AddToken(
            IdentifiabilitySensitiveStringToken(codec->mimeType()));
      } else {
        builder.AddToken(IdentifiableToken());
      }
      if (codec->hasClockRate()) {
        builder.AddValue(codec->clockRate());
      } else {
        builder.AddToken(IdentifiableToken());
      }
      if (codec->hasChannels()) {
        builder.AddValue(codec->channels());
      } else {
        builder.AddToken(IdentifiableToken());
      }
      if (codec->hasSdpFmtpLine()) {
        builder.AddToken(
            IdentifiabilitySensitiveStringToken(codec->sdpFmtpLine()));
      } else {
        builder.AddToken(IdentifiableToken());
      }
    }
  } else {
    builder.AddToken(IdentifiableToken());
  }
  if (capabilities.hasHeaderExtensions()) {
    for (const auto& header_extension : capabilities.headerExtensions()) {
      if (header_extension->hasUri()) {
        builder.AddToken(
            IdentifiabilitySensitiveStringToken(header_extension->uri()));
      } else {
        builder.AddToken(IdentifiableToken());
      }
    }
  } else {
    builder.AddToken(IdentifiableToken());
  }
}

}  // namespace blink
