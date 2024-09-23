// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP8 raw stream parser,
// as defined in RFC 6386.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/vp8_parser.h"

#include <cstring>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace media {

#define ERROR_RETURN(what)                           \
  do {                                               \
    DVLOG(1) << "Error while trying to read " #what; \
    return false;                                    \
  } while (0)

#define BD_READ_BOOL_OR_RETURN(out) \
  do {                              \
    if (!bd_.ReadBool(out))         \
      ERROR_RETURN(out);            \
  } while (0)

#define BD_READ_BOOL_WITH_PROB_OR_RETURN(out, prob) \
  do {                                              \
    if (!bd_.ReadBool(out, prob))                   \
      ERROR_RETURN(out);                            \
  } while (0)

#define BD_READ_UNSIGNED_OR_RETURN(num_bits, out) \
  do {                                            \
    int _out;                                     \
    if (!bd_.ReadLiteral(num_bits, &_out))        \
      ERROR_RETURN(out);                          \
    *out = _out;                                  \
  } while (0)

#define BD_READ_SIGNED_OR_RETURN(num_bits, out)    \
  do {                                             \
    int _out;                                      \
    if (!bd_.ReadLiteralWithSign(num_bits, &_out)) \
      ERROR_RETURN(out);                           \
    *out = _out;                                   \
  } while (0)

Vp8FrameHeader::Vp8FrameHeader() = default;

Vp8FrameHeader::~Vp8FrameHeader() = default;

Vp8FrameHeader::Vp8FrameHeader(const Vp8FrameHeader& fhdr) = default;

Vp8FrameHeader& Vp8FrameHeader::operator=(const Vp8FrameHeader& fhdr) = default;

Vp8Parser::Vp8Parser() : stream_(nullptr), bytes_left_(0) {
}

Vp8Parser::~Vp8Parser() = default;

bool Vp8Parser::ParseFrame(const uint8_t* ptr,
                           size_t frame_size,
                           Vp8FrameHeader* fhdr) {
  stream_ = ptr;
  bytes_left_ = frame_size;

  *fhdr = Vp8FrameHeader();
  fhdr->data = stream_;
  fhdr->frame_size = bytes_left_;

  if (!ParseFrameTag(fhdr))
    return false;

  fhdr->first_part_offset = stream_ - fhdr->data;

  if (!ParseFrameHeader(fhdr))
    return false;

  if (!ParsePartitions(fhdr))
    return false;

  DVLOG(4) << "Frame parsed, start: " << static_cast<const void*>(ptr)
           << ", size: " << frame_size
           << ", offsets: to first_part=" << fhdr->first_part_offset
           << ", to macroblock data (in bits)=" << fhdr->macroblock_bit_offset;

  return true;
}

static inline uint32_t GetBitsAt(uint32_t data, size_t shift, size_t num_bits) {
  return ((data >> shift) & ((1 << num_bits) - 1));
}

bool Vp8Parser::ParseFrameTag(Vp8FrameHeader* fhdr) {
  const size_t kFrameTagSize = 3;
  if (bytes_left_ < kFrameTagSize)
    return false;

  uint32_t frame_tag = (stream_[2] << 16) | (stream_[1] << 8) | stream_[0];
  fhdr->frame_type =
      static_cast<Vp8FrameHeader::FrameType>(GetBitsAt(frame_tag, 0, 1));
  fhdr->version = GetBitsAt(frame_tag, 1, 2);
  fhdr->is_experimental = !!GetBitsAt(frame_tag, 3, 1);
  fhdr->show_frame =!!GetBitsAt(frame_tag, 4, 1);
  fhdr->first_part_size = GetBitsAt(frame_tag, 5, 19);

  stream_ += kFrameTagSize;
  bytes_left_ -= kFrameTagSize;

  if (fhdr->IsKeyframe()) {
    const size_t kKeyframeTagSize = 7;
    if (bytes_left_ < kKeyframeTagSize)
      return false;

    static const uint8_t kVp8StartCode[] = {0x9d, 0x01, 0x2a};
    if (memcmp(stream_, kVp8StartCode, sizeof(kVp8StartCode)) != 0)
        return false;

    stream_ += sizeof(kVp8StartCode);
    bytes_left_ -= sizeof(kVp8StartCode);

    uint16_t data = (stream_[1] << 8) | stream_[0];
    fhdr->width = data & 0x3fff;
    fhdr->horizontal_scale = data >> 14;

    data = (stream_[3] << 8) | stream_[2];
    fhdr->height = data & 0x3fff;
    fhdr->vertical_scale = data >> 14;

    stream_ += 4;
    bytes_left_ -= 4;
  }

  return true;
}

bool Vp8Parser::ParseFrameHeader(Vp8FrameHeader* fhdr) {
  if (!bd_.Initialize(stream_, bytes_left_))
    return false;

  bool keyframe = fhdr->IsKeyframe();
  if (keyframe) {
    unsigned int data;
    BD_READ_UNSIGNED_OR_RETURN(1, &data);  // color_space
    BD_READ_UNSIGNED_OR_RETURN(1, &data);  // clamping_type
    fhdr->is_full_range = data == 1;
  } else {
    fhdr->is_full_range = false;
  }

  if (!ParseSegmentationHeader(keyframe))
    return false;

  fhdr->segmentation_hdr = curr_segmentation_hdr_;

  if (!ParseLoopFilterHeader(keyframe))
    return false;

  fhdr->loopfilter_hdr = curr_loopfilter_hdr_;

  int log2_nbr_of_dct_partitions;
  BD_READ_UNSIGNED_OR_RETURN(2, &log2_nbr_of_dct_partitions);
  fhdr->num_of_dct_partitions = static_cast<size_t>(1)
                                << log2_nbr_of_dct_partitions;

  if (!ParseQuantizationHeader(&fhdr->quantization_hdr))
    return false;

  if (keyframe) {
    BD_READ_BOOL_OR_RETURN(&fhdr->refresh_entropy_probs);
  } else {
    BD_READ_BOOL_OR_RETURN(&fhdr->refresh_golden_frame);
    BD_READ_BOOL_OR_RETURN(&fhdr->refresh_alternate_frame);

    int refresh_mode;
    if (!fhdr->refresh_golden_frame) {
      BD_READ_UNSIGNED_OR_RETURN(2, &refresh_mode);
      fhdr->copy_buffer_to_golden =
          static_cast<Vp8FrameHeader::GoldenRefreshMode>(refresh_mode);
    }

    if (!fhdr->refresh_alternate_frame) {
      BD_READ_UNSIGNED_OR_RETURN(2, &refresh_mode);
      fhdr->copy_buffer_to_alternate =
          static_cast<Vp8FrameHeader::AltRefreshMode>(refresh_mode);
    }

    BD_READ_UNSIGNED_OR_RETURN(1, &fhdr->sign_bias_golden);
    BD_READ_UNSIGNED_OR_RETURN(1, &fhdr->sign_bias_alternate);
    BD_READ_BOOL_OR_RETURN(&fhdr->refresh_entropy_probs);
    BD_READ_BOOL_OR_RETURN(&fhdr->refresh_last);
  }

  if (keyframe)
    ResetProbs();

  fhdr->entropy_hdr = curr_entropy_hdr_;

  if (!ParseTokenProbs(&fhdr->entropy_hdr, fhdr->refresh_entropy_probs))
    return false;

  BD_READ_BOOL_OR_RETURN(&fhdr->mb_no_skip_coeff);
  if (fhdr->mb_no_skip_coeff)
    BD_READ_UNSIGNED_OR_RETURN(8, &fhdr->prob_skip_false);

  if (!keyframe) {
    BD_READ_UNSIGNED_OR_RETURN(8, &fhdr->prob_intra);
    BD_READ_UNSIGNED_OR_RETURN(8, &fhdr->prob_last);
    BD_READ_UNSIGNED_OR_RETURN(8, &fhdr->prob_gf);
  }

  if (!ParseIntraProbs(&fhdr->entropy_hdr, fhdr->refresh_entropy_probs,
                       keyframe))
    return false;

  if (!keyframe) {
    if (!ParseMVProbs(&fhdr->entropy_hdr, fhdr->refresh_entropy_probs))
      return false;
  }

  fhdr->macroblock_bit_offset = bd_.BitOffset();
  fhdr->bool_dec_range = bd_.GetRange();
  fhdr->bool_dec_value = bd_.GetBottom();
  fhdr->bool_dec_count = 7 - (bd_.BitOffset() + 7) % 8;

  return true;
}

bool Vp8Parser::ParseSegmentationHeader(bool keyframe) {
  Vp8SegmentationHeader* shdr = &curr_segmentation_hdr_;

  if (keyframe)
    memset(shdr, 0, sizeof(*shdr));

  BD_READ_BOOL_OR_RETURN(&shdr->segmentation_enabled);
  if (!shdr->segmentation_enabled)
    return true;

  BD_READ_BOOL_OR_RETURN(&shdr->update_mb_segmentation_map);
  BD_READ_BOOL_OR_RETURN(&shdr->update_segment_feature_data);
  if (shdr->update_segment_feature_data) {
    int mode;
    BD_READ_UNSIGNED_OR_RETURN(1, &mode);
    shdr->segment_feature_mode =
        static_cast<Vp8SegmentationHeader::SegmentFeatureMode>(mode);

    for (size_t i = 0; i < kMaxMBSegments; ++i) {
      bool quantizer_update;
      BD_READ_BOOL_OR_RETURN(&quantizer_update);
      if (quantizer_update)
        BD_READ_SIGNED_OR_RETURN(7, &shdr->quantizer_update_value[i]);
      else
        shdr->quantizer_update_value[i] = 0;
    }

    for (size_t i = 0; i < kMaxMBSegments; ++i) {
      bool loop_filter_update;
      BD_READ_BOOL_OR_RETURN(&loop_filter_update);
      if (loop_filter_update)
        BD_READ_SIGNED_OR_RETURN(6, &shdr->lf_update_value[i]);
      else
        shdr->lf_update_value[i] = 0;
    }
  }

  if (shdr->update_mb_segmentation_map) {
    for (size_t i = 0; i < kNumMBFeatureTreeProbs; ++i) {
      bool segment_prob_update;
      BD_READ_BOOL_OR_RETURN(&segment_prob_update);
      if (segment_prob_update)
        BD_READ_UNSIGNED_OR_RETURN(8, &shdr->segment_prob[i]);
      else
        shdr->segment_prob[i] = Vp8SegmentationHeader::kDefaultSegmentProb;
    }
  }

  return true;
}

bool Vp8Parser::ParseLoopFilterHeader(bool keyframe) {
  Vp8LoopFilterHeader* lfhdr = &curr_loopfilter_hdr_;

  if (keyframe)
    memset(lfhdr, 0, sizeof(*lfhdr));

  int type;
  BD_READ_UNSIGNED_OR_RETURN(1, &type);
  lfhdr->type = static_cast<Vp8LoopFilterHeader::Type>(type);
  BD_READ_UNSIGNED_OR_RETURN(6, &lfhdr->level);
  BD_READ_UNSIGNED_OR_RETURN(3, &lfhdr->sharpness_level);
  BD_READ_BOOL_OR_RETURN(&lfhdr->loop_filter_adj_enable);

  if (lfhdr->loop_filter_adj_enable) {
    BD_READ_BOOL_OR_RETURN(&lfhdr->mode_ref_lf_delta_update);
    if (lfhdr->mode_ref_lf_delta_update) {
      for (size_t i = 0; i < kNumBlockContexts; ++i) {
        bool ref_frame_delta_update_flag;
        BD_READ_BOOL_OR_RETURN(&ref_frame_delta_update_flag);
        if (ref_frame_delta_update_flag)
          BD_READ_SIGNED_OR_RETURN(6, &lfhdr->ref_frame_delta[i]);
      }

      for (size_t i = 0; i < kNumBlockContexts; ++i) {
        bool mb_mode_delta_update_flag;
        BD_READ_BOOL_OR_RETURN(&mb_mode_delta_update_flag);
        if (mb_mode_delta_update_flag)
          BD_READ_SIGNED_OR_RETURN(6, &lfhdr->mb_mode_delta[i]);
      }
    }
  }

  return true;
}

bool Vp8Parser::ParseQuantizationHeader(Vp8QuantizationHeader* qhdr) {
  // If any of the delta values is not present, the delta should be zero.
  memset(qhdr, 0, sizeof(*qhdr));

  BD_READ_UNSIGNED_OR_RETURN(7, &qhdr->y_ac_qi);

  bool delta_present;

  BD_READ_BOOL_OR_RETURN(&delta_present);
  if (delta_present)
    BD_READ_SIGNED_OR_RETURN(4, &qhdr->y_dc_delta);

  BD_READ_BOOL_OR_RETURN(&delta_present);
  if (delta_present)
    BD_READ_SIGNED_OR_RETURN(4, &qhdr->y2_dc_delta);

  BD_READ_BOOL_OR_RETURN(&delta_present);
  if (delta_present)
    BD_READ_SIGNED_OR_RETURN(4, &qhdr->y2_ac_delta);

  BD_READ_BOOL_OR_RETURN(&delta_present);
  if (delta_present)
    BD_READ_SIGNED_OR_RETURN(4, &qhdr->uv_dc_delta);

  BD_READ_BOOL_OR_RETURN(&delta_present);
  if (delta_present)
    BD_READ_SIGNED_OR_RETURN(4, &qhdr->uv_ac_delta);

  return true;
}

// See spec for details on these values.
const uint8_t kCoeffUpdateProbs[kNumBlockTypes][kNumCoeffBands]
    [kNumPrevCoeffContexts][kNumEntropyNodes] = {
  {
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255},
      {249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255},
      {234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255},
      {250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255},
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
  },
  {
    {
      {217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255},
      {234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255},
    },
    {
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
  },
  {
    {
      {186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255},
      {234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255},
      {251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255},
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
  },
  {
    {
      {248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255},
      {248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      {246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      {252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255},
      {248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      {253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      {252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
    },
  },
};

const uint8_t kKeyframeYModeProbs[kNumYModeProbs] = {145, 156, 163, 128};
const uint8_t kKeyframeUVModeProbs[kNumUVModeProbs] = {142, 114, 183};

const uint8_t kDefaultYModeProbs[kNumYModeProbs] = {112, 86, 140, 37};
const uint8_t kDefaultUVModeProbs[kNumUVModeProbs] = {162, 101, 204};

const uint8_t kDefaultCoeffProbs[kNumBlockTypes][kNumCoeffBands]
    [kNumPrevCoeffContexts][kNumEntropyNodes] = {
  {
    {
      {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
    },
    {
      {253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
      {189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
      {106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128},
    },
    {
      {  1,  98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
      {181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
      { 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128},
    },
    {
      {  1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
      {184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
      { 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128},
    },
    {
      {  1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
      {170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
      { 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128},
    },
    {
      {  1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
      {207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
      {102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128},
    },
    {
      {  1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
      {177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
      { 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128},
    },
    {
      {  1,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      {246,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      {255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
    }
  },
  {
    {
      {198,  35, 237, 223, 193, 187, 162, 160, 145, 155,  62},
      {131,  45, 198, 221, 172, 176, 220, 157, 252, 221,   1},
      { 68,  47, 146, 208, 149, 167, 221, 162, 255, 223, 128},
    },
    {
      {  1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
      {184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
      { 81,  99, 181, 242, 176, 190, 249, 202, 255, 255, 128},
    },
    {
      {  1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
      { 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
      { 23,  91, 163, 242, 170, 187, 247, 210, 255, 255, 128},
    },
    {
      {  1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
      {109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
      { 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128},
    },
    {
      {  1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
      { 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
      { 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128},
    },
    {
      {  1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
      {124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
      { 35,  77, 181, 251, 193, 211, 255, 205, 128, 128, 128},
    },
    {
      {  1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
      {121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
      { 45,  99, 188, 251, 195, 217, 255, 224, 128, 128, 128},
    },
    {
      {  1,   1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
      {203,   1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
      {137,   1, 177, 255, 224, 255, 128, 128, 128, 128, 128},
    }
  },
  {
    {
      {253,   9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
      {175,  13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
      { 73,  17, 171, 221, 161, 179, 236, 167, 255, 234, 128},
    },
    {
      {  1,  95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
      {239,  90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
      {155,  77, 195, 248, 188, 195, 255, 255, 128, 128, 128},
    },
    {
      {  1,  24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
      {201,  51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
      { 69,  46, 190, 239, 201, 218, 255, 228, 128, 128, 128},
    },
    {
      {  1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
      {223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
      {141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128},
    },
    {
      {  1,  16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
      {190,  36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
      {149,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
    },
    {
      {  1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      {247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      {240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128},
    },
    {
      {  1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
      {213,  62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
      { 55,  93, 255, 128, 128, 128, 128, 128, 128, 128, 128},
    },
    {
      {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
    }
  },
  {
    {
      {202,  24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
      {126,  38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
      { 61,  46, 138, 219, 151, 178, 240, 170, 255, 216, 128},
    },
    {
      {  1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
      {166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
      { 39,  77, 162, 232, 172, 180, 245, 178, 255, 255, 128},
    },
    {
      {  1,  52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
      {124,  74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
      { 24,  71, 130, 219, 154, 170, 243, 182, 255, 255, 128},
    },
    {
      {  1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
      {149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
      { 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128}
    },
    {
      {  1,  81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
      {123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
      { 20,  95, 153, 243, 164, 173, 255, 203, 128, 128, 128},
    },
    {
      {  1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
      {168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
      { 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128},
    },
    {
      {  1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
      {141,  84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
      { 42,  80, 160, 240, 162, 185, 255, 205, 128, 128, 128},
    },
    {
      {  1,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      {244,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      {238,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
    },
  },
};

const uint8_t kMVUpdateProbs[kNumMVContexts][kNumMVProbs] =
{
  {
    237, 246, 253, 253, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 250, 250, 252, 254, 254,
  },
  {
    231, 243, 245, 253, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 251, 251, 254, 254, 254,
  },
};

const uint8_t kDefaultMVProbs[kNumMVContexts][kNumMVProbs] = {
  {
    162, 128, 225, 146, 172, 147, 214,  39, 156,
    128, 129, 132,  75, 145, 178, 206, 239, 254, 254,
  },
  {
    164, 128, 204, 170, 119, 235, 140, 230, 228,
    128, 130, 130,  74, 148, 180, 203, 236, 254, 254,
  },
};

void Vp8Parser::ResetProbs() {
  static_assert(
      sizeof(curr_entropy_hdr_.coeff_probs) == sizeof(kDefaultCoeffProbs),
      "coeff_probs_arrays_must_be_of_correct_size");
  memcpy(curr_entropy_hdr_.coeff_probs, kDefaultCoeffProbs,
         sizeof(curr_entropy_hdr_.coeff_probs));

  static_assert(sizeof(curr_entropy_hdr_.mv_probs) == sizeof(kDefaultMVProbs),
                "mv_probs_arrays_must_be_of_correct_size");
  memcpy(curr_entropy_hdr_.mv_probs, kDefaultMVProbs,
         sizeof(curr_entropy_hdr_.mv_probs));

  static_assert(
      sizeof(curr_entropy_hdr_.y_mode_probs) == sizeof(kDefaultYModeProbs),
      "y_probs_arrays_must_be_of_correct_size");
  memcpy(curr_entropy_hdr_.y_mode_probs, kDefaultYModeProbs,
         sizeof(curr_entropy_hdr_.y_mode_probs));

  static_assert(
      sizeof(curr_entropy_hdr_.uv_mode_probs) == sizeof(kDefaultUVModeProbs),
      "uv_probs_arrays_must_be_of_correct_size");
  memcpy(curr_entropy_hdr_.uv_mode_probs, kDefaultUVModeProbs,
         sizeof(curr_entropy_hdr_.uv_mode_probs));
}

bool Vp8Parser::ParseTokenProbs(Vp8EntropyHeader* ehdr,
                                bool update_curr_probs) {
  for (size_t i = 0; i < kNumBlockTypes; ++i) {
    for (size_t j = 0; j < kNumCoeffBands; ++j) {
      for (size_t k = 0; k < kNumPrevCoeffContexts; ++k) {
        for (size_t l = 0; l < kNumEntropyNodes; ++l) {
          bool coeff_prob_update_flag;
          BD_READ_BOOL_WITH_PROB_OR_RETURN(&coeff_prob_update_flag,
                                           kCoeffUpdateProbs[i][j][k][l]);
          if (coeff_prob_update_flag)
            BD_READ_UNSIGNED_OR_RETURN(8, &ehdr->coeff_probs[i][j][k][l]);
        }
      }
    }
  }

  if (update_curr_probs) {
    memcpy(curr_entropy_hdr_.coeff_probs, ehdr->coeff_probs,
           sizeof(curr_entropy_hdr_.coeff_probs));
  }

  return true;
}

bool Vp8Parser::ParseIntraProbs(Vp8EntropyHeader* ehdr,
                                bool update_curr_probs,
                                bool keyframe) {
  if (keyframe) {
    static_assert(
        sizeof(ehdr->y_mode_probs) == sizeof(kKeyframeYModeProbs),
        "y_probs_arrays_must_be_of_correct_size");
    memcpy(ehdr->y_mode_probs, kKeyframeYModeProbs,
           sizeof(ehdr->y_mode_probs));

    static_assert(
        sizeof(ehdr->uv_mode_probs) == sizeof(kKeyframeUVModeProbs),
        "uv_probs_arrays_must_be_of_correct_size");
    memcpy(ehdr->uv_mode_probs, kKeyframeUVModeProbs,
           sizeof(ehdr->uv_mode_probs));
  } else {
    bool intra_16x16_prob_update_flag;
    BD_READ_BOOL_OR_RETURN(&intra_16x16_prob_update_flag);
    if (intra_16x16_prob_update_flag) {
      for (size_t i = 0; i < kNumYModeProbs; ++i)
        BD_READ_UNSIGNED_OR_RETURN(8, &ehdr->y_mode_probs[i]);

      if (update_curr_probs) {
        memcpy(curr_entropy_hdr_.y_mode_probs, ehdr->y_mode_probs,
               sizeof(curr_entropy_hdr_.y_mode_probs));
      }
    }

    bool intra_chroma_prob_update_flag;
    BD_READ_BOOL_OR_RETURN(&intra_chroma_prob_update_flag);
    if (intra_chroma_prob_update_flag) {
      for (size_t i = 0; i < kNumUVModeProbs; ++i)
        BD_READ_UNSIGNED_OR_RETURN(8, &ehdr->uv_mode_probs[i]);

      if (update_curr_probs) {
        memcpy(curr_entropy_hdr_.uv_mode_probs, ehdr->uv_mode_probs,
               sizeof(curr_entropy_hdr_.uv_mode_probs));
      }
    }
  }

  return true;
}

bool Vp8Parser::ParseMVProbs(Vp8EntropyHeader* ehdr, bool update_curr_probs) {
  for (size_t mv_ctx = 0; mv_ctx < kNumMVContexts; ++mv_ctx) {
    for (size_t p = 0; p < kNumMVProbs; ++p) {
      bool mv_prob_update_flag;
      BD_READ_BOOL_WITH_PROB_OR_RETURN(&mv_prob_update_flag,
                                       kMVUpdateProbs[mv_ctx][p]);
      if (mv_prob_update_flag) {
        uint8_t prob;
        BD_READ_UNSIGNED_OR_RETURN(7, &prob);
        ehdr->mv_probs[mv_ctx][p] = prob ? (prob << 1) : 1;
      }
    }
  }

  if (update_curr_probs) {
    memcpy(curr_entropy_hdr_.mv_probs, ehdr->mv_probs,
           sizeof(curr_entropy_hdr_.mv_probs));
  }

  return true;
}

bool Vp8Parser::ParsePartitions(Vp8FrameHeader* fhdr) {
  CHECK_GE(fhdr->num_of_dct_partitions, 1u);
  CHECK_LE(fhdr->num_of_dct_partitions, kMaxDCTPartitions);

  // DCT partitions start after the first partition and partition size values
  // that follow it. There are num_of_dct_partitions - 1 sizes stored in the
  // stream after the first partition, each 3 bytes long. The size of last
  // DCT partition is not stored in the stream, but is instead calculated by
  // taking the remainder of the frame size after the penultimate DCT partition.
  size_t first_dct_pos = fhdr->first_part_offset + fhdr->first_part_size +
                         (fhdr->num_of_dct_partitions - 1) * 3;

  // Make sure we have enough data for the first partition and partition sizes.
  if (fhdr->frame_size < first_dct_pos)
    return false;

  // Total size of all DCT partitions.
  size_t bytes_left = fhdr->frame_size - first_dct_pos;

  // Position ourselves at the beginning of partition size values.
  const uint8_t* ptr = fhdr->data +
                       base::checked_cast<size_t>(fhdr->first_part_offset) +
                       fhdr->first_part_size;

  // Read sizes from the stream (if present).
  for (size_t i = 0; i < fhdr->num_of_dct_partitions - 1; ++i) {
    fhdr->dct_partition_sizes[i] = (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];

    // Make sure we have enough data in the stream for ith partition and
    // subtract its size from total.
    if (bytes_left < fhdr->dct_partition_sizes[i])
      return false;

    bytes_left -= fhdr->dct_partition_sizes[i];

    // Move to the position of the next partition size value.
    ptr += 3;
  }

  // The remainder of the data belongs to the last DCT partition.
  fhdr->dct_partition_sizes[fhdr->num_of_dct_partitions - 1] = bytes_left;

  DVLOG(4) << "Control part size: " << fhdr->first_part_size;
  for (size_t i = 0; i < fhdr->num_of_dct_partitions; ++i)
    DVLOG(4) << "DCT part " << i << " size: " << fhdr->dct_partition_sizes[i];

  return true;
}

}  // namespace media
