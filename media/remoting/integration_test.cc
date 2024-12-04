// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "media/base/test_data_util.h"
#include "media/remoting/end2end_test_renderer.h"
#include "media/test/pipeline_integration_test_base.h"
#include "media/test/test_media_source.h"

namespace media {
namespace remoting {

constexpr int kAppendTimeSec = 1;

class MediaRemotingIntegrationTest : public testing::Test,
                                     public PipelineIntegrationTestBase {
 public:
  MediaRemotingIntegrationTest() {
    SetCreateRendererCB(base::BindRepeating(
        &MediaRemotingIntegrationTest::CreateEnd2EndTestRenderer,
        base::Unretained(this)));
  }

  MediaRemotingIntegrationTest(const MediaRemotingIntegrationTest&) = delete;
  MediaRemotingIntegrationTest& operator=(const MediaRemotingIntegrationTest&) =
      delete;

 private:
  std::unique_ptr<Renderer> CreateEnd2EndTestRenderer(
      std::optional<RendererType> renderer_type) {
    return std::make_unique<End2EndTestRenderer>(
        this->CreateRendererImpl(renderer_type));
  }
};

TEST_F(MediaRemotingIntegrationTest, BasicPlayback) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm", TestTypeFlags::kHashed));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_EQ("f0be120a90a811506777c99a2cdf7cc1", GetVideoHash());
  EXPECT_EQ("-3.59,-2.06,-0.43,2.15,0.77,-0.95,", GetAudioHash().ToString());
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
  ASSERT_TRUE(source.AppendAtTime(base::Seconds(kAppendTimeSec),
                                  second_file->AsSpan()));
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
