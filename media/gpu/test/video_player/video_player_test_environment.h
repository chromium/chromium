// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_

#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_player/decoder_wrapper.h"
#include "media/gpu/test/video_test_environment.h"

namespace media {
namespace test {

class VideoBitstream;

// Test environment for video decode tests. Performs setup and teardown once for
// the entire test run.
class VideoPlayerTestEnvironment : public VideoTestEnvironment {
 public:
  enum class ValidatorType {
    kNone,
    kMD5,
    kSSIM,
  };

  static VideoPlayerTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      ValidatorType validator_type,
      const DecoderImplementation implementation,
      bool linear_output,
      const base::FilePath& output_folder = base::FilePath(),
      const FrameOutputConfig& frame_output_config = FrameOutputConfig(),
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {},
      const bool need_task_environment = true);
  ~VideoPlayerTestEnvironment() override;

  // Get the video the tests will be ran on.
  const media::test::VideoBitstream* Video() const;
  // Check whether frame validation is enabled.
  bool IsValidatorEnabled() const;
  // Get the validator type.
  ValidatorType GetValidatorType() const;
  // Return which implementation is used.
  DecoderImplementation GetDecoderImplementation() const;
  // Returns whether the final output of the decoder should be linear buffers.
  bool ShouldOutputLinearBuffers() const;

  // Get the frame output mode.
  FrameOutputMode GetFrameOutputMode() const;
  // Get the file format used when outputting frames.
  VideoFrameFileWriter::OutputFormat GetFrameOutputFormat() const;
  // Get the maximum number of frames that will be output.
  uint64_t GetFrameOutputLimit() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;

 private:
  VideoPlayerTestEnvironment(
      std::unique_ptr<media::test::VideoBitstream> video,
      ValidatorType validator_type,
      const DecoderImplementation implementation,
      bool linear_output,
      const base::FilePath& output_folder,
      const FrameOutputConfig& frame_output_config,
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features,
      const bool need_task_environment = true);

  const std::unique_ptr<media::test::VideoBitstream> video_;
  const ValidatorType validator_type_;
  const DecoderImplementation implementation_;
  const bool linear_output_;

  const FrameOutputConfig frame_output_config_;
  const base::FilePath output_folder_;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
