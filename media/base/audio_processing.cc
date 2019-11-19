// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_processing.h"

#include "base/strings/strcat.h"

namespace media {

std::string AudioProcessingSettings::ToString() const {
  auto agc_to_string = [](AutomaticGainControlType type) -> const char* {
    switch (type) {
      case AutomaticGainControlType::kDisabled:
        return "disabled";
      case AutomaticGainControlType::kDefault:
        return "default";
      case AutomaticGainControlType::kExperimental:
        return "experimental";
      case AutomaticGainControlType::kHybridExperimental:
        return "hybrid experimental";
    }
  };

  auto aec_to_string = [](EchoCancellationType type) -> const char* {
    switch (type) {
      case EchoCancellationType::kDisabled:
        return "disabled";
      case EchoCancellationType::kAec3:
        return "aec3";
      case EchoCancellationType::kSystemAec:
        return "system aec";
    }
  };

  auto ns_to_string = [](NoiseSuppressionType type) -> const char* {
    switch (type) {
      case NoiseSuppressionType::kDisabled:
        return "disabled";
      case NoiseSuppressionType::kDefault:
        return "default";
      case NoiseSuppressionType::kExperimental:
        return "experimental";
    }
  };

  auto bool_to_yes_no = [](bool b) -> const char* { return b ? "yes" : "no"; };

  return base::StrCat(
      {"agc: ", agc_to_string(automatic_gain_control),
       ", aec: ", aec_to_string(echo_cancellation),
       ", ns: ", ns_to_string(noise_suppression),
       ", high pass filter: ", bool_to_yes_no(high_pass_filter),
       ", typing detection: ", bool_to_yes_no(typing_detection),
       ", stereo mirroring: ", bool_to_yes_no(stereo_mirroring)});
}

}  // namespace media
