// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_codec_capability.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_header_extension_capability.h"
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
      if (codec->hasScalabilityModes()) {
        for (const auto& mode : codec->scalabilityModes()) {
          builder.AddToken(IdentifiabilitySensitiveStringToken(mode));
        }
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
