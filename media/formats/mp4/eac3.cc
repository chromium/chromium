// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/eac3.h"

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

EAC3::EAC3() = default;

EAC3::EAC3(const EAC3& other) = default;

EAC3::~EAC3() = default;

bool EAC3::Parse(const std::vector<uint8_t>& data, MediaLog* media_log) {
  if (data.empty()) {
    return false;
  }

  // Parse dec3 box using reader.
  BitReader reader(&data[0], data.size());

  // skip data_rate
  RCHECK(reader.SkipBits(13));

  int num_ind_sub;
  RCHECK(reader.ReadBits(3, &num_ind_sub));

  int max_channel_count = 0;
  for (int i = 0; i < num_ind_sub + 1; i++) {
    // Please refer to ETSI TS 102 366 V1.4.1
    //    https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.03.01_60/ts_102366v010301p.pdf
    //    F.4 AC3SpecificBox
    //        fscod     2 bits
    //        bsid      5 bits
    //        reserved  1 bits
    //        asvc      1 bits
    //        bsmod     3 bits
    RCHECK(reader.SkipBits(2 + 5 + 1 + 1 + 3));

    int acmod;
    RCHECK(reader.ReadBits(3, &acmod));
    int lfeon;
    RCHECK(reader.ReadBits(1, &lfeon));

    max_channel_count =
        std::max(kGlobalChannelArray[acmod] + lfeon, max_channel_count);

    // skip reserved
    RCHECK(reader.SkipBits(3));

    int num_dep_sub;
    RCHECK(reader.ReadBits(4, &num_dep_sub));
    if (num_dep_sub > 0) {
      // skip chan_loc
      RCHECK(reader.SkipBits(9));
    } else {
      // skip reserved
      RCHECK(reader.SkipBits(1));
    }
  }
  channel_count_ = max_channel_count;
  RCHECK(channel_count_ >= 1 && channel_count_ <= limits::kMaxChannels);
  return true;
}

uint32_t EAC3::GetChannelCount() const {
  return channel_count_;
}

}  // namespace mp4
}  // namespace media
