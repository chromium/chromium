// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_DECODER_BUFFER_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_DECODER_BUFFER_VALIDATOR_H_

#include <stdint.h>

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/vp9_parser.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class DecoderBuffer;

namespace test {

class DecoderBufferValidator : public BitstreamProcessor {
 public:
  static std::unique_ptr<DecoderBufferValidator> Create(
      VideoCodecProfile profile,
      const gfx::Rect& visible_rect,
      size_t num_spatial_layers,
      size_t num_temporal_layers,
      SVCInterLayerPredMode inter_layer_pred);
  ~DecoderBufferValidator() override;

  // BitstreamProcessor implementation.
  void ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                        size_t frame_index) override;
  bool WaitUntilDone() override;

  const std::vector<int>& GetQPValues(size_t spatial_idx,
                                      size_t temporal_idx) const {
    return qp_values_[spatial_idx][temporal_idx];
  }

 protected:
  static constexpr size_t kMaxTemporalLayers = 3;
  static constexpr size_t kMaxSpatialLayers = 3;

  DecoderBufferValidator(const gfx::Rect& visible_rect,
                         size_t num_temporal_layers);

  // Returns true if decoder_buffer is valid and expected, otherwise false.
  virtual bool Validate(const DecoderBuffer* buffer,
                        const BitstreamBufferMetadata& metadata) = 0;

  // The expected visible rectangle that |decoder_buffer| has.
  const gfx::Rect visible_rect_;
  // The number of temporal layers.
  const size_t num_temporal_layers_;

  std::vector<int> qp_values_[kMaxSpatialLayers][kMaxTemporalLayers];

 private:
  // The number of detected errors by Validate().
  size_t num_errors_ = 0;
};

class H264Validator : public DecoderBufferValidator {
 public:
  H264Validator(VideoCodecProfile profile,
                const gfx::Rect& visible_rect,
                const size_t num_temporal_layers,
                std::optional<uint8_t> level = std::nullopt);
  ~H264Validator() override;

 private:
  bool Validate(const DecoderBuffer* buffer,
                const BitstreamBufferMetadata& metadata) override;

  // Returns whether the |slice_hdr| is the first slice of a new frame.
  bool IsNewPicture(const H264SliceHeader& slice_hdr);
  // Initialize |cur_pic_| with |slice_hdr|.
  bool UpdateCurrentPicture(const H264SliceHeader& slice_hdr);

  // These represent whether sequence parameter set (SPS), picture parameter set
  // (PPS) and IDR frame have been input, respectively.
  bool seen_sps_;
  bool seen_pps_;
  bool seen_idr_;

  // Current h264 picture. It is used to check that H264Picture is created from
  // a given bitstream.
  scoped_refptr<H264Picture> cur_pic_;
  // Current SPS and PPS id.
  int cur_sps_id_;
  int cur_pps_id_;

  H264Parser parser_;

  // The expected h264 profile of |decoder_buffer|.
  const int profile_;
  // The expected h264 level of |decoder_buffer|. Check if it is not
  // std::nullopt.
  std::optional<uint8_t> level_;
};

class VP8Validator : public DecoderBufferValidator {
 public:
  VP8Validator(const gfx::Rect& visible_rect, size_t num_temporal_layers);
  ~VP8Validator() override;

 private:
  bool Validate(const DecoderBuffer* buffer,
                const BitstreamBufferMetadata& metadata) override;

  Vp8Parser parser_;
  // Whether key frame has been input.
  bool seen_keyframe_ = false;
};

class VP9Validator : public DecoderBufferValidator {
 public:
  VP9Validator(VideoCodecProfile profile,
               const gfx::Rect& visible_rect,
               size_t max_num_spatial_layers,
               size_t num_temporal_layers,
               SVCInterLayerPredMode inter_layer_pred);
  ~VP9Validator() override;

 private:
  // Struct representing the expected state of a reference buffer.
  struct BufferState {
    int picture_id = 0;
    uint8_t spatial_id = 0;
    uint8_t temporal_id = 0;
  };

  bool Validate(const DecoderBuffer* buffer,
                const BitstreamBufferMetadata& metadata) override;

  // Validate DecoderBuffer for a vanilla stream.
  bool ValidateVanillaStream(const DecoderBuffer& decoder_buffer,
                             const BitstreamBufferMetadata& metadata,
                             const Vp9FrameHeader& header);
  // Validate DecoderBuffer for a temporal or spatial layer stream.
  bool ValidateSVCStream(const DecoderBuffer& decoder_buffer,
                         const BitstreamBufferMetadata& metadata,
                         const Vp9FrameHeader& header);
  // Validate DecoderBuffer for S-mode stream.
  bool ValidateSmodeStream(const DecoderBuffer& decoder_buffer,
                           const BitstreamBufferMetadata& metadata,
                           const Vp9FrameHeader& header);

  // The expected VP9 profile of |decoder_buffer|.
  const int profile_;
  const size_t max_num_spatial_layers_;
  const bool s_mode_;

  std::vector<std::unique_ptr<Vp9Parser>> parsers_;

  size_t cur_num_spatial_layers_;
  std::vector<gfx::Size> spatial_layer_resolutions_;
  int next_picture_id_;

  uint8_t begin_active_spatial_layer_index_ = 0;

  // An optional state for each specified VP9 reference buffer.
  // A nullopt indicates either keyframe not yet seen, or that a
  // buffer has been invalidated (e.g. due to sync points).
  std::vector<std::array<std::optional<BufferState>, kVp9NumRefFrames>>
      reference_buffers_;

  std::optional<base::TimeDelta> dropped_superframe_timestamp_;
};

class AV1Validator : public DecoderBufferValidator {
 public:
  // TODO(greenjustin): Add support for more than 1 spatial and temporal layer
  // if we need it.
  explicit AV1Validator(const gfx::Rect& visible_rect);
  ~AV1Validator() override = default;

 private:
  bool Validate(const DecoderBuffer* buffer,
                const BitstreamBufferMetadata& metadata) override;

  libgav1::InternalFrameBufferList buffer_list_;
  libgav1::BufferPool buffer_pool_;
  libgav1::DecoderState decoder_state_;
  std::optional<libgav1::ObuSequenceHeader> sequence_header_ = std::nullopt;
  uint64_t frame_num_ = 0;
};
}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_DECODER_BUFFER_VALIDATOR_H_
