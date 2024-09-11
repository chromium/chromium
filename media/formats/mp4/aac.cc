// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/aac.h"

#include <stddef.h>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp4/rcheck.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media::mp4 {

AAC::AAC()
    : profile_(0),
      frequency_index_(0),
      channel_config_(0),
      frequency_(0),
      extension_frequency_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED) {}

AAC::AAC(const AAC& other) = default;

AAC::~AAC() = default;

bool AAC::Parse(base::span<const uint8_t> data, MediaLog* media_log) {
  codec_specific_data_.assign(data.begin(), data.end());

  if (data.empty()) {
    return false;
  }

  BitReader reader(data.data(), data.size());
  uint8_t extension_type = 0;
  bool ps_present = false;
  uint8_t extension_frequency_index = 0xff;

  frequency_ = 0;
  extension_frequency_ = 0;

  // Parsing below is a partial implementation of ISO 14496-3:2009 that covers
  // profiles in range of [1, 4] as well as SBR (5) and PS (29) extensions.

  // Read base configuration
  RCHECK(reader.ReadBits(5, &profile_));

  // Signals that we need more than 5 bits for profile.
  if (profile_ == 31) {
    uint8_t tmp;
    RCHECK(reader.ReadBits(6, &tmp));
    profile_ = 32 + tmp;
  }

  RCHECK(reader.ReadBits(4, &frequency_index_));
  if (frequency_index_ == 0xf) {
    RCHECK(reader.ReadBits(24, &frequency_));
  }
  RCHECK(reader.ReadBits(4, &channel_config_));

  // Read extension configuration for explicitly signaled HE-AAC profiles
  // 5 = HEv1 (Spectral Band Replication), 29 = HEv2 (Parametric Stereo).
  if (profile_ == 5 || profile_ == 29) {
    ps_present = (profile_ == 29);
    extension_type = 5;
    RCHECK(reader.ReadBits(4, &extension_frequency_index));
    if (extension_frequency_index == 0xf) {
      RCHECK(reader.ReadBits(24, &extension_frequency_));
    }
    // With HE extensions now known, determine underlying profile.
    RCHECK(reader.ReadBits(5, &profile_));
  }

  // Parsing not implemented for profiles outside this range, so error out. Note
  // that values of 5 (HE-AACv1) and 29 (HE-AACv2) are parsed above and the
  // value of profile_ must now reflect the the underlying profile being used
  // with those extensions (these extensions are supported).
  // 1 = AAC main, 2 = AAC LC, 3 = AAC SSR, 4 = AAC LTP
  if ((profile_ < 1 || profile_ > 4) && profile_ != kXHeAAcType) {
    MEDIA_LOG(ERROR, media_log)
        << "Audio codec(mp4a.40." << static_cast<int>(profile_)
        << ") is not supported.";
    return false;
  }

  RCHECK(SkipDecoderGASpecificConfig(&reader));

  // Read extension configuration again
  // Note: The check for 16 available bits comes from the AAC spec.
  if (extension_type != 5 && reader.bits_available() >= 16) {
    uint16_t sync_extension_type;
    uint8_t sbr_present_flag;
    uint8_t ps_present_flag;

    if (reader.ReadBits(11, &sync_extension_type) &&
        sync_extension_type == 0x2b7) {
      if (reader.ReadBits(5, &extension_type) && extension_type == 5) {
        RCHECK(reader.ReadBits(1, &sbr_present_flag));

        if (sbr_present_flag) {
          RCHECK(reader.ReadBits(4, &extension_frequency_index));

          if (extension_frequency_index == 0xf) {
            RCHECK(reader.ReadBits(24, &extension_frequency_));
          }

          // Note: The check for 12 available bits comes from the AAC spec.
          if (reader.bits_available() >= 12) {
            RCHECK(reader.ReadBits(11, &sync_extension_type));
            if (sync_extension_type == 0x548) {
              RCHECK(reader.ReadBits(1, &ps_present_flag));
              ps_present = ps_present_flag != 0;
            }
          }
        }
      }
    }
  }

  if (frequency_ == 0) {
    if (frequency_index_ >= kADTSFrequencyTableSize) {
      MEDIA_LOG(ERROR, media_log)
          << "Sampling Frequency Index(0x" << std::hex
          << static_cast<int>(frequency_index_)
          << ") is not supported. Please see ISO 14496-3:2009 Table 1.18 "
          << "for supported Sampling Frequencies.";
      return false;
    }
    frequency_ = kADTSFrequencyTable[frequency_index_];
  }

  if (extension_frequency_ == 0 && extension_frequency_index != 0xff) {
    if (extension_frequency_index >= kADTSFrequencyTableSize) {
      MEDIA_LOG(ERROR, media_log)
          << "Extension Sampling Frequency Index(0x" << std::hex
          << static_cast<int>(extension_frequency_index)
          << ") is not supported. Please see ISO 14496-3:2009 Table 1.18 "
          << "for supported Sampling Frequencies.";
      return false;
    }
    extension_frequency_ = kADTSFrequencyTable[extension_frequency_index];
  }

  // When Parametric Stereo is on, mono will be played as stereo.
  if (ps_present && channel_config_ == 1) {
    channel_layout_ = CHANNEL_LAYOUT_STEREO;
  } else {
    if (channel_config_ >= kADTSChannelLayoutTableSize) {
      MEDIA_LOG(ERROR, media_log)
          << "Channel Configuration(" << static_cast<int>(channel_config_)
          << ") is not supported. Please see ISO 14496-3:2009 Table 1.19 "
          << "for supported Channel Configurations.";
      return false;
    }
    channel_layout_ = kADTSChannelLayoutTable[channel_config_];
  }
  DCHECK(channel_layout_ != CHANNEL_LAYOUT_NONE);

  return true;
}

int AAC::GetOutputSamplesPerSecond(bool sbr_in_mimetype) const {
  if (extension_frequency_ > 0) {
    return extension_frequency_;
  }

  if (!sbr_in_mimetype) {
    return frequency_;
  }

  // The following code is written according to ISO 14496-3:2009 Table 1.11 and
  // Table 1.25. (Table 1.11 refers to the capping to 48000, Table 1.25 refers
  // to SBR doubling the AAC sample rate.)
  // TODO(acolwell) : Extend sample rate cap to 96kHz for Level 5 content.
  DCHECK_GT(frequency_, 0);
  return std::min(2 * frequency_, 48000);
}

ChannelLayout AAC::GetChannelLayout(bool sbr_in_mimetype) const {
  // Check for implicit signalling of HE-AAC and indicate stereo output
  // if the mono channel configuration is signalled.
  // See ISO 14496-3:2009 Section 1.6.5.3 for details about this special casing.
  if (sbr_in_mimetype && channel_config_ == 1) {
    return CHANNEL_LAYOUT_STEREO;
  }

  return channel_layout_;
}

base::HeapArray<uint8_t> AAC::CreateAdtsFromEsds(
    base::span<const uint8_t> buffer,
    int* adts_header_size) const {
  *adts_header_size = 0;
  if (profile_ == kXHeAAcType) {
    return {};
  }

  DCHECK(profile_ >= 1 && profile_ <= 4 && frequency_index_ != 0xf &&
         channel_config_ <= 7);

  // `total_size` might be too big; ADTS represents packet size in 13 bits.
  const size_t total_size = buffer.size() + kADTSHeaderMinSize;
  if (total_size >= (1 << 13)) {
    return base::HeapArray<uint8_t>();
  }

  auto output_buffer = base::HeapArray<uint8_t>::Uninit(total_size);
  SetAdtsHeader(output_buffer.first(kADTSHeaderMinSize), total_size);
  output_buffer.last(buffer.size()).copy_from(buffer);
  *adts_header_size = kADTSHeaderMinSize;
  return output_buffer;
}

void AAC::SetAdtsHeader(base::span<uint8_t> adts, size_t total_size) const {
  CHECK_NE(profile_, kXHeAAcType);
  CHECK_EQ(adts.size(), kADTSHeaderMinSize);

  adts[0] = 0xff;
  adts[1] = 0xf1;
  adts[2] =
      ((profile_ - 1) << 6) + (frequency_index_ << 2) + (channel_config_ >> 2);
  adts[3] =
      static_cast<uint8_t>(((channel_config_ & 0x3) << 6) + (total_size >> 11));
  adts[4] = static_cast<uint8_t>((total_size & 0x7ff) >> 3);
  adts[5] = ((total_size & 7) << 5) + 0x1f;
  adts[6] = 0xfc;
}

AudioCodecProfile AAC::GetProfile() const {
  return profile_ == kXHeAAcType ? AudioCodecProfile::kXHE_AAC
                                 : AudioCodecProfile::kUnknown;
}

// Currently this function only support GASpecificConfig defined in
// ISO 14496-3:2009 Table 4.1 - Syntax of GASpecificConfig()
bool AAC::SkipDecoderGASpecificConfig(BitReader* bit_reader) const {
  switch (profile_) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
    case 17:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case kXHeAAcType:
      return SkipGASpecificConfig(bit_reader);
    default:
      break;
  }

  return false;
}

// The following code is written according to ISO 14496-3:2009 Table 4.1 -
// GASpecificConfig.
bool AAC::SkipGASpecificConfig(BitReader* bit_reader) const {
  uint8_t extension_flag = 0;
  uint8_t depends_on_core_coder;
  uint16_t dummy;

  RCHECK(bit_reader->ReadBits(1, &dummy));  // frameLengthFlag
  RCHECK(bit_reader->ReadBits(1, &depends_on_core_coder));
  if (depends_on_core_coder == 1) {
    RCHECK(bit_reader->ReadBits(14, &dummy));  // coreCoderDelay
  }

  RCHECK(bit_reader->ReadBits(1, &extension_flag));
  RCHECK(channel_config_ != 0);

  if (profile_ == 6 || profile_ == 20) {
    RCHECK(bit_reader->ReadBits(3, &dummy));  // layerNr
  }

  if (extension_flag) {
    if (profile_ == 22) {
      RCHECK(bit_reader->ReadBits(5, &dummy));   // numOfSubFrame
      RCHECK(bit_reader->ReadBits(11, &dummy));  // layer_length
    }

    if (profile_ == 17 || profile_ == 19 || profile_ == 20 || profile_ == 23) {
      RCHECK(bit_reader->ReadBits(3, &dummy));  // resilience flags
    }

    RCHECK(bit_reader->ReadBits(1, &dummy));  // extensionFlag3
  }

  return true;
}

}  // namespace media::mp4
