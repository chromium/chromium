// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_BITSTREAM_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_BITSTREAM_VALIDATOR_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/video_decoder.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MediaLog;
class VideoFrame;
class VideoDecoderConfig;

namespace test {
// BitstreamValidator validates encoded bitstreams by decoding them using a
// software decoder and optionally processes the decoded video frames using one
// or more VideoFrameProcessors (e.g. compute SSIM with the original video frame
// and save them to files).
class BitstreamValidator : public BitstreamProcessor {
 public:
  // |decoder_config| is used to create a software VideoDecoder.
  // |last_frame_index| is the index of the last bitstream in the stream.
  static std::unique_ptr<BitstreamValidator> Create(
      const VideoDecoderConfig& decoder_config,
      size_t last_frame_index,
      std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors =
          {},
      std::optional<size_t> spatial_layer_index_to_decode_ = std::nullopt,
      std::optional<size_t> temporal_layer_index_to_decode = std::nullopt,
      const std::vector<gfx::Size>& spatial_layer_resolutions = {});

  ~BitstreamValidator() override;

  // BitstreamProcessor implementation.
  void ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                        size_t frame_index) override;
  bool WaitUntilDone() override;

 private:
  BitstreamValidator(
      std::unique_ptr<VideoDecoder> decoder,
      std::unique_ptr<MediaLog> media_log,
      size_t last_frame_index,
      const gfx::Rect& decoding_rect,
      std::optional<size_t> spatial_layer_index_to_decode_,
      std::optional<size_t> temporal_layer_index_to_decode,
      const std::vector<gfx::Size>& spatial_layer_resolutions,
      std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors);
  BitstreamValidator(const BitstreamValidator&) = delete;
  BitstreamValidator& operator=(const BitstreamValidator&) = delete;

  bool Initialize(const VideoDecoderConfig& decoder_config);
  void InitializeVideoDecoder(const VideoDecoderConfig& decoder_config,
                              VideoDecoder::InitCB init_cb);
  void ProcessBitstreamTask(scoped_refptr<BitstreamRef> decoder_buffer,
                            size_t frame_index);
  void OutputFrameProcessed();

  // Functions for media::VideoDecoder.
  void DecodeDone(int64_t timestamp, DecoderStatus status);
  void VerifyOutputFrame(scoped_refptr<VideoFrame> frame);

  // Construct the spatial index conversion table |original_spatial_indices_|
  // from |spatial_layer_resolutions|.
  void ConstructSpatialIndices(
      const std::vector<gfx::Size>& spatial_layer_resolutions);

  // Validator components touched by validator_thread_ only.
  std::unique_ptr<VideoDecoder> decoder_;
  const std::unique_ptr<MediaLog> media_log_;
  const size_t last_frame_index_;
  const gfx::Rect desired_decoding_rect_;
  const std::optional<size_t> spatial_layer_index_to_decode_;
  const std::optional<size_t> temporal_layer_index_to_decode_;
  const std::vector<gfx::Size> spatial_layer_resolutions_;
  const std::vector<std::unique_ptr<VideoFrameProcessor>>
      video_frame_processors_;
  // The key is timestamp, and the value is BitstreamRef that is being processed
  // by |decoder_| and its frame index.
  static constexpr size_t kDecoderBufferMapSize = 32;
  base::LRUCache<int64_t, std::pair<size_t, scoped_refptr<BitstreamRef>>>
      decoding_buffers_{kDecoderBufferMapSize};

  // The conversion table from the current spatial index to the spatial index of
  // the initial spatial layers. Constructed in ConstructSpatialIndices().
  std::vector<uint8_t> original_spatial_indices_;

  base::Thread validator_thread_;
  mutable base::ConditionVariable validator_cv_;
  mutable base::Lock validator_lock_;

  // The number of buffers that are being validated.
  size_t num_buffers_validating_ GUARDED_BY(validator_lock_);
  // True if |decoder_| detects an error while decoding bitstreams.
  bool decode_error_ GUARDED_BY(validator_lock_);
  // True if a flush is being processed.
  bool waiting_flush_done_ GUARDED_BY(validator_lock_);

  SEQUENCE_CHECKER(validator_sequence_checker_);
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_BITSTREAM_VALIDATOR_H_
