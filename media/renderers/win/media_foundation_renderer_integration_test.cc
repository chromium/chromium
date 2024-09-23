// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/win/media_foundation_renderer.h"

#include <mfapi.h>

#include <memory>

#include "base/win/windows_version.h"
#include "media/base/media_util.h"
#include "media/base/supported_types.h"
#include "media/test/pipeline_integration_test_base.h"
#include "media/test/test_media_source.h"

namespace media {

namespace {

using VideoCodecMap = base::flat_map<VideoCodec, GUID>;

const VideoCodecMap& GetVideoCodecsMap() {
  static const base::NoDestructor<VideoCodecMap> AllVideoCodecsMap(
      {{VideoCodec::kVP9, MFVideoFormat_VP90},
       {VideoCodec::kHEVC, MFVideoFormat_HEVC}});
  return *AllVideoCodecsMap;
}

// TODO(xhwang): Generalize this to support more codecs, or use CanPlay() or
// IsTypeSupported() which can take mime types directly.
bool CanDecodeVideoCodec(VideoCodec codec) {
  auto codecs = GetVideoCodecsMap();
  MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Video, codecs[codec]};
  IMFActivate** activates = nullptr;
  UINT32 count = 0;

  if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                       MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT |
                           MFT_ENUM_FLAG_HARDWARE,
                       &input_type, /*output_type=*/nullptr, &activates,
                       &count))) {
    return false;
  }

  for (UINT32 i = 0; i < count; ++i) {
    activates[i]->Release();
  }
  CoTaskMemFree(activates);

  if (count == 0) {
    LOG(WARNING) << "No decoder for " << media::GetCodecName(codec);
    return false;
  }

  return true;
}

}  // namespace

class MediaFoundationRendererIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  MediaFoundationRendererIntegrationTest() {
    SetCreateRendererCB(base::BindRepeating(
        &MediaFoundationRendererIntegrationTest::CreateMediaFoundationRenderer,
        base::Unretained(this)));
  }

  MediaFoundationRendererIntegrationTest(
      const MediaFoundationRendererIntegrationTest&) = delete;
  MediaFoundationRendererIntegrationTest& operator=(
      const MediaFoundationRendererIntegrationTest&) = delete;

 private:
  std::unique_ptr<Renderer> CreateMediaFoundationRenderer(
      std::optional<RendererType> /*renderer_type*/) {
    LUID empty_luid{0, 0};
    auto renderer = std::make_unique<MediaFoundationRenderer>(
        task_environment_.GetMainThreadTaskRunner(),
        std::make_unique<NullMediaLog>(), empty_luid,
        /*force_dcomp_mode_for_testing=*/true);
    return renderer;
  }
};

TEST_F(MediaFoundationRendererIntegrationTest, BasicPlayback) {
  // TODO(crbug.com/40194343): This test is very flaky on win10-20h2.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN10_20H2) {
    GTEST_SKIP() << "Skipping test for WIN10_20H2 and greater";
  }
  if (!CanDecodeVideoCodec(VideoCodec::kVP9)) {
    return;
  }

  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(MediaFoundationRendererIntegrationTest, BasicPlayback_MediaSource) {
  // TODO(crbug.com/40194343): This test is very flaky on win10-20h2.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN10_20H2) {
    GTEST_SKIP() << "Skipping test for WIN10_20H2 and greater";
  }
  if (!CanDecodeVideoCodec(VideoCodec::kVP9)) {
    return;
  }

  TestMediaSource source("bear-vp9.webm", 67504);
  EXPECT_EQ(PIPELINE_OK, StartPipelineWithMediaSource(&source));
  source.EndOfStream();

  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  source.Shutdown();
  Stop();
}

TEST_F(MediaFoundationRendererIntegrationTest,
       HEVCPlayback_with_FFMpegDemuxer) {
  if (!CanDecodeVideoCodec(VideoCodec::kHEVC)) {
    return;
  }

  // FFMpegDemuxer will verify if video codec is supported internally, add HEVC
  // profile here to let demuxer initialization successfully.
  media::UpdateDefaultSupportedVideoProfiles(
      {media::VideoCodecProfile::HEVCPROFILE_MAIN});

  ASSERT_EQ(PIPELINE_OK, Start("bear-3840x2160-hevc.mp4", kUnreliableDuration));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  Stop();
}

TEST_F(MediaFoundationRendererIntegrationTest, ChangePlaybackRate) {
  // Test changing the playback rate.
  if (!CanDecodeVideoCodec(VideoCodec::kVP9)) {
    GTEST_SKIP() << "ChangePlaybackRate test requires VP9 decoder.";
  }

  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9.webm"));
  // Check the default playback rate.
  ASSERT_EQ(0.0f, pipeline_->GetPlaybackRate());
  // Start playback at rate 2.0.
  pipeline_->SetPlaybackRate(2.0);
  EXPECT_EQ(2.0f, pipeline_->GetPlaybackRate());
  ASSERT_TRUE(WaitUntilOnEnded());
  Stop();
}

TEST_F(MediaFoundationRendererIntegrationTest, ChangeVolume) {
  // Test changing the volume.
  if (!CanDecodeVideoCodec(VideoCodec::kVP9)) {
    GTEST_SKIP() << "ChangeVolume test requires VP9 decoder.";
  }

  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9.webm"));
  // Check default volume
  ASSERT_EQ(1.0f, pipeline_->GetVolume());
  // Change volume and verify
  pipeline_->SetVolume(0.5f);
  EXPECT_EQ(0.5f, pipeline_->GetVolume());
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  Stop();
}

}  // namespace media
