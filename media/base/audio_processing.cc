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
       ", gain control: ", bool_to_yes_no(automatic_gain_control),
       ", multichannel capture processing: ",
       bool_to_yes_no(multi_channel_capture_processing),
       ", loopback aec: ", bool_to_yes_no(use_loopback_aec_reference)});
}

}  // namespace media
