// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/ac3.h"

#include <algorithm>
#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/base/limits.h"
#include "media/formats/mp4/rcheck.h"

namespace media {

namespace mp4 {
// Please refer to ETSI TS 102 366 V1.4.1
//     https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.03.01_60/ts_102366v010301p.pdf
//     4.4.2.3 acmod - Audio coding mode - 3 bits
// for more details.
constexpr uint8_t kGlobalChannelArray[] = {2, 1, 2, 3, 3, 4, 4, 5};

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
  if (acmod >= static_cast<int>(sizeof(kGlobalChannelArray))) {
    return false;
  }

  int lfeon;
  RCHECK(reader.ReadBits(1, &lfeon));

  channel_count_ = kGlobalChannelArray[acmod] + lfeon;
  RCHECK(channel_count_ >= 1 && channel_count_ <= limits::kMaxChannels);

  // skip bit_rate_code, reserved
  RCHECK(reader.SkipBits(5 + 5));
  return true;
}

uint32_t AC3::GetChannelCount() const {
  return channel_count_;
}

}  // namespace mp4
}  // namespace media
