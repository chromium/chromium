// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_TEST_SUITE_H_
#define MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_TEST_SUITE_H_

#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"

namespace base {
namespace test {
class TaskEnvironment;
}  // namespace test
}  // namespace base

namespace media {
namespace test {

class VideoDecodeAcceleratorTestSuite : public base::TestSuite {
 public:
  static VideoDecodeAcceleratorTestSuite* Create(int argc, char** argv);

  VideoDecodeAcceleratorTestSuite(const VideoDecodeAcceleratorTestSuite&) =
      delete;
  ~VideoDecodeAcceleratorTestSuite() override;

  VideoDecodeAcceleratorTestSuite& operator=(
      const VideoDecodeAcceleratorTestSuite&) = delete;

  // Gets the video the tests will be ran on.
  const media::test::VideoBitstream* Video() const;
  // Checks whether frame validation is enabled.
  bool IsValidatorEnabled() const;
  // Gets the validator type.
  VideoPlayerTestEnvironment::ValidatorType GetValidatorType() const;
  // Returns which implementation is used.
  DecoderImplementation GetDecoderImplementation() const;
  // Returns whether the final output of the decoder should be linear buffers.
  bool ShouldOutputLinearBuffers() const;

  // Gets the frame output mode.
  FrameOutputMode GetFrameOutputMode() const;
  // Gets the file format used when outputting frames.
  VideoFrameFileWriter::OutputFormat GetFrameOutputFormat() const;
  // Gets the maximum number of frames that will be output.
  uint64_t GetFrameOutputLimit() const;
  // Gets the output folder.
  const base::FilePath& OutputFolder() const;

  // Gets the name of the test output file path (testsuitename/testname).
  base::FilePath GetTestOutputFilePath() const;

  // Gets whether the video_test_env_ exists.
  bool ValidVideoTestEnv() const;

  // Queries whether V4L2 virtual driver is used on ARM VM.
  bool IsV4L2VirtualDriver() const;

 protected:
  // Overridden from base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  VideoDecodeAcceleratorTestSuite(
      int argc,
      char** argv,
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      VideoPlayerTestEnvironment::ValidatorType validator_type,
      const DecoderImplementation implementation,
      bool linear_output,
      const base::FilePath& output_folder,
      const FrameOutputConfig& frame_output_config,
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);

  static std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;

  std::unique_ptr<VideoPlayerTestEnvironment> video_test_env_;
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_TEST_SUITE_H_
