// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "media/base/test_data_util.h"
#include "media/remoting/end2end_test_renderer.h"
#include "media/test/pipeline_integration_test_base.h"
#include "media/test/test_media_source.h"

namespace media {
namespace remoting {

namespace {

constexpr int kAppendTimeSec = 1;

class TestRendererFactory final : public PipelineTestRendererFactory {
 public:
  explicit TestRendererFactory(
      std::unique_ptr<PipelineTestRendererFactory> renderer_factory)
      : default_renderer_factory_(std::move(renderer_factory)) {}
  ~TestRendererFactory() override = default;

  // PipelineTestRendererFactory implementation.
  std::unique_ptr<Renderer> CreateRenderer() override {
    return std::make_unique<End2EndTestRenderer>(
        default_renderer_factory_->CreateRenderer());
  }

 private:
  std::unique_ptr<PipelineTestRendererFactory> default_renderer_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRendererFactory);
};

}  // namespace

class MediaRemotingIntegrationTest : public testing::Test,
                                     public PipelineIntegrationTestBase {
 public:
  MediaRemotingIntegrationTest() {
    std::unique_ptr<PipelineTestRendererFactory> factory =
        std::move(renderer_factory_);
    renderer_factory_.reset(new TestRendererFactory(std::move(factory)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRemotingIntegrationTest);
};

TEST_F(MediaRemotingIntegrationTest, BasicPlayback) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm", TestTypeFlags::kHashed));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_EQ("f0be120a90a811506777c99a2cdf7cc1", GetVideoHash());
  EXPECT_EQ("-3.59,-2.06,-0.43,2.15,0.77,-0.95,", GetAudioHash());
}

TEST_F(MediaRemotingIntegrationTest, BasicPlayback_MediaSource) {
  TestMediaSource source("bear-320x240.webm", 219229);
  EXPECT_EQ(PIPELINE_OK, StartPipelineWithMediaSource(&source));
  source.EndOfStream();

  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  source.Shutdown();
  Stop();
}

TEST_F(MediaRemotingIntegrationTest, MediaSource_ConfigChange_WebM) {
  TestMediaSource source("bear-320x240-16x9-aspect.webm", kAppendWholeFile);
  EXPECT_EQ(PIPELINE_OK, StartPipelineWithMediaSource(&source));

  EXPECT_CALL(*this, OnVideoNaturalSizeChange(gfx::Size(640, 360))).Times(1);
  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360.webm");
  ASSERT_TRUE(source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                                  second_file->data(),
                                  second_file->data_size()));
  source.EndOfStream();

  Play();
  EXPECT_TRUE(WaitUntilOnEnded());

  source.Shutdown();
  Stop();
}

TEST_F(MediaRemotingIntegrationTest, SeekWhilePlaying) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm"));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());
}

}  // namespace remoting
}  // namespace media
