// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"

namespace content {

class MediaColorTest : public MediaBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        media::GetTestDataPath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void RunColorTest(const std::string& video_file,
                    const std::string& html_path) {
    GURL base_url = embedded_test_server()->GetURL(html_path);

    GURL::Replacements replacements;
    replacements.SetQueryStr(video_file);
    GURL test_url = base_url.ReplaceComponents(replacements);

    std::string final_title = RunTest(test_url, media::kEndedTitle);
    EXPECT_EQ(media::kEndedTitle, final_title);
  }
  void RunBlackWhiteTest(const std::string& video_file) {
    RunColorTest(video_file, "/blackwhite.html");
  }
  void RunGBRPTest(const std::string& video_file) {
    RunColorTest(video_file, "/gbrp.html");
  }
  void SetUp() override {
    EnablePixelOutput();
    MediaBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv420pVp8) {
  RunBlackWhiteTest("yuv420p.webm");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv444pVp9) {
  RunBlackWhiteTest("yuv444p.webm");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, GbrpVp9) {
  RunGBRPTest("vp9.mp4");
}

// Fuchsia doesn't able to playback 4:4:4 av1.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_GbrpAv1 DISABLED_GbrpAv1
#else
#define MAYBE_GbrpAv1 GbrpAv1
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_GbrpAv1) {
  RunGBRPTest("av1.mp4");
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

// This test fails on Android: http://crbug.com/938320
// It also fails on ChromeOS https://crbug.com/938618
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Yuv420pH264 DISABLED_Yuv420pH264
#else
#define MAYBE_Yuv420pH264 Yuv420pH264
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuv420pH264) {
  RunBlackWhiteTest("yuv420p.mp4");
}

// This test fails on Android: http://crbug.com/647818
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Yuvj420pH264 DISABLED_Yuvj420pH264
#else
#define MAYBE_Yuvj420pH264 Yuvj420pH264
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuvj420pH264) {
  RunBlackWhiteTest("yuvj420p.mp4");
}

// This fails on ChromeOS: http://crbug.com/647400,
// This fails on Android: http://crbug.com/938320,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
#define MAYBE_Yuv420pRec709H264 DISABLED_Yuv420pRec709H264
#else
#define MAYBE_Yuv420pRec709H264 Yuv420pRec709H264
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuv420pRec709H264) {
  RunBlackWhiteTest("yuv420p_rec709.mp4");
}

// Android doesn't support 10bpc.
// This test flakes on mac: http://crbug.com/810908
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_Yuv420pHighBitDepth DISABLED_Yuv420pHighBitDepth
#else
#define MAYBE_Yuv420pHighBitDepth Yuv420pHighBitDepth
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuv420pHighBitDepth) {
  RunBlackWhiteTest("yuv420p_hi10p.mp4");
}

// Android devices usually only support baseline, main and high.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv422pH264) {
  RunBlackWhiteTest("yuv422p.mp4");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv444pH264) {
  RunBlackWhiteTest("yuv444p.mp4");
}

// TODO(crbug.com/343014700): Add GbrpH265 test for H265 after resolving
// color space full range issue on macOS, and validate HEVC 4:4:4 + GBR
// video on Windows is working as expected.
IN_PROC_BROWSER_TEST_F(MediaColorTest, GbrpH264) {
  RunGBRPTest("h264.mp4");
}
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace content
