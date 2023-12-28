// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/ac4.h"

#include <algorithm>
#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp4/rcheck.h"

namespace media {

namespace mp4 {

namespace {
// Refer to Table A.27: Speaker layouts and speaker indices in
// https://www.etsi.org/deliver/etsi_ts/103100_103199/10319002/01.02.01_60/ts_10319002v010201p.pdf
constexpr int kSpeakerIndicesMap[] = {
    2,  // (Group Index 00), Left (L) + Right (R)
    1,  // (Group Index 01), Centre (C)
    2,  // (Group Index 02), Left Surround (Ls) + Right Surround (Rs)
    2,  // (Group Index 03), Left Back (Lb)  + Right Back (Rb)
    2,  // (Group Index 04), Top Front Left (Tfl)  + Top Front Right (Tfr)
    2,  // (Group Index 05), Top Back Left (Tbl) + Top Back Right (Tbr)
    1,  // (Group Index 06), LFE
    2,  // (Group Index 07), Top Left (Tl) + Top Right (Tr)
    2,  // (Group Index 08), Top Side Left (Tsl) + Top Side Right (Tsr)
    1,  // (Group Index 09), Top Front Centre (Tfc)
    1,  // (Group Index 10), Tfc
    1,  // (Group Index 11), Top Centre (Tc)
    1,  // (Group Index 12), LFE2
    2,  // (Group Index 13), Bottom Front Left (Bfl) + Bottom Front Right(Bfr)
    1,  // (Group Index 14), Bottom Front  Centre(Bfc)
    1,  // (Group Index 15), Back Centre (Cb)
    2,  // (Group Index 16), Left Screen (Lscr) + Right Screen (Rscr)
    2,  // (Group Index 17), Left Wide (Lw) + Right Wide (Rw)
    2,  // (Group Index 18), Vertical Height Left(Vhl) + Vertical Height
        // Right(Vhr)
};

// Refer to E.11.7 dsi_substream_channel_mask in
// https://www.etsi.org/deliver/etsi_ts/103100_103199/10319002/01.02.01_60/ts_10319002v010201p.pdf
constexpr int kMaxSpeakerGroupIndex = std::size(kSpeakerIndicesMap);

int ChannelMaskToChannelCount(int mask) {
  int channels = 0;
  for (size_t i = 0; i < kMaxSpeakerGroupIndex; i++) {
    if ((mask >> i) & 0x1) {
      channels += kSpeakerIndicesMap[i];
    }
  }
  return channels;
}

bool ParseAc4BitrateDsi(BitReader& reader) {
  // Skip bit_rate_mode, bit_rate, bit_rate_precision, (2+32+32) bits
  RCHECK(reader.SkipBits(2 + 32 + 32));
  return true;
}

bool ParseAlternativeInfo(BitReader& reader) {
  int name_len;
  // name_len, 16 bits
  RCHECK(reader.ReadBits(16, &name_len));
  // Skip presentation_name * name_len, (8*name_len) bits
  RCHECK(reader.SkipBits(name_len * 8));

  int n_targets;
  // n_targets, 5 bits
  RCHECK(reader.ReadBits(5, &n_targets));
  // Skip n_targets * (target_md_compat, target_device_category),
  // (n_targets*(3+8)) bits
  RCHECK(reader.SkipBits(n_targets * (3 + 8)));
  return true;
}

bool Ac4ByteAlign(BitReader& reader) {
  auto bits = reader.bits_read() % 8;
  if (bits) {
    RCHECK(reader.SkipBits(8 - bits));
  }
  return true;
}

}  // namespace

AC4::AC4() = default;

AC4::AC4(const AC4& other) = default;

AC4::~AC4() = default;

bool AC4::ParseAc4DsiV1(BitReader& reader) {
  int bitstream_version;
  // bitstream_version, 7 bits
  RCHECK(reader.ReadBits(7, &bitstream_version));
  // Skip fs_index and frame_rate_index, (1+4) bits
  RCHECK(reader.SkipBits(1 + 4));
  int n_presentations;
  // n_presentations, 9 bits
  RCHECK(reader.ReadBits(9, &n_presentations));

  if (bitstream_version > 1) {
    int b_program_id;
    // b_program_id, 1 bit
    RCHECK(reader.ReadBits(1, &b_program_id));
    if (b_program_id) {
      // Skip short_program_id, 16 bits
      RCHECK(reader.SkipBits(16));
      int b_uuid;
      // b_uuid, 1 bit
      RCHECK(reader.ReadBits(1, &b_uuid));
      if (b_uuid) {
        // Skip program_uuid, 16*8 bits
        RCHECK(reader.SkipBits(16 * 8));
      }
    }
  }

  RCHECK(ParseAc4BitrateDsi(reader));
  RCHECK(Ac4ByteAlign(reader));

  for (int i = 0; i < n_presentations; i++) {
    int presentation_version;
    // presentation_version, 8 bits
    RCHECK(reader.ReadBits(8, &presentation_version));
    int pres_bytes;
    // pres_bytes, 8 bits
    RCHECK(reader.ReadBits(8, &pres_bytes));
    if (pres_bytes == 255) {
      int add_pres_bytes;
      // add_pres_bytes, 16 bits
      RCHECK(reader.ReadBits(16, &add_pres_bytes));
      pres_bytes += add_pres_bytes;
    }

    int consumed_pres_bytes = 0;
    if (presentation_version == 0) {
      // This case is not supported anymore.
      return false;
    } else if (presentation_version == 1 || presentation_version == 2) {
      RCHECK(ParseAc4PresentationV1Dsi(reader, pres_bytes, consumed_pres_bytes,
                                       bitstream_version,
                                       presentation_version));
    }

    int skip_bytes = pres_bytes - consumed_pres_bytes;
    RCHECK(skip_bytes >= 0);
    RCHECK(reader.SkipBits(skip_bytes * 8));
  }

  return true;
}

bool AC4::ParseAc4PresentationV1Dsi(BitReader& reader,
                                    int pres_bytes,
                                    int& consumed_pres_bytes,
                                    uint8_t bitstream_version,
                                    uint8_t presentation_version) {
  int bits_read = reader.bits_read();

  int presentation_config_v1;
  // presentation_config_v1, 5 bits
  RCHECK(reader.ReadBits(5, &presentation_config_v1));

  int b_add_emdf_substreams;
  if (presentation_config_v1 == 0x06) {
    b_add_emdf_substreams = 1;
  } else {
    int mdcompat;
    // mdcompat, 3 bits
    RCHECK(reader.ReadBits(3, &mdcompat));
    int b_presentation_id;
    // b_presentation_id, 1 bits
    RCHECK(reader.ReadBits(1, &b_presentation_id));
    if (b_presentation_id) {
      // Skip presentation_id, 5 bits
      RCHECK(reader.SkipBits(5));
    }
    // Skip dsi_frame_rate_multiply_info, dsi_frame_rate_fraction_info,
    // presentation_emdf_version, presentation_key_id, (2+2+5+10) bits
    RCHECK(reader.SkipBits(19));

    int b_presentation_channel_coded;
    // b_presentation_channel_coded, 1 bit
    RCHECK(reader.ReadBits(1, &b_presentation_channel_coded));
    if (b_presentation_channel_coded) {
      int dsi_presentation_ch_mode;
      // dsi_presentation_ch_mode, 5 bits
      RCHECK(reader.ReadBits(5, &dsi_presentation_ch_mode));
      if (dsi_presentation_ch_mode == 11 || dsi_presentation_ch_mode == 12 ||
          dsi_presentation_ch_mode == 13 || dsi_presentation_ch_mode == 14) {
        // Skip pres_b_4_back_channels_present, pres_top_channel_pairs, (1+2)
        // bits
        RCHECK(reader.SkipBits(3));
      }
      // Skip presentation_channel_mask_v1, 24 bits
      RCHECK(reader.SkipBits(24));
    }

    int b_presentation_core_differs;
    // b_presentation_core_differs, 1 bit
    RCHECK(reader.ReadBits(1, &b_presentation_core_differs));
    if (b_presentation_core_differs) {
      int b_presentation_core_channel_coded;
      // b_presentation_core_channel_coded, 1 bit
      RCHECK(reader.ReadBits(1, &b_presentation_core_channel_coded));
      if (b_presentation_core_channel_coded) {
        // Skip dsi_presentation_channel_mode_core, 2 bits
        RCHECK(reader.SkipBits(2));
      }
    }

    int b_presentation_filter;
    // b_presentation_filter, 1 bit
    RCHECK(reader.ReadBits(1, &b_presentation_filter));
    if (b_presentation_filter) {
      // Skip b_enable_presentation, 1 bit
      RCHECK(reader.SkipBits(1));
      int n_filter_bytes;
      // n_filter_bytes, 8 bits
      RCHECK(reader.ReadBits(8, &n_filter_bytes));
      // Skip filter_data, (n_filter_bytes * 8) bits
      RCHECK(reader.SkipBits(n_filter_bytes * 8));
    }

    int ac4_substream_groups = 0;
    if (presentation_config_v1 == 0x1f) {
      ac4_substream_groups = 1;
    } else {
      int b_multi_pid;
      // b_multi_pid, 1 bit
      RCHECK(reader.ReadBits(1, &b_multi_pid));

      int n_substream_groups_minus2;
      switch (presentation_config_v1) {
        case 0:
        case 1:
        case 2:
          ac4_substream_groups = 2;
          break;
        case 3:
        case 4:
          ac4_substream_groups = 3;
          break;
        case 5:
          // n_substream_groups_minus2, 3 bits
          RCHECK(reader.ReadBits(3, &n_substream_groups_minus2));
          ac4_substream_groups = n_substream_groups_minus2 + 2;
          break;
        default:
          int n_skip_bytes;
          // n_skip_bytes, 7 bits
          RCHECK(reader.ReadBits(7, &n_skip_bytes));
          // Skip skip_data, (n_skip_bytes * 8) bits
          RCHECK(reader.SkipBits(n_skip_bytes * 8));
          break;
      }
    }

    if (ac4_substream_groups) {
      for (int i = 0; i < ac4_substream_groups; i++) {
        RCHECK(ParseAc4SubstreamGroupDsi(reader, bitstream_version,
                                         presentation_version, mdcompat));
      }
    }

    // b_pre_virtualized, 1 bit
    RCHECK(reader.SkipBits(1));
    // b_add_emdf_substreams, 1 bit
    RCHECK(reader.ReadBits(1, &b_add_emdf_substreams));
  }

  if (b_add_emdf_substreams) {
    int n_add_emdf_substreams;
    // n_add_emdf_substreams, 7 bits
    RCHECK(reader.ReadBits(7, &n_add_emdf_substreams));
    // Skip substream_emdf_version, substream_key_id, (5+10) *
    // n_add_emdf_substreams
    RCHECK(reader.SkipBits(15 * n_add_emdf_substreams));
  }

  int b_presentation_bitrate_info;
  // b_presentation_bitrate_info, 1 bit
  RCHECK(reader.ReadBits(1, &b_presentation_bitrate_info));
  if (b_presentation_bitrate_info) {
    RCHECK(ParseAc4BitrateDsi(reader));
  }

  int b_alternative;
  // b_alternative, 1 bit
  RCHECK(reader.ReadBits(1, &b_alternative));
  if (b_alternative) {
    RCHECK(Ac4ByteAlign(reader));
    RCHECK(ParseAlternativeInfo(reader));
  }

  RCHECK(Ac4ByteAlign(reader));

  if ((reader.bits_read() - bits_read) <= (pres_bytes - 1) * 8) {
    // Skip de_indicator, 1 bit
    RCHECK(reader.SkipBits(1));
    // Skip dolby_atmos_indicator, 1 bit
    RCHECK(reader.SkipBits(1));
    // Skip reserved, 4 bits
    RCHECK(reader.SkipBits(4));
    int b_extended_presentation_id;
    // b_extended_presentation_id, 1 bit
    RCHECK(reader.ReadBits(1, &b_extended_presentation_id));
    if (b_extended_presentation_id) {
      // extended_presentation_id, 9 bits
      RCHECK(reader.SkipBits(9));
    } else {
      // Skip reserved, 1 bit
      RCHECK(reader.SkipBits(1));
    }
  }

  consumed_pres_bytes = (reader.bits_read() - bits_read) / 8;
  return true;
}

bool AC4::ParseAc4SubstreamGroupDsi(BitReader& reader,
                                    uint8_t bitstream_version,
                                    uint8_t presentation_version,
                                    uint8_t presentation_level) {
  AC4StreamInfo stream_info;
  stream_info.bitstream_version = bitstream_version;
  stream_info.presentation_version = presentation_version;
  stream_info.presentation_level = presentation_level;
  // Refer to below page, presentation_version will be 02 when streaming IMS:
  // https://ott.dolby.com/OnDelKits/AC-4/Dolby_AC-4_Online_Delivery_Kit_1.5/Documentation/Specs/AC4_DASH/help_files/topics/ac4_dash_c_signal_IMS.html
  stream_info.is_ims = presentation_version == 0x02;
  stream_info.is_ajoc = 0;
  stream_info.channels = 0;

  int b_substreams_present;
  // b_substreams_present, 1 bit
  RCHECK(reader.ReadBits(1, &b_substreams_present));
  // Skip b_hsf_ext, 1 bit
  RCHECK(reader.SkipBits(1));
  int b_channel_coded;
  // b_channel_coded, 1 bit
  RCHECK(reader.ReadBits(1, &b_channel_coded));
  int n_substreams;
  // n_substreams, 8 bits
  RCHECK(reader.ReadBits(8, &n_substreams));
  for (int i = 0; i < n_substreams; i++) {
    // Skip dsi_sf_multiplier, 2 bits
    RCHECK(reader.SkipBits(2));
    int b_substream_bitrate_indicator;
    // b_substream_bitrate_indicator, 1 bit
    RCHECK(reader.ReadBits(1, &b_substream_bitrate_indicator));
    if (b_substream_bitrate_indicator) {
      // Skip substream_bitrate_indicator, 5 bits
      RCHECK(reader.SkipBits(5));
    }
    if (b_channel_coded) {
      // dsi_substream_channel_mask, 24 bits
      int dsi_substream_channel_mask = 0;
      RCHECK(reader.ReadBits(24, &dsi_substream_channel_mask));
      stream_info.channels =
          ChannelMaskToChannelCount(dsi_substream_channel_mask);
    } else {
      int b_ajoc;
      // b_ajoc, 1 bit
      RCHECK(reader.ReadBits(1, &b_ajoc));
      stream_info.is_ajoc = b_ajoc;

      if (b_ajoc) {
        int b_static_dmx;
        // b_static_dmx, 1 bit
        RCHECK(reader.ReadBits(1, &b_static_dmx));
        if (b_static_dmx == 0) {
          // Skip n_dmx_objects_minus1, 4 bits
          RCHECK(reader.SkipBits(4));
        }
        // Skip n_umx_objects_minus1, 6 bits
        RCHECK(reader.SkipBits(6));
      }

      // Skip b_substream_contains_bed_objects,
      // b_substream_contains_dynamic_objects,
      // b_substream_contains_ISF_objects,reserved (1+1+1+1) bits
      RCHECK(reader.SkipBits(4));
    }
    stream_info_internals_.push_back(stream_info);
  }

  int b_content_type;
  // b_content_type, 1 bit
  RCHECK(reader.ReadBits(1, &b_content_type));
  if (b_content_type) {
    // Skip content_classifier, 3 bits
    RCHECK(reader.SkipBits(3));
    int b_language_indicator;
    // b_language_indicator, 1 bit
    RCHECK(reader.ReadBits(1, &b_language_indicator));
    if (b_language_indicator) {
      int n_language_tag_bytes;
      // n_language_tag_bytes, 6 bits
      RCHECK(reader.ReadBits(6, &n_language_tag_bytes));
      // Skip language_tag_bytes * n_language_tag_bytes, (8 *
      // n_language_tag_bytes) bits
      RCHECK(reader.SkipBits(n_language_tag_bytes * 8));
    }
  }

  return true;
}

bool AC4::Parse(const std::vector<uint8_t>& data, MediaLog* media_log) {
  if (data.empty()) {
    return false;
  }

  BitReader reader(&data[0], data.size());

  int ac4_dsi_version = 0;
  // ac4_dsi_version, 3 bits
  RCHECK(reader.ReadBits(3, &ac4_dsi_version));
  // Only support DSI version 1 here since version 0 is not used in practice.
  if (ac4_dsi_version == 1) {
    RCHECK(ParseAc4DsiV1(reader));
  } else {
    return false;
  }

  if (stream_info_internals_.size() > 0) {
    LogAC4Parameters();
    return true;
  }
  return false;
}

std::vector<uint8_t> AC4::StreamInfo() const {
  DCHECK(stream_info_internals_.size() > 0);
  return {stream_info_internals_[0].bitstream_version,
          stream_info_internals_[0].presentation_version,
          stream_info_internals_[0].presentation_level,
          stream_info_internals_[0].is_ajoc,
          stream_info_internals_[0].is_ims,
          stream_info_internals_[0].channels};
}

void AC4::LogAC4Parameters() {
  DVLOG(3) << "parsed AC4 sub-stream count: " << stream_info_internals_.size()
           << ", first sub-stream is selected: " << "ac4_bitstream_version "
           << stream_info_internals_[0].bitstream_version
           << ", ac4_presentation_version "
           << stream_info_internals_[0].presentation_version
           << ", ac4_presentation_level "
           << stream_info_internals_[0].presentation_level;
}

}  // namespace mp4

}  // namespace media
