// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_processing.h"

#include "base/strings/strcat.h"

namespace media {

std::string AudioProcessingSettings::ToString() const {
  auto bool_to_yes_no = [](bool b) -> const char* { return b ? "yes" : "no"; };

  return base::StrCat(
      {"aec: ", bool_to_yes_no(echo_cancellation),
       ", ns: ", bool_to_yes_no(noise_suppression),
       ", transient ns: ", bool_to_yes_no(transient_noise_suppression),
       ", gain control: ", bool_to_yes_no(automatic_gain_control),
       ", high pass filter: ", bool_to_yes_no(high_pass_filter),
       ", multichannel capture processing: ",
       bool_to_yes_no(multi_channel_capture_processing),
       ", force apm creation: ", bool_to_yes_no(force_apm_creation),
       ", stereo mirroring: ", bool_to_yes_no(stereo_mirroring)});
}

}  // namespace media
