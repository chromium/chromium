// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/eac3.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/formats/mp4/ac3_constants.h"
#include "media/formats/mp4/rcheck.h"

namespace media {

namespace mp4 {

EAC3::EAC3() = default;

EAC3::EAC3(const EAC3& other) = default;

EAC3::~EAC3() = default;

bool EAC3::Parse(const std::vector<uint8_t>& data, MediaLog* media_log) {
  if (data.empty()) {
    return false;
  }

  // For EC3SpecificBox, please refer to ETSI TS 102 366 V1.4.1
  //    https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.03.01_60/ts_102366v010301p.pdf
  //    F.6 EC3SpecificBox
  //        data_rate                                   13 bits
  //        num_ind_sub                                 3 bits
  //        {
  //            fscod                                   2 bits
  //            bsid                                    5 bits
  //            reserved                                1 bits
  //            asvc                                    1 bits
  //            bsmod                                   3 bits
  //            acmod                                   3 bits
  //            lfeon                                   1 bits
  //            reserved                                3 bits
  //            num_dep_sub                             4 bits
  //            if num_dep_sub > 0 chan_loc             9 bits
  //            else reserved                           1 bits
  //        }
  //        reserved                             variable bits

  // At least one independent substreams exist without dependent substream.
  if (data.size() * 8 < (13 + 3 + (2 + 5 + 1 + 1 + 3 + 3 + 1 + 3 + 4 + 1))) {
    return false;
  }

  // Parse dec3 box using reader.
  BitReader reader(&data[0], data.size());

  // skip data_rate
  RCHECK(reader.SkipBits(13));

  int num_ind_sub;
  RCHECK(reader.ReadBits(3, &num_ind_sub));

  ChannelLayout max_channel_layout = CHANNEL_LAYOUT_UNSUPPORTED;
  uint32_t max_channel_count = 0;
  for (int i = 0; i < num_ind_sub + 1; i++) {
    // skip fscod, bsid, reserved, asvc, bsmod
    RCHECK(reader.SkipBits(2 + 5 + 1 + 1 + 3));

    int acmod;
    RCHECK(reader.ReadBits(3, &acmod));
    if (acmod >= kAC3AudioCodingModeSize) {
      return false;
    }

    int lfeon;
    RCHECK(reader.ReadBits(1, &lfeon));

    ChannelLayout channel_layout = kAC3AudioCodingModeTable[lfeon][acmod];
    uint32_t channel_count = ChannelLayoutToChannelCount(channel_layout);

    // always use channel layout with the largest number of channels when
    // possible.
    if (channel_count > max_channel_count) {
      max_channel_layout = channel_layout;
      max_channel_count = channel_count;
    }

    // skip reserved
    RCHECK(reader.SkipBits(3));

    int num_dep_sub;
    RCHECK(reader.ReadBits(4, &num_dep_sub));
    if (num_dep_sub == 0) {
      // skip reserved
      RCHECK(reader.SkipBits(1));
      continue;
    }

    int chan_loc;
    RCHECK(reader.ReadBits(9, &chan_loc));
    if (chan_loc == 0) {
      // skip since no additional channel location info.
      continue;
    }

    // acmod should always be the largest value if chan_loc > 0.
    if (acmod != kAC3AudioCodingModeSize - 1) {
      return false;
    }

    if (chan_loc == EC3DependentSubstreamChannelLocation::LRS_RRS) {
      channel_layout = lfeon == 0 ? CHANNEL_LAYOUT_7_0 : CHANNEL_LAYOUT_7_1;
    } else if (chan_loc == EC3DependentSubstreamChannelLocation::LC_RC) {
      channel_layout =
          lfeon == 0 ? CHANNEL_LAYOUT_7_0_FRONT : CHANNEL_LAYOUT_7_1_WIDE;
    } else if (chan_loc == EC3DependentSubstreamChannelLocation::CS) {
      channel_layout = lfeon == 0 ? CHANNEL_LAYOUT_6_0 : CHANNEL_LAYOUT_6_1;
    } else {
      // otherwise chan_loc has unsupported channel locations, we ignore these
      // label if we don't support them, and considers streams to only contain
      // independent streams.
    }
    channel_count = ChannelLayoutToChannelCount(channel_layout);

    // always use channel layout with the largest number of channels when
    // possible.
    if (channel_count > max_channel_count) {
      max_channel_layout = channel_layout;
      max_channel_count = channel_count;
    }
  }

  channel_layout_ = max_channel_layout;
  RCHECK(channel_layout_ > CHANNEL_LAYOUT_UNSUPPORTED);
  channel_count_ = max_channel_count;
  RCHECK(channel_count_ >= 1 && channel_count_ <= limits::kMaxChannels);

  return true;
}

uint32_t EAC3::GetChannelCount() const {
  return channel_count_;
}

ChannelLayout EAC3::GetChannelLayout() const {
  return channel_layout_;
}

}  // namespace mp4
}  // namespace media
