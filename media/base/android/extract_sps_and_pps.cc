// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/extract_sps_and_pps.h"

#include <array>

#include "media/formats/mp4/box_definitions.h"

namespace media {

void ExtractSpsAndPps(const std::vector<uint8_t>& avcc,
                      std::vector<uint8_t>* sps_out,
                      std::vector<uint8_t>* pps_out) {
  if (avcc.empty())
    return;

  mp4::AVCDecoderConfigurationRecord record;
  if (!record.Parse(avcc.data(), avcc.size())) {
    DVLOG(1) << "Failed to extract SPS and PPS";
    return;
  }

  constexpr std::array<uint8_t, 4> prefix = {{0, 0, 0, 1}};
  for (const std::vector<uint8_t>& sps : record.sps_list) {
    sps_out->insert(sps_out->end(), prefix.begin(), prefix.end());
    sps_out->insert(sps_out->end(), sps.begin(), sps.end());
  }

  for (const std::vector<uint8_t>& pps : record.pps_list) {
    pps_out->insert(pps_out->end(), prefix.begin(), prefix.end());
    pps_out->insert(pps_out->end(), pps.begin(), pps.end());
  }
}

}  // namespace media
