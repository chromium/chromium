// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP8 raw stream parser,
// as defined in RFC 6386.

#ifndef MEDIA_PARSERS_VP8_PARSER_H_
#define MEDIA_PARSERS_VP8_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "media/parsers/media_parsers_export.h"
#include "media/parsers/vp8_bool_decoder.h"

namespace media {

// See spec for definitions of values/fields.
const size_t kMaxMBSegments = 4;
const size_t kNumMBFeatureTreeProbs = 3;

// Member of Vp8FrameHeader and will be 0-initialized
// in Vp8FrameHeader's constructor.
struct Vp8SegmentationHeader {
  enum SegmentFeatureMode { FEATURE_MODE_DELTA = 0, FEATURE_MODE_ABSOLUTE = 1 };

  bool segmentation_enabled;
  bool update_mb_segmentation_map;
  bool update_segment_feature_data;
  SegmentFeatureMode segment_feature_mode;

  int8_t quantizer_update_value[kMaxMBSegments];
  int8_t lf_update_value[kMaxMBSegments];
  static const int kDefaultSegmentProb = 255;
  uint8_t segment_prob[kNumMBFeatureTreeProbs];
};

const size_t kNumBlockContexts = 4;

// Member of Vp8FrameHeader and will be 0-initialized
// in Vp8FrameHeader's constructor.
struct Vp8LoopFilterHeader {
  enum Type { LOOP_FILTER_TYPE_NORMAL = 0, LOOP_FILTER_TYPE_SIMPLE = 1 };
  Type type;
  uint8_t level;
  uint8_t sharpness_level;
  bool loop_filter_adj_enable;
  bool mode_ref_lf_delta_update;

  int8_t ref_frame_delta[kNumBlockContexts];
  int8_t mb_mode_delta[kNumBlockContexts];
};

// Member of Vp8FrameHeader and will be 0-initialized
// in Vp8FrameHeader's constructor.
struct Vp8QuantizationHeader {
  uint8_t y_ac_qi;
  int8_t y_dc_delta;
  int8_t y2_dc_delta;
  int8_t y2_ac_delta;
  int8_t uv_dc_delta;
  int8_t uv_ac_delta;
};

const size_t kNumBlockTypes = 4;
const size_t kNumCoeffBands = 8;
const size_t kNumPrevCoeffContexts = 3;
const size_t kNumEntropyNodes = 11;

const size_t kNumMVContexts = 2;
const size_t kNumMVProbs = 19;

const size_t kNumYModeProbs = 4;
const size_t kNumUVModeProbs = 3;

// Member of Vp8FrameHeader and will be 0-initialized
// in Vp8FrameHeader's constructor.
struct Vp8EntropyHeader {
  uint8_t coeff_probs[kNumBlockTypes][kNumCoeffBands][kNumPrevCoeffContexts]
                     [kNumEntropyNodes];

  uint8_t y_mode_probs[kNumYModeProbs];
  uint8_t uv_mode_probs[kNumUVModeProbs];

  uint8_t mv_probs[kNumMVContexts][kNumMVProbs];
};

const size_t kMaxDCTPartitions = 8;
const size_t kNumVp8ReferenceBuffers = 3;

enum Vp8RefType : size_t {
  VP8_FRAME_LAST = 0,
  VP8_FRAME_GOLDEN = 1,
  VP8_FRAME_ALTREF = 2,
};

struct MEDIA_PARSERS_EXPORT Vp8FrameHeader {
  Vp8FrameHeader();

  enum FrameType { KEYFRAME = 0, INTERFRAME = 1 };
  bool IsKeyframe() const { return frame_type == KEYFRAME; }

  enum GoldenRefreshMode {
    NO_GOLDEN_REFRESH = 0,
    COPY_LAST_TO_GOLDEN = 1,
    COPY_ALT_TO_GOLDEN = 2,
  };

  enum AltRefreshMode {
    NO_ALT_REFRESH = 0,
    COPY_LAST_TO_ALT = 1,
    COPY_GOLDEN_TO_ALT = 2,
  };

  FrameType frame_type;
  uint8_t version;
  bool is_experimental;
  bool show_frame;
  size_t first_part_size;

  uint16_t width;
  uint8_t horizontal_scale;
  uint16_t height;
  uint8_t vertical_scale;

  Vp8SegmentationHeader segmentation_hdr;
  Vp8LoopFilterHeader loopfilter_hdr;
  Vp8QuantizationHeader quantization_hdr;

  size_t num_of_dct_partitions;

  Vp8EntropyHeader entropy_hdr;

  bool refresh_entropy_probs;
  bool refresh_golden_frame;
  bool refresh_alternate_frame;
  GoldenRefreshMode copy_buffer_to_golden;
  AltRefreshMode copy_buffer_to_alternate;
  uint8_t sign_bias_golden;
  uint8_t sign_bias_alternate;
  bool refresh_last;

  bool mb_no_skip_coeff;
  uint8_t prob_skip_false;
  uint8_t prob_intra;
  uint8_t prob_last;
  uint8_t prob_gf;

  const uint8_t* data;
  size_t frame_size;

  size_t dct_partition_sizes[kMaxDCTPartitions];
  // Offset in bytes from data.
  off_t first_part_offset;
  // Offset in bits from first_part_offset.
  off_t macroblock_bit_offset;

  // Bool decoder state
  uint8_t bool_dec_range;
  uint8_t bool_dec_value;
  uint8_t bool_dec_count;
};

// A parser for raw VP8 streams as specified in RFC 6386.
class MEDIA_PARSERS_EXPORT Vp8Parser {
 public:
  Vp8Parser();
  ~Vp8Parser();

  // Try to parse exactly one VP8 frame starting at |ptr| and of size |size|,
  // filling the parsed data in |fhdr|. Return true on success.
  // Size has to be exactly the size of the frame and coming from the caller,
  // who needs to acquire it from elsewhere (normally from a container).
  bool ParseFrame(const uint8_t* ptr, size_t size, Vp8FrameHeader* fhdr);

 private:
  bool ParseFrameTag(Vp8FrameHeader* fhdr);
  bool ParseFrameHeader(Vp8FrameHeader* fhdr);

  bool ParseSegmentationHeader(bool keyframe);
  bool ParseLoopFilterHeader(bool keyframe);
  bool ParseQuantizationHeader(Vp8QuantizationHeader* qhdr);
  bool ParseTokenProbs(Vp8EntropyHeader* ehdr, bool update_curr_probs);
  bool ParseIntraProbs(Vp8EntropyHeader* ehdr,
                       bool update_curr_probs,
                       bool keyframe);
  bool ParseMVProbs(Vp8EntropyHeader* ehdr, bool update_curr_probs);
  bool ParsePartitions(Vp8FrameHeader* fhdr);
  void ResetProbs();

  // These persist across calls to ParseFrame() and may be used and/or updated
  // for subsequent frames if the stream instructs us to do so.
  Vp8SegmentationHeader curr_segmentation_hdr_;
  Vp8LoopFilterHeader curr_loopfilter_hdr_;
  Vp8EntropyHeader curr_entropy_hdr_;

  const uint8_t* stream_;
  size_t bytes_left_;
  Vp8BoolDecoder bd_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Parser);
};

}  // namespace media

#endif  // MEDIA_PARSERS_VP8_PARSER_H_
