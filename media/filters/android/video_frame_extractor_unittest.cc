// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/android/video_frame_extractor.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

struct ExtractVideoFrameResult {
  bool success = false;
  std::vector<uint8_t> encoded_frame;
  VideoDecoderConfig decoder_config;
};

void OnFrameExtracted(ExtractVideoFrameResult* result,
                      base::RepeatingClosure quit_closure,
                      bool success,
                      std::vector<uint8_t> encoded_frame,
                      const VideoDecoderConfig& decoder_config) {
  result->success = success;
  result->encoded_frame = std::move(encoded_frame);
  result->decoder_config = decoder_config;

  quit_closure.Run();
}

class VideoFrameExtractorTest : public testing::Test {
 public:
  VideoFrameExtractorTest() {}
  ~VideoFrameExtractorTest() override {}

 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  ExtractVideoFrameResult ExtractFrame(const base::FilePath& file_path) {
    base::File file(
        file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    DCHECK(file.IsValid());
    data_source_ = std::make_unique<FileDataSource>();
    CHECK(data_source_->Initialize(file_path));
    extractor_ = std::make_unique<VideoFrameExtractor>(data_source_.get());

    ExtractVideoFrameResult result;
    base::RunLoop loop;
    extractor_->Start(
        base::BindOnce(&OnFrameExtracted, &result, loop.QuitClosure()));
    loop.Run();
    return result;
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FileDataSource> data_source_;
  std::unique_ptr<VideoFrameExtractor> extractor_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameExtractorTest);
};

// Verifies the encoded video frame can be extracted correctly.
TEST_F(VideoFrameExtractorTest, ExtractVideoFrame) {
  auto result = ExtractFrame(GetTestDataFilePath("bear.mp4"));
  EXPECT_TRUE(result.success);
  EXPECT_GT(result.encoded_frame.size(), 0u);
  EXPECT_EQ(result.decoder_config.codec(), VideoCodec::kCodecH264);
}

// Verifies graceful failure when trying to extract frame from an invalid video
// file.
TEST_F(VideoFrameExtractorTest, ExtractInvalidVideoFile) {
  // Creates a dummy video file, frame extraction should fail.
  base::FilePath file = temp_dir().AppendASCII("test.txt");
  EXPECT_GT(base::WriteFile(file, "123", sizeof("123")), 0);

  auto result = ExtractFrame(file);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.encoded_frame.size(), 0u);
  EXPECT_FALSE(result.decoder_config.IsValidConfig());
}

}  // namespace
}  // namespace media
