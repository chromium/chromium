// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <memory>

#include <mfapi.h>

#include "base/win/windows_version.h"
#include "media/base/media_util.h"
#include "media/test/pipeline_integration_test_base.h"
#include "media/test/test_media_source.h"

namespace media {

namespace {

// TODO(xhwang): Generalize this to support more codecs, or use CanPlay() or
// IsTypeSupported() which can take mime types directly.
bool CanDecodeVp9() {
  if (!MediaFoundationRenderer::IsSupported()) {
    LOG(WARNING) << "MediaFoundationRenderer not supported";
    return false;
  }

  MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Video, MFVideoFormat_VP90};
  IMFActivate** activates = nullptr;
  UINT32 count = 0;

  if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                       MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT |
                           MFT_ENUM_FLAG_HARDWARE,
                       &input_type, /*output_type=*/nullptr, &activates,
                       &count))) {
    return false;
  }

  for (UINT32 i = 0; i < count; ++i)
    activates[i]->Release();
  CoTaskMemFree(activates);

  if (count == 0) {
    LOG(WARNING) << "No decoder for VP9";
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
      absl::optional<RendererType> /*renderer_type*/) {
    LUID empty_luid{0, 0};
    auto renderer = std::make_unique<MediaFoundationRenderer>(
        task_environment_.GetMainThreadTaskRunner(),
        std::make_unique<NullMediaLog>(), empty_luid,
        /*force_dcomp_mode_for_testing=*/true);
    return renderer;
  }
};

TEST_F(MediaFoundationRendererIntegrationTest, BasicPlayback) {
  // TODO(crbug.com/1240681): This test is very flaky on win10-20h2.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN10_20H2) {
    GTEST_SKIP() << "Skipping test for WIN10_20H2 and greater";
  }
  if (!CanDecodeVp9())
    return;

  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(MediaFoundationRendererIntegrationTest, BasicPlayback_MediaSource) {
  // TODO(crbug.com/1240681): This test is very flaky on win10-20h2.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN10_20H2) {
    GTEST_SKIP() << "Skipping test for WIN10_20H2 and greater";
  }
  if (!CanDecodeVp9())
    return;

  TestMediaSource source("bear-vp9.webm", 67504);
  EXPECT_EQ(PIPELINE_OK, StartPipelineWithMediaSource(&source));
  source.EndOfStream();

  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  source.Shutdown();
  Stop();
}

}  // namespace media
