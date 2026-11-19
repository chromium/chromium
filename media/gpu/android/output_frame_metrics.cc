// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/output_frame_metrics.h"

#include "base/notreached.h"

namespace media {

std::string_view OutputFrameEligibilityToString(
    OutputFrameEligibility eligibility) {
  switch (eligibility) {
    case OutputFrameEligibility::kEligible:
      return "eligible";
    case OutputFrameEligibility::kFeatureDisabled:
      return "feature_disabled";
    case OutputFrameEligibility::kNoSurfaceControl:
      return "no_surface_control";
    case OutputFrameEligibility::kSecureCodec:
      return "secure_codec";
    case OutputFrameEligibility::kThreadedTextureMailboxes:
      return "threaded_texture_mailboxes";
    case OutputFrameEligibility::kNotTextureOwner:
      return "not_texture_owner";
    case OutputFrameEligibility::kSoftwareCodec:
      return "software_codec";
    case OutputFrameEligibility::kNoBlockModel:
      return "no_block_model";
  }
  NOTREACHED();
}

}  // namespace media
