// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_

#include <limits>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "media/base/video_codecs.h"
#include "media/gpu/test/video_test_environment.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {
namespace test {

class Video;

// Test environment for video encoder tests. Performs setup and teardown once
// for the entire test run.
class VideoEncoderTestEnvironment : public VideoTestEnvironment {
 public:
  // VideoEncoderTest uses at most 60 frames in the given video file.
  // This limitation is required as a long video stream might not fit in
  // a device's memory or the number of allocatable handles in the system.
  // TODO(hiroh): Streams frames from disk so we can avoid this limitation when
  // encoding long video streams.
  static constexpr size_t kMaxReadFrames = 60;

  static VideoEncoderTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      bool enable_bitstream_validator,
      const base::FilePath& output_folder,
      const std::string& codec,
      size_t num_temporal_layers,
      bool output_bitstream,
      const FrameOutputConfig& frame_output_config = FrameOutputConfig());

  ~VideoEncoderTestEnvironment() override;

  // Get the video the tests will be ran on.
  media::test::Video* Video() const;
  // Whether bitstream validation is enabled.
  bool IsBitstreamValidatorEnabled() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;
  // Get the output codec profile.
  VideoCodecProfile Profile() const;
  // Get the number of temporal layers.
  size_t NumTemporalLayers() const;
  // Get the target bitrate (bits/second).
  uint32_t Bitrate() const;
  // Whether the encoded bitstream is saved to disk.
  bool SaveOutputBitstream() const;
  base::Optional<base::FilePath> OutputBitstreamFilePath() const;
  // Gets the frame output configuration.
  const FrameOutputConfig& ImageOutputConfig() const;

  // Get the GpuMemoryBufferFactory for doing buffer allocations. This needs to
  // survive as long as the process is alive just like in production which is
  // why it's in here as there are threads that won't immediately die when an
  // individual test is completed.
  gpu::GpuMemoryBufferFactory* GetGpuMemoryBufferFactory() const;

 private:
  VideoEncoderTestEnvironment(std::unique_ptr<media::test::Video> video,
                              bool enable_bitstream_validator,
                              const base::FilePath& output_folder,
                              VideoCodecProfile profile,
                              size_t num_temporal_layers,
                              uint32_t bitrate,
                              bool save_output_bitstream,
                              const FrameOutputConfig& frame_output_config);

  // Video file to be used for testing.
  const std::unique_ptr<media::test::Video> video_;
  // Whether bitstream validation should be enabled while testing.
  const bool enable_bitstream_validator_;
  // Output folder to be used to store test artifacts (e.g. perf metrics).
  const base::FilePath output_folder_;
  // VideoCodecProfile to be produced by VideoEncoder.
  const VideoCodecProfile profile_;
  // The number of temporal layers of the stream to be produced by VideoEncoder.
  // This is only for vp9 stream.
  const size_t num_temporal_layers_;
  // Targeted bitrate (bits/second) of the stream produced by VideoEncoder.
  const uint32_t bitrate_;
  // Whether the bitstream produced by VideoEncoder is saved to disk.
  const bool save_output_bitstream_;
  // The configuration about saving decoded images of bitstream encoded by
  // VideoEncoder.
  // The configuration used when saving the decoded images of bitstream encoded
  // by VideoEncoder to disk.
  const FrameOutputConfig frame_output_config_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
