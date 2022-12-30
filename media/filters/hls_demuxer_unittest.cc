// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_demuxer.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace media {

class HlsDemuxerTest : public testing::Test {
 protected:
  HlsDemuxerTest() { CreateNewDemuxer(); }
  ~HlsDemuxerTest() override { Shutdown(); }

  HlsDemuxerTest(const HlsDemuxerTest&) = delete;
  HlsDemuxerTest& operator=(const HlsDemuxerTest&) = delete;

  void Shutdown() {
    if (demuxer_) {
      demuxer_->Stop();
    }
    demuxer_.reset();
    task_environment_.RunUntilIdle();
  }

  void CreateNewDemuxer() {
    EXPECT_MEDIA_LOG(HlsDemuxerCtor());
    demuxer_ = std::make_unique<HlsDemuxer>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), &media_log_);
  }

  base::test::TaskEnvironment task_environment_;
  StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<HlsDemuxer> demuxer_;
};

TEST_F(HlsDemuxerTest, PreInitializationState) {
  EXPECT_TRUE(demuxer_);
  EXPECT_EQ(0u, demuxer_->GetAllStreams().size());
  EXPECT_EQ("HlsDemuxer", demuxer_->GetDisplayName());
  EXPECT_EQ(DemuxerType::kHlsDemuxer, demuxer_->GetDemuxerType());
  EXPECT_EQ(base::TimeDelta(), demuxer_->GetStartTime());
  EXPECT_EQ(base::Time(), demuxer_->GetTimelineOffset());
  EXPECT_EQ(0u, demuxer_->GetMemoryUsage());
  EXPECT_EQ(absl::nullopt, demuxer_->GetContainerForMetrics());
}

}  // namespace media
