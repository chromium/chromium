// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_decode_accelerator_test_suite.h"
#include "base/test/task_environment.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_bitstream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

std::unique_ptr<base::test::TaskEnvironment>
    VideoDecodeAcceleratorTestSuite::task_environment_;

// static
VideoDecodeAcceleratorTestSuite* VideoDecodeAcceleratorTestSuite::Create(
    int argc,
    char** argv) {
  // TODO(bchoobineh)
  NOTIMPLEMENTED();

  return nullptr;
}

VideoDecodeAcceleratorTestSuite::VideoDecodeAcceleratorTestSuite(
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
    const std::vector<base::test::FeatureRef>& disabled_features)
    : base::TestSuite(argc, argv),
      video_test_env_(std::move(media::test::VideoPlayerTestEnvironment::Create(
          video_path,
          video_metadata_path,
          validator_type,
          implementation,
          linear_output,
          base::FilePath(output_folder),
          frame_output_config,
          enabled_features,
          disabled_features,
          false))) {}

VideoDecodeAcceleratorTestSuite::~VideoDecodeAcceleratorTestSuite() = default;

const media::test::VideoBitstream* VideoDecodeAcceleratorTestSuite::Video()
    const {
  return video_test_env_->Video();
}

bool VideoDecodeAcceleratorTestSuite::IsValidatorEnabled() const {
  return video_test_env_->IsValidatorEnabled();
}

VideoPlayerTestEnvironment::ValidatorType
VideoDecodeAcceleratorTestSuite::GetValidatorType() const {
  return video_test_env_->GetValidatorType();
}

DecoderImplementation
VideoDecodeAcceleratorTestSuite::GetDecoderImplementation() const {
  return video_test_env_->GetDecoderImplementation();
}

bool VideoDecodeAcceleratorTestSuite::ShouldOutputLinearBuffers() const {
  return video_test_env_->ShouldOutputLinearBuffers();
}

FrameOutputMode VideoDecodeAcceleratorTestSuite::GetFrameOutputMode() const {
  return video_test_env_->GetFrameOutputMode();
}

VideoFrameFileWriter::OutputFormat
VideoDecodeAcceleratorTestSuite::GetFrameOutputFormat() const {
  return video_test_env_->GetFrameOutputFormat();
}

uint64_t VideoDecodeAcceleratorTestSuite::GetFrameOutputLimit() const {
  return video_test_env_->GetFrameOutputLimit();
}

const base::FilePath& VideoDecodeAcceleratorTestSuite::OutputFolder() const {
  return video_test_env_->OutputFolder();
}

base::FilePath VideoDecodeAcceleratorTestSuite::GetTestOutputFilePath() const {
  return video_test_env_->GetTestOutputFilePath();
}

void VideoDecodeAcceleratorTestSuite::Initialize() {
  // TODO(bchoobineh)
  NOTIMPLEMENTED();
}

void VideoDecodeAcceleratorTestSuite::Shutdown() {
  // TODO(bchoobineh)
  NOTIMPLEMENTED();
}

}  // namespace test
}  // namespace media
