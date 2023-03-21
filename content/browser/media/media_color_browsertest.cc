// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

  void RunColorTest(const std::string& video_file) {
    GURL base_url = embedded_test_server()->GetURL("/blackwhite.html");

    GURL::Replacements replacements;
    replacements.SetQueryStr(video_file);
    GURL test_url = base_url.ReplaceComponents(replacements);

    std::string final_title = RunTest(test_url, media::kEndedTitle);
    EXPECT_EQ(media::kEndedTitle, final_title);
  }
  void SetUp() override {
    EnablePixelOutput();
    MediaBrowserTest::SetUp();
  }
};

// Android doesn't support Theora.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv420pTheora) {
  RunColorTest("yuv420p.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv422pTheora) {
  RunColorTest("yuv422p.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv444pTheora) {
  RunColorTest("yuv444p.ogv");
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv420pVp8) {
  RunColorTest("yuv420p.webm");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv444pVp9) {
  RunColorTest("yuv444p.webm");
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
  RunColorTest("yuv420p.mp4");
}

// This test fails on Android: http://crbug.com/647818
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Yuvj420pH264 DISABLED_Yuvj420pH264
#else
#define MAYBE_Yuvj420pH264 Yuvj420pH264
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuvj420pH264) {
  RunColorTest("yuvj420p.mp4");
}

// This fails on ChromeOS: http://crbug.com/647400,
// This fails on Android: http://crbug.com/938320,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
#define MAYBE_Yuv420pRec709H264 DISABLED_Yuv420pRec709H264
#else
#define MAYBE_Yuv420pRec709H264 Yuv420pRec709H264
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuv420pRec709H264) {
  RunColorTest("yuv420p_rec709.mp4");
}

// Android doesn't support 10bpc.
// This test flakes on mac: http://crbug.com/810908
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_Yuv420pHighBitDepth DISABLED_Yuv420pHighBitDepth
#else
#define MAYBE_Yuv420pHighBitDepth Yuv420pHighBitDepth
#endif
IN_PROC_BROWSER_TEST_F(MediaColorTest, MAYBE_Yuv420pHighBitDepth) {
  RunColorTest("yuv420p_hi10p.mp4");
}

// Android devices usually only support baseline, main and high.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv422pH264) {
  RunColorTest("yuv422p.mp4");
}

IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv444pH264) {
  RunColorTest("yuv444p.mp4");
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(MediaColorTest, Yuv420pMpeg4) {
  RunColorTest("yuv420p.avi");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace content
