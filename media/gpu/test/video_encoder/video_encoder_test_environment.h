// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_

#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/video/video_encode_accelerator.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {

class Bitrate;

namespace test {
class RawVideo;

// Test environment for video encoder tests. Performs setup and teardown once
// for the entire test run.
class VideoEncoderTestEnvironment : public VideoTestEnvironment {
 public:
  enum class TestType {
    // Validate the encoder output.
    // video_encode_accelerator_tests
    kValidation,
    // Measure the quality performance.
    // video_encode_accelerator_perf_tests --quality
    kQualityPerformance,
    // Measure the speed performance.
    // video_encode_accelerator_perf_tests --speed
    kSpeedPerformance,
  };

  // |read_all_frames_in_video| is weather we read all the frames in
  // |video_path|. If it is false, it reads up to RawVideo::kLimitedReadFrames.
  static VideoEncoderTestEnvironment* Create(
      TestType test_type,
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      const base::FilePath& output_folder,
      const std::string& codec,
      const std::string& svc_mode,
      VideoEncodeAccelerator::Config::ContentType content_type,
      bool save_output_bitstream,
      std::optional<uint32_t> output_bitrate,
      Bitrate::Mode bitrate_mode,
      bool reverse,
      const FrameOutputConfig& frame_output_config = FrameOutputConfig(),
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {});

  ~VideoEncoderTestEnvironment() override;

  // Get the video the tests will be ran on.
  media::test::RawVideo* Video() const;
  // Generate the nv12 video from |video_| the test will be ran on.
  media::test::RawVideo* GenerateNV12Video();
  // Gets the running test type.
  TestType RunTestType() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;
  // Get the output codec profile.
  VideoCodecProfile Profile() const;
  // Get the spatial layers config for SVC. Return empty vector in non SVC mode.
  const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
  SpatialLayers() const;
  SVCInterLayerPredMode InterLayerPredMode() const;
  VideoEncodeAccelerator::Config::ContentType ContentType() const;
  // Get the target bitrate (bits/second).
  const VideoBitrateAllocation& BitrateAllocation() const;
  // Whether the encoded bitstream is saved to disk.
  bool SaveOutputBitstream() const;
  // Get the output file path for a bitstream to be saved to disk.
  base::FilePath OutputFilePath(const VideoCodec& codec,
                                bool svc_enable = false,
                                int spatial_idx = 0,
                                int temporal_idx = 0) const;
  // True if the video should play backwards at reaching the end of video.
  // Otherwise the video loops. See the comment in AlignedDataHelper for detail.
  bool Reverse() const;
  std::optional<base::FilePath> OutputBitstreamFilePath() const;
  // Gets the frame output configuration.
  const FrameOutputConfig& ImageOutputConfig() const;

  // Get the GpuMemoryBufferFactory for doing buffer allocations. This needs to
  // survive as long as the process is alive just like in production which is
  // why it's in here as there are threads that won't immediately die when an
  // individual test is completed.
  gpu::GpuMemoryBufferFactory* GetGpuMemoryBufferFactory() const;

 private:
  // TODO(crbug.com/40228467): merge |use_vbr| and |bitrate| into a single
  // Bitrate-typed field.
  VideoEncoderTestEnvironment(
      TestType test_type,
      std::unique_ptr<media::test::RawVideo> video,
      const base::FilePath& output_folder,
      const base::FilePath& output_bitstream_file_base_name,
      VideoCodecProfile profile,
      SVCInterLayerPredMode inter_layer_pred_mode,
      size_t num_temporal_layers,
      size_t num_spatial_layers,
      VideoEncodeAccelerator::Config::ContentType content_type,
      const media::VideoBitrateAllocation& bitrate,
      bool save_output_bitstream,
      bool reverse,
      const FrameOutputConfig& frame_output_config,
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);

  const TestType test_type_;
  // Video file to be used for testing.
  const std::unique_ptr<media::test::RawVideo> video_;
  // NV12 video file to be used for testing.
  std::unique_ptr<media::test::RawVideo> nv12_video_;
  // Output folder to be used to store test artifacts (e.g. perf metrics).
  const base::FilePath output_folder_;
  // The base name of the file to be used to store the bitstream.
  const base::FilePath output_bitstream_file_base_name_;
  // VideoCodecProfile to be produced by VideoEncoder.
  const VideoCodecProfile profile_;
  // Inter layer predict mode.
  const SVCInterLayerPredMode inter_layer_pred_mode_;
  // Targeted bitrate (bits/second) of the stream produced by VideoEncoder.
  const VideoBitrateAllocation bitrate_;
  // The spatial layers of the stream which aligned with |num_spatial_layers_|
  // and |num_temporal_layers_|. This is only for vp9 stream.
  std::vector<VideoEncodeAccelerator::Config::SpatialLayer> spatial_layers_;
  // A type of content to be encoded.
  VideoEncodeAccelerator::Config::ContentType content_type_;
  // Whether the bitstream produced by VideoEncoder is saved to disk.
  const bool save_output_bitstream_;
  // True if the video should play backwards at reaching the end of video.
  // Otherwise the video loops. See the comment in AlignedDataHelper for detail.
  const bool reverse_;
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
