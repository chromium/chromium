// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/mock_media_log.h"
#include "media/base/pipeline_status.h"
#include "media/filters/hls_codec_detector.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using ::base::test::RunOnceCallback;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;

class HlsCodecDetectorTest : public testing::Test {
 public:
  HlsCodecDetectorTest()
      : media_log_(std::make_unique<NiceMock<media::MockMediaLog>>()),
        mock_hrh_(std::make_unique<media::MockHlsRenditionHost>()),
        detector_(std::make_unique<HlsCodecDetector>(media_log_.get(),
                                                     mock_hrh_.get())) {}

  ~HlsCodecDetectorTest() override { task_environment_.RunUntilIdle(); }

 protected:
  MOCK_METHOD(void, CodecsOk, (std::string codecs), ());
  MOCK_METHOD(void, ContainerOk, (std::string codecs), ());
  MOCK_METHOD(void, DetectionFailed, (HlsDemuxerStatus codecs), ());

  void OnDetection(
      HlsDemuxerStatus::Or<HlsCodecDetector::ContainerAndCodecs> result) {
    if (result.has_value()) {
      auto cnc = std::move(result).value();
      ContainerOk(cnc.container);
      CodecsOk(cnc.codecs);
    } else {
      return DetectionFailed(std::move(result).error());
    }
  }

  void CheckCodecs(std::unique_ptr<HlsDataSourceStream> stream) {
    detector_->DetermineContainerAndCodec(
        std::move(stream), base::BindOnce(&HlsCodecDetectorTest::OnDetection,
                                          base::Unretained(this)));
  }

  void CheckContainer(std::unique_ptr<HlsDataSourceStream> stream) {
    detector_->DetermineContainerOnly(
        std::move(stream), base::BindOnce(&HlsCodecDetectorTest::OnDetection,
                                          base::Unretained(this)));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MockHlsRenditionHost> mock_hrh_;
  std::unique_ptr<HlsCodecDetector> detector_;
};

TEST_F(HlsCodecDetectorTest, TestTS) {
  auto stream = std::make_unique<HlsDataSourceStream>(
      std::make_unique<FileHlsDataSource>("bear-1280x720-hls.ts"));
  EXPECT_CALL(*this, CodecsOk("avc1.420000, mp4a.40.05"));
  EXPECT_CALL(*this, ContainerOk("video/mp2t"));
  CheckCodecs(std::move(stream));
  task_environment_.RunUntilIdle();

  stream = std::make_unique<HlsDataSourceStream>(
      std::make_unique<FileHlsDataSource>("bear-1280x720-hls.ts"));
  EXPECT_CALL(*this, CodecsOk(""));
  EXPECT_CALL(*this, ContainerOk("video/mp2t"));
  CheckContainer(std::move(stream));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsCodecDetectorTest, TestFmp4) {
  auto stream = std::make_unique<HlsDataSourceStream>(
      std::make_unique<FileHlsDataSource>("bear-1280x720-avt_subt_frag.mp4"));
  EXPECT_CALL(*this, DetectionFailed(_));
  CheckCodecs(std::move(stream));
  task_environment_.RunUntilIdle();
}

}  // namespace media
