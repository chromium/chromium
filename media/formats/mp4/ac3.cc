// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/ac3.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/base/limits.h"
#include "media/formats/mp4/ac3_constants.h"
#include "media/formats/mp4/rcheck.h"

namespace media {

namespace mp4 {

AC3::AC3() = default;

AC3::AC3(const AC3& other) = default;

AC3::~AC3() = default;

bool AC3::Parse(const std::vector<uint8_t>& data, MediaLog* media_log) {
  if (data.empty()) {
    return false;
  }

  // For AC3SpecificBox, Please refer to ETSI TS 102 366 V1.4.1
  //    https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.03.01_60/ts_102366v010301p.pdf
  //    F.4 AC3SpecificBox
  //        fscod           2 bits
  //        bsid            5 bits
  //        bsmod           3 bits
  //        acmod           3 bits
  //        lfeon           1 bits
  //        bit_rate_code   5 bits
  //        reserved        5 bits

  if (data.size() * 8 < (2 + 5 + 3 + 3 + 1 + 5 + 5)) {
    return false;
  }

  // Parse dac3 box using reader.
  BitReader reader(&data[0], data.size());

  // skip fscod, bsid, bsmod
  RCHECK(reader.SkipBits(2 + 5 + 3));

  int acmod;
  RCHECK(reader.ReadBits(3, &acmod));
  if (acmod >= kAC3AudioCodingModeSize) {
    return false;
  }

  int lfeon;
  RCHECK(reader.ReadBits(1, &lfeon));

  channel_layout_ = kAC3AudioCodingModeTable[lfeon][acmod];
  RCHECK(channel_layout_ > CHANNEL_LAYOUT_UNSUPPORTED);
  channel_count_ = ChannelLayoutToChannelCount(channel_layout_);
  RCHECK(channel_count_ >= 1 && channel_count_ <= limits::kMaxChannels);

  // skip bit_rate_code, reserved
  RCHECK(reader.SkipBits(5 + 5));
  return true;
}

uint32_t AC3::GetChannelCount() const {
  return channel_count_;
}

ChannelLayout AC3::GetChannelLayout() const {
  return channel_layout_;
}

}  // namespace mp4
}  // namespace media
