// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_DECODER_BUFFER_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_DECODER_BUFFER_VALIDATOR_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/parsers/vp8_parser.h"
#include "media/video/h264_parser.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class DecoderBuffer;

namespace test {
class DecoderBufferValidator : public BitstreamProcessor {
 public:
  DecoderBufferValidator(const gfx::Rect& visible_rect,
                         size_t num_temporal_layers);
  ~DecoderBufferValidator() override;

  // BitstreamProcessor implementation.
  void ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                        size_t frame_index) override;
  bool WaitUntilDone() override;

 protected:
  // Returns true if decoder_buffer is valid and expected, otherwise false.
  virtual bool Validate(const DecoderBuffer& decoder_buffer,
                        const BitstreamBufferMetadata& metadata) = 0;

  // The expected visible rectangle that |decoder_buffer| has.
  const gfx::Rect visible_rect_;
  // The number of temporal layers.
  const size_t num_temporal_layers_;

 private:
  // The number of detected errors by Validate().
  size_t num_errors_ = 0;
};

class H264Validator : public DecoderBufferValidator {
 public:
  H264Validator(VideoCodecProfile profile,
                const gfx::Rect& visible_rect,
                const size_t num_temporal_layers,
                absl::optional<uint8_t> level = absl::nullopt);
  ~H264Validator() override;

 private:
  bool Validate(const DecoderBuffer& decoder_buffer,
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
  // absl::nullopt.
  absl::optional<uint8_t> level_;
};

class VP8Validator : public DecoderBufferValidator {
 public:
  VP8Validator(const gfx::Rect& visible_rect, size_t num_temporal_layers);
  ~VP8Validator() override;

 private:
  bool Validate(const DecoderBuffer& decoder_buffer,
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
               size_t num_temporal_layers);
  ~VP9Validator() override;

 private:
  // Struct representing the expected state of a reference buffer.
  struct BufferState {
    int picture_id = 0;
    uint8_t spatial_id = 0;
    uint8_t temporal_id = 0;
  };

  bool Validate(const DecoderBuffer& decoder_buffer,
                const BitstreamBufferMetadata& metadata) override;

  Vp9Parser parser_;

  // The expected VP9 profile of |decoder_buffer|.
  const int profile_;
  const size_t max_num_spatial_layers_;
  size_t cur_num_spatial_layers_;
  std::vector<gfx::Size> spatial_layer_resolutions_;
  int next_picture_id_;

  // An optional state for each specified VP9 reference buffer.
  // A nullopt indicates either keyframe not yet seen, or that a
  // buffer has been invalidated (e.g. due to sync points).
  std::array<absl::optional<BufferState>, kVp9NumRefFrames> reference_buffers_;
};

class AV1Validator : public DecoderBufferValidator {
 public:
  // TODO(greenjustin): Add support for more than 1 spatial and temporal layer
  // if we need it.
  explicit AV1Validator(const gfx::Rect& visible_rect);
  ~AV1Validator() override = default;

 private:
  bool Validate(const DecoderBuffer& decoder_buffer,
                const BitstreamBufferMetadata& metadata) override;

  libgav1::InternalFrameBufferList buffer_list_;
  libgav1::BufferPool buffer_pool_;
  libgav1::DecoderState decoder_state_;
  absl::optional<libgav1::ObuSequenceHeader> sequence_header_ = absl::nullopt;
  uint64_t frame_num_ = 0;
};
}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_DECODER_BUFFER_VALIDATOR_H_
