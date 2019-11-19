// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "ui/display/display_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

const char kProbably[] = "probably";
const char kMaybe[] = "maybe";
const char kNot[] = "";

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const char* kPropProbably = kProbably;
const char* kPropMaybe = kMaybe;
#else
const char* kPropProbably = kNot;
const char* kPropMaybe = kNot;
#endif  // USE_PROPRIETARY_CODECS

#if !defined(OS_ANDROID)
const char* kOggVideoProbably = kProbably;
const char* kOggVideoMaybe = kMaybe;
const char* kTheoraProbably = kProbably;
const char* kHlsProbably = kNot;
const char* kHlsMaybe = kNot;
#else
const char* kOggVideoProbably = kNot;
const char* kOggVideoMaybe = kNot;
const char* kTheoraProbably = kNot;
const char* kHlsProbably = kPropProbably;
const char* kHlsMaybe = kPropMaybe;
#endif  // !OS_ANDROID

// Chrome doesn't support HEVC.
const char* kHevcSupported = kNot;

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
const char* kMp2tsMaybe = kPropMaybe;
const char* kMp2tsProbably = kPropProbably;
#else
const char* kMp2tsMaybe = kNot;
const char* kMp2tsProbably = kNot;
#endif

// High 10-bit profile is only available when we can use ffmpeg to decode H.264.
// Even though FFmpeg is used on Android, we only use platform decoders for
// H.264
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
const char* kHi10pProbably = kPropProbably;
#else
const char* kHi10pProbably = kPropMaybe;
#endif

namespace content {

class MediaCanPlayTypeTest : public MediaBrowserTest {
 public:
  MediaCanPlayTypeTest() : url_("about:blank") {}

  void SetUpOnMainThread() override {
    EXPECT_TRUE(NavigateToURL(shell(), url_));
  }

  std::string CanPlay(const std::string& type) {
    std::string command("document.createElement('video').canPlayType(");
    command.append(type);
    command.append(")");

    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        shell(), "window.domAutomationController.send(" + command + ");",
        &result));
    return result;
  }

  void TestMPEGUnacceptableCombinations(const std::string& mime) {
    // AVC codecs must be followed by valid 6-digit hexadecimal number.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.12345\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.12345\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.1234567\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.1234567\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.number\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.number\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.12345.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.12345.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.123456.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.123456.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.123456.7\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.123456.7\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.x23456\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.1x3456\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.12x456\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.123x56\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.1234x6\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.12345x\"'"));

    // Old-style avc1 codecs must be followed by two dot-separated decimal
    // numbers (H.264 profile and level)
    // Invalid formats
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1..\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.30.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.x66.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66x.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.x30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.30x\"'"));
    // Invalid level values
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.300\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.-1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.x\"'"));
    // Invalid profile values
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.0.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.65.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.67.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.76.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.78.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.99.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.101.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.300.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.-1.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.x.30\"'"));
    // Old-style avc1 codec ids are supported only for video/mp2t container.
    if (mime != "video/mp2t") {
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.13\"'"));
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.77.30\"'"));
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.100.40\"'"));
    }
    // Old-style codec ids are not supported for avc3 codec strings.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.66.13\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.77.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.100.40\"'"));

    // AAC codecs must be followed by one or two valid hexadecimal numbers.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.no\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.0k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.0k.0k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.4.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.0k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2.0\"'"));

    // Unlike just "avc1", just "mp4a" is not supported.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.\"'"));

    // Other names for the codecs are not supported.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"h264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"h.264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"H264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"H.264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"aac\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AAC\"'"));

    // Codecs must not end with a dot.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.\"'"));

    // A simple substring match is not sufficient.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"lavc1337\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\";mp4a+\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\";mp4a.40+\"'"));

    // Codecs not belonging to MPEG container.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, 1\"'"));

    // Remove all but "audio/mpeg" when https://crbug.com/592889 is fixed.
    if (mime != "audio/mpeg" && mime != "audio/mp4" && mime != "video/mp4" &&
        mime != "video/mp2t" &&
        !base::EndsWith(mime, "mpegurl", base::CompareCase::SENSITIVE)) {
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp3\"'"));
    }

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, vorbis\"'"));

    if (mime != "audio/mp4" && mime != "video/mp4") {
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"opus\"'"));
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, opus\"'"));
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, opus\"'"));
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, opus\"'"));
      EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, opus\"'"));
    }

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8.0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9.0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp08\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\"vp08.00.01.08.02.01.01.00\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40.02\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.02\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1.4d401e\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3.64001f\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1, MP4\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3, MP4\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC3.64001F, MP4.40.2\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC1.4D401E, MP4.40.02\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC3.64001F, MP4.40.02\"'"));

    // Unknown codecs.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc4\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4ax\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-4\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-4\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a4\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a7\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a5.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a6.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a5.1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a6.1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"unknown\"'"));

    // Don't allow incomplete/ambiguous codec ids for HEVC.
    // Codec string must have info about codec level/profile, as described in
    // ISO/IEC FDIS 14496-15 section E.3, for example "hev1.1.6.L93.B0"
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1\"'"));

    // Invalid codecs that look like something similar to HEVC/H.265
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1x\"'"));
    // First component of codec id must be "hev1" or "hvc1" (case-sensitive)
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hevc.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev0.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc0.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev2.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc2.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"HEVC.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"HEV0.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"HVC0.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"HEV2.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"HVC2.1.6.L93.B0\"'"));

    // Trailing dot is not allowed.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0.\"'"));
    // Invalid general_profile_space/general_profile_idc
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.x.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.x.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.d1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.d1.6.L93.B0\"'"));
    // Invalid general_profile_compatibility_flags
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.x.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.x.L93.B0\"'"));
    // Invalid general_tier_flag
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.x.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.x.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.Lx.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.Lx.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.Hx.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.Hx.B0\"'"));
    // Invalid constraint flags
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.x\"'"));
  }

  void TestOGGUnacceptableCombinations(const std::string& mime) {
    // Codecs not belonging to OGG container.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp08\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\"vp08.00.01.08.02.01.01.00\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9.0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09.00.10.08\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0,opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0,opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp3\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.66\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.67\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.68\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.69\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.6B\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.02\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A6\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a6\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"Theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"Opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"Vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"Theora, Opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"Theora, Vorbis\"'"));

    // Unknown codecs.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"unknown\"'"));
  }

  void TestWEBMUnacceptableCombinations(const std::string& mime) {
    // Codecs not belonging to WEBM container.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8.0, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9.0, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp08\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0,opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0,opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"flac\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp3\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.66\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.67\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.68\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.69\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.6B\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8.0, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9.0, mp4a.40\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A6\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a6\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"VP8, Vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"VP8.0, Opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"VP9, Vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"VP9.0, Opus\"'"));

    // Unknown codec.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"unknown\"'"));
  }

  void TestWAVUnacceptableCombinations(const std::string& mime) {
    // Codecs not belonging to WAV container.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8.0, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9.0, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp08\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09.00.10.08\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.66.30\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0,opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0,opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"flac\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp3\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.66\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.67\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.68\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.69\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.6B\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.02\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A6\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a6\"'"));

    // Unknown codec.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"unknown\"'"));
  }

  void TestHLSCombinations(const std::string& mime) {
    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "'"));

    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"avc1\"'"));
    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"avc3\"'"));
    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"avc1, mp4a.40\"'"));
    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"avc3, mp4a.40\"'"));

    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc1.42E01E\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc1.42101E\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc1.42701E\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc1.42F01E\"'"));

    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc3.42E01E\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc3.42801E\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"avc3.42C01E\"'"));

    // Android, is the only platform that supports these types, and its HLS
    // implementations uses platform codecs, which do not include MPEG-2 AAC.
    // See https://crbug.com/544268.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.66\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.67\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.68\"'"));

    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"mp4a.69\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"mp4a.6B\"'"));

    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"mp4a.40.02\"'"));
    EXPECT_EQ(kHlsProbably,
              CanPlay("'" + mime + "; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
    EXPECT_EQ(kHlsProbably,
              CanPlay("'" + mime + "; codecs=\"avc1.42E01E, mp4a.40.02\"'"));
    EXPECT_EQ(kHlsProbably,
              CanPlay("'" + mime + "; codecs=\"avc3.42E01E, mp4a.40.5\"'"));
    EXPECT_EQ(kHlsProbably,
              CanPlay("'" + mime + "; codecs=\"avc3.42E01E, mp4a.40.05\"'"));
    EXPECT_EQ(kHlsProbably,
              CanPlay("'" + mime + "; codecs=\"avc3.42E01E, mp4a.40.29\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\"hev1.1.6.L93.B0,mp4a.40.5\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\"hvc1.1.6.L93.B0,mp4a.40.5\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp09.00.10.08\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"flac\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ac-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"ec-3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.A6\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a5\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.a6\"'"));

    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"avc1, mp4a.40.2\"'"));
    EXPECT_EQ(kHlsMaybe,
              CanPlay("'" + mime + "; codecs=\"avc1, mp4a.40.02\"'"));
    EXPECT_EQ(kHlsMaybe, CanPlay("'" + mime + "; codecs=\"avc3, mp4a.40.2\"'"));
    EXPECT_EQ(kHlsMaybe,
              CanPlay("'" + mime + "; codecs=\"avc3, mp4a.40.02\"'"));
    EXPECT_EQ(kHlsMaybe,
              CanPlay("'" + mime + "; codecs=\"avc1.42E01E, mp4a.40\"'"));
    EXPECT_EQ(kHlsMaybe,
              CanPlay("'" + mime + "; codecs=\"avc3.42E01E, mp4a.40\"'"));

    TestMPEGUnacceptableCombinations(mime);
    // This result is incorrect. See https://crbug.com/592889.
    EXPECT_EQ(kHlsProbably, CanPlay("'" + mime + "; codecs=\"mp3\"'"));
  }

 private:
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(MediaCanPlayTypeTest);
};

#if BUILDFLAG(ENABLE_AV1_DECODER)
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_av1) {
  // Fully qualified codec strings are required. These tests are not exhaustive
  // since codec string parsing is exhaustively tested elsewhere.
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"av1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"av1\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"av01.0.04M.08\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"av01.0.04M.08\"'"));
}
#else
// AV1 is enabled by default. However, on platforms where it is not built, such
// as ARM-based devices, av1 must be unsupported.
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_av1_unsupported) {
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"av01.0.04M.08\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"av01.0.04M.08\"'"));
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_wav) {
  EXPECT_EQ(kMaybe, CanPlay("'audio/wav'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/wav; codecs=\"1\"'"));

  TestWAVUnacceptableCombinations("audio/wav");

  EXPECT_EQ(kMaybe, CanPlay("'audio/x-wav'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/x-wav; codecs=\"1\"'"));

  TestWAVUnacceptableCombinations("audio/x-wav");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_webm) {
  EXPECT_EQ(kMaybe, CanPlay("'video/webm'"));

  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0, opus\"'"));

  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp9\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp9.0\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp9, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp9.0, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp9, opus\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp9.0, opus\"'"));

  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8, vp9\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0, vp9.0\"'"));

  TestWEBMUnacceptableCombinations("video/webm");

  EXPECT_EQ(kMaybe, CanPlay("'audio/webm'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/webm; codecs=\"vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/webm; codecs=\"opus\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/webm; codecs=\"opus, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0, opus\"'"));

  TestWEBMUnacceptableCombinations("audio/webm");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_ogg) {
  EXPECT_EQ(kOggVideoMaybe, CanPlay("'video/ogg'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"theora, flac\"'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"flac, opus, vorbis\"'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"vp8\"'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"vp8, vorbis\"'"));

  TestOGGUnacceptableCombinations("video/ogg");

  EXPECT_EQ(kMaybe, CanPlay("'audio/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"flac\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"flac, vorbis, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, flac\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, vorbis\"'"));

  TestOGGUnacceptableCombinations("audio/ogg");

  EXPECT_EQ(kMaybe, CanPlay("'application/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'application/ogg; codecs=\"flac\"'"));
  EXPECT_EQ(kProbably, CanPlay("'application/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kProbably, CanPlay("'application/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kProbably,
            CanPlay("'application/ogg; codecs=\"flac, opus, vorbis\"'"));
  EXPECT_EQ(kTheoraProbably, CanPlay("'application/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, flac\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, vorbis\"'"));

  TestOGGUnacceptableCombinations("application/ogg");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_flac) {
  EXPECT_EQ(kProbably, CanPlay("'audio/flac'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"flac\"'"));

  // See CodecSupportTest_mp4 for more flac combos.
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"flac\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"flac\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/flac'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-flac'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-flac'"));
  EXPECT_EQ(kNot, CanPlay("'application/x-flac'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"flac\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"flac\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"flac\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/flac; codecs=\"mp4a.40.02\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_mp3) {
  EXPECT_EQ(kNot, CanPlay("'video/mp3'"));
  EXPECT_EQ(kNot, CanPlay("'video/mpeg'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-mp3'"));

  // audio/mpeg without a codecs parameter (RFC 3003 compliant)
  EXPECT_EQ(kProbably, CanPlay("'audio/mpeg'"));

  // audio/mpeg with mp3 in codecs parameter. (Not RFC compliant, but
  // very common in the wild so it is a defacto standard).
  EXPECT_EQ(kProbably, CanPlay("'audio/mpeg; codecs=\"mp3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.68\"'"));
  // The next two results are wrong due to https://crbug.com/592889.
  EXPECT_EQ(kProbably, CanPlay("'audio/mpeg; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/mpeg; codecs=\"mp4a.6B\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.40.02\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"flac\"'"));

  TestMPEGUnacceptableCombinations("audio/mpeg");

  // audio/mp3 does not allow any codecs parameter
  EXPECT_EQ(kProbably, CanPlay("'audio/mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.6B\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.40.02\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"flac\"'"));

  TestMPEGUnacceptableCombinations("audio/mp3");
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp3\"'"));

  // audio/x-mp3 does not allow any codecs parameter
  EXPECT_EQ(kProbably, CanPlay("'audio/x-mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.6B\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.40.02\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"flac\"'"));

  TestMPEGUnacceptableCombinations("audio/x-mp3");
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp3\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_mp4) {
  EXPECT_EQ(kMaybe, CanPlay("'video/mp4'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, avc3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, flac\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, flac\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, opus\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, opus\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42F01E\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42C01E\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"mp4a.6B\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.29\"'"));

  // AC3 and EAC3 (aka Dolby Digital Plus, DD+) audio codecs. These are not
  // supported by Chrome by default.
  // TODO(servolk): Strictly speaking only mp4a.A5 and mp4a.A6 codec ids are
  // valid according to RFC 6381 section 3.3, 3.4. Lower-case oti (mp4a.a5 and
  // mp4a.a6) should be rejected. But we used to allow those in older versions
  // of Chromecast firmware and some apps (notably MPL) depend on those codec
  // types being supported, so they should be allowed for now (crbug.com/564960)
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"mp4a.a6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"mp4a.A6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"avc1.640028,ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"avc1.640028,mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"avc1.640028,mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"avc1.640028,ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"avc1.640028,mp4a.a6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp4; codecs=\"avc1.640028,mp4a.A6\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40\"'"));

  EXPECT_EQ(kHevcSupported, CanPlay("'video/mp4; codecs=\"hev1.1.6.L93.B0\"'"));
  EXPECT_EQ(kHevcSupported, CanPlay("'video/mp4; codecs=\"hvc1.1.6.L93.B0\"'"));
  EXPECT_EQ(kHevcSupported,
            CanPlay("'video/mp4; codecs=\"hev1.1.6.L93.B0, mp4a.40.5\"'"));
  EXPECT_EQ(kHevcSupported,
            CanPlay("'video/mp4; codecs=\"hvc1.1.6.L93.B0, mp4a.40.5\"'"));

  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"vp09.00.10.08\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"flac\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.4D401E, flac\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.64001F, flac\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"opus\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.4D401E, opus\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.64001F, opus\"'"));

  TestMPEGUnacceptableCombinations("video/mp4");
  // This result is incorrect. See https://crbug.com/592889.
  EXPECT_EQ(kProbably, CanPlay("'video/mp4; codecs=\"mp3\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.42F01E\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.42C01E\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"mp4a.6B\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.29\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"hev1.1.6.L93.B0\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"hvc1.1.6.L93.B0\"'"));
  EXPECT_EQ(kNot,
            CanPlay("'video/x-m4v; codecs=\"hev1.1.6.L93.B0, mp4a.40.5\"'"));
  EXPECT_EQ(kNot,
            CanPlay("'video/x-m4v; codecs=\"hvc1.1.6.L93.B0, mp4a.40.5\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"vp09.00.10.08\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"mp4a.a6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"mp4a.A6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"avc1.640028,ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"avc1.640028,mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"avc1.640028,mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"avc1.640028,ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"avc1.640028,mp4a.a6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"avc1.640028,mp4a.A6\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"flac\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4v; codecs=\"opus\"'"));

  TestMPEGUnacceptableCombinations("video/x-m4v");

  EXPECT_EQ(kMaybe, CanPlay("'audio/mp4'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"mp4a.6B\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.29\"'"));

  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"flac\"'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc3, mp4a.40\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"hev1.1.6.L93.B0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"hvc1.1.6.L93.B0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"hev1.1.6.L93.B0,mp4a.40.5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"hvc1.1.6.L93.B0,mp4a.40.5\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"vp09.00.10.08\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.a6\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.A6\"'"));

  TestMPEGUnacceptableCombinations("audio/mp4");
  // This result is incorrect. See https://crbug.com/592889.
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"mp3\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"mp4a.6B\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.29\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"hev1.1.6.L93.B0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"hvc1.1.6.L93.B0\"'"));
  EXPECT_EQ(kNot,
            CanPlay("'audio/x-m4a; codecs=\"hev1.1.6.L93.B0, mp4a.40.5\"'"));
  EXPECT_EQ(kNot,
            CanPlay("'audio/x-m4a; codecs=\"hvc1.1.6.L93.B0, mp4a.40.5\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"vp09.00.10.08\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"mp4a.a6\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"mp4a.A6\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/x-m4a; codecs=\"flac\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-m4a; codecs=\"opus\"'"));

  TestMPEGUnacceptableCombinations("audio/x-m4a");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Avc1Variants) {
  // avc1 without extensions results in "maybe" for compatibility.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1\"'"));

  // A valid-looking 6-digit hexadecimal number will result in at least "maybe".
  // But the first hex byte after the dot must be a valid profile_idc and the
  // lower two bits of the second byte/4th digit must be zero.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42AC23\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42ACDF\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42acdf\"'"));

  // Invalid profile 0x12.
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc1.123456\"'"));
  // Valid profile/level, but reserved bits are set to 1 (4th digit after dot).
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc1.42011E\"'"));
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc1.42021E\"'"));

  // Both upper and lower case hexadecimal digits are accepted.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42e01e\"'"));

  // From a YouTube DASH MSE test manifest.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4d401f\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4d401e\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4d4015\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.640028\"'"));

  //
  // Baseline Profile (66 == 0x42).
  //  The first two digits after the dot must be 42. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42401E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00A\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc1.42G01E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc1.42000G\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E0FF\"'"));

  //
  // Main Profile (77 == 0x4D).
  //  The first two digits after the dot must be 4D. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4D001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4D400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4D800A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4DE00A\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc1.4DG01E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc1.4D000G\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.4DE0FF\"'"));

  //
  // High Profile (100 == 0x64).
  //  The first two digits after the dot must be 64. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64800A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64E00A\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc1.64G01E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc1.64000G\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.64E0FF\"'"));

  //
  // High 10-bit Profile (110 == 0x6E).
  //  The first two digits after the dot must be 6E. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kHi10pProbably, CanPlay("'video/mp4; codecs=\"avc1.6E001E\"'"));
  EXPECT_EQ(kPropProbably,  CanPlay("'video/mp4; codecs=\"avc1.6E400A\"'"));
  EXPECT_EQ(kPropProbably,  CanPlay("'video/mp4; codecs=\"avc1.6E800A\"'"));
  EXPECT_EQ(kPropProbably,  CanPlay("'video/mp4; codecs=\"avc1.6EE00A\"'"));
  EXPECT_EQ(kNot,           CanPlay("'video/mp4; codecs=\"avc1.6EG01E\"'"));
  EXPECT_EQ(kNot,           CanPlay("'video/mp4; codecs=\"avc1.6E000G\"'"));
  EXPECT_EQ(kPropMaybe,     CanPlay("'video/mp4; codecs=\"avc1.6EE0FF\"'"));

  //
  //  Other profiles are not known to be supported.
  //

  // Extended Profile (88 == 0x58).
  //   Without any constraint flags.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.58001E\"'"));
  //   With constraint_set0_flag==1 indicating compatibility with baseline
  //   profile.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.58801E\"'"));
  //   With constraint_set1_flag==1 indicating compatibility with main profile.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.58401E\"'"));
  //   With constraint_set2_flag==1 indicating compatibility with extended
  //   profile, the result is 'maybe' the same as for straight extended profile.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.58201E\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Avc3Variants) {
  // avc3 without extensions results in "maybe" for compatibility.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3\"'"));

  // A valid-looking 6-digit hexadecimal number will result in at least "maybe".
  // But the first hex byte after the dot must be a valid profile_idc and the
  // lower two bits of the second byte/4th digit must be zero.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.42AC23\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.42ACDF\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.42acdf\"'"));

  // Invalid profile 0x12.
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc3.123456\"'"));
  // Valid profile/level, but reserved bits are set to 1 (4th digit after dot).
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc3.42011E\"'"));
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc3.42021E\"'"));

  // Both upper and lower case hexadecimal digits are accepted.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42e01e\"'"));

  // From a YouTube DASH MSE test manifest.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4d401f\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4d401e\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4d4015\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.640028\"'"));

  //
  // Baseline Profile (66 == 0x42).
  //  The first two digits after the dot must be 42. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42800A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E00A\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc3.42G01E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc3.42000G\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42E0FF\"'"));

  //
  // Main Profile (77 == 0x4D).
  //  The first two digits after the dot must be 4D. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4D001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4D400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4D800A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4DE00A\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc3.4DG01E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc3.4D000G\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.4DE0FF\"'"));

  //
  // High Profile (100 == 0x64).
  //  The first two digits after the dot must be 64. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64800A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64E00A\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc3.64G01E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'video/mp4; codecs=\"avc3.64000G\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.64E0FF\"'"));

  //
  // High 10-bit Profile (110 == 0x6E).
  //  The first two digits after the dot must be 6E. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  EXPECT_EQ(kHi10pProbably, CanPlay("'video/mp4; codecs=\"avc3.6E001E\"'"));
  EXPECT_EQ(kPropProbably,  CanPlay("'video/mp4; codecs=\"avc3.6E400A\"'"));
  EXPECT_EQ(kPropProbably,  CanPlay("'video/mp4; codecs=\"avc3.6E800A\"'"));
  EXPECT_EQ(kPropProbably,  CanPlay("'video/mp4; codecs=\"avc3.6EE00A\"'"));
  EXPECT_EQ(kNot,           CanPlay("'video/mp4; codecs=\"avc3.6EG01E\"'"));
  EXPECT_EQ(kNot,           CanPlay("'video/mp4; codecs=\"avc3.6E000G\"'"));
  EXPECT_EQ(kPropMaybe,     CanPlay("'video/mp4; codecs=\"avc3.6EE0FF\"'"));

  //
  //  Other profiles are not known to be supported.
  //

  // Extended Profile (88 == 0x58).
  //   Without any constraint flags.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.58001E\"'"));
  //   With constraint_set0_flag==1 indicating compatibility with baseline
  //   profile.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.58801E\"'"));
  //   With constraint_set1_flag==1 indicating compatibility with main profile.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.58401E\"'"));
  //   With constraint_set2_flag==1 indicating compatibility with extended
  //   profile, the result is 'maybe' the same as for straight extended profile.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.58201E\"'"));
}

// Tests AVC levels using AVC1 Baseline (0x42E0zz).
// Other supported values for the first four hexadecimal digits should behave
// the same way but are not tested.
// For each full level, the following are tested:
// * The hexadecimal value before it is not supported.
// * The hexadecimal value for the main level and all sub-levels are supported.
// * The hexadecimal value after the last sub-level it is not supported.
// * Decimal representations of the levels are not supported.

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_AvcLevels) {
  // Level 0 is not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E000\"'"));

  // Levels 1 (0x0A), 1.1 (0x0B), 1.2 (0x0C), 1.3 (0x0D).
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E009\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00B\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00C\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00D\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E00E\"'"));
  // Verify that decimal representations of levels are not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E001\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E010\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E011\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E012\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E013\"'"));

  // Levels 2 (0x14), 2.1 (0x15), 2.2 (0x16)
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E013\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E014\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E015\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E016\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E017\"'"));
  // Verify that decimal representations of levels are not supported.
  // However, 20 is level 3.2.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E002\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E020\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E021\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E022\"'"));

  // Levels 3 (0x1e), 3.1 (0x1F), 3.2 (0x20)
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E01D\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01F\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E020\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E021\"'"));
  // Verify that decimal representations of levels are not supported.
  // However, 32 is level 5.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E003\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E030\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E031\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E032\"'"));

  // Levels 4 (0x28), 4.1 (0x29), 4.2 (0x2A)
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E027\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E028\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E029\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E02A\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E02B\"'"));
  // Verify that decimal representations of levels are not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E004\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E040\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E041\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E042\"'"));

  // Levels 5 (0x32), 5.1 (0x33), 5.2 (0x34).
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E031\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E032\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E033\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E034\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E035\"'"));
  // Verify that decimal representations of levels are not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E005\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E050\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E051\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E052\"'"));
}

// All values that return positive results are tested. There are also
// negative tests for values around or that could potentially be confused with
// (e.g. case, truncation, hex <-> deciemal conversion) those values that return
// positive results.
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Mp4aVariants) {
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.60\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.61\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.62\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.63\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.65\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.65\"'"));
  // MPEG2 AAC Main, LC, and SSR are supported.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.68\"'"));
  // MP3.
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6A\"'"));
  // MP3.
  EXPECT_EQ(kProbably, CanPlay("'audio/mp4; codecs=\"mp4a.6B\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6b\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6C\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6D\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6F\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.76\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.4\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.39\"'"));

  // mp4a.40 without further extension is ambiguous and results in "maybe".
  EXPECT_EQ(kPropMaybe,    CanPlay("'audio/mp4; codecs=\"mp4a.40\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.0\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.1\"'"));
  // MPEG4 AAC LC.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.3\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.4\"'"));
  // MPEG4 AAC SBR v1.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.6\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.7\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.8\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.9\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.10\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.20\"'"));
  // MPEG4 AAC SBR PS v2.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.29\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.30\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.40\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.50\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.290\"'"));
  // Check conversions of decimal 29 to hex and hex 29 to decimal.
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.1d\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.1D\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.41\"'"));

  // Allow leading zeros in aud-oti for specific MPEG4 AAC strings.
  // See http://crbug.com/440607.
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.00\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.01\"'"));
  // MPEG4 AAC LC.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.03\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.04\"'"));
  // MPEG4 AAC SBR v1.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.029\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.41\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.41.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.4.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.400.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.040.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.4.5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.400.5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.040.5\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_HLS) {
  TestHLSCombinations("application/vnd.apple.mpegurl");
  TestHLSCombinations("application/x-mpegurl");
  TestHLSCombinations("audio/mpegurl");
  TestHLSCombinations("audio/x-mpegurl");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_AAC_ADTS) {
  EXPECT_EQ(kPropProbably, CanPlay("'audio/aac'"));

  // audio/aac doesn't support the codecs parameter.
  EXPECT_EQ(kNot, CanPlay("'audio/aac; codecs=\"1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/aac; codecs=\"aac\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/aac; codecs=\"mp4a.40.2\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Mpeg2Ts) {
  EXPECT_EQ(kMp2tsMaybe, CanPlay("'video/mp2t'"));

  // video/mp2t must support standard RFC 6381 compliant H.264 / AAC codec ids.
  // H.264 baseline, main, high profiles
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.640028\"'"));

  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp4a.68\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp4a.6B\"'"));

  // AAC LC audio
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp4a.40.2\"'"));
  // H.264 + AAC audio combinations
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.42E01E,mp4a.40.2\"'"));
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.4D401E,mp4a.40.2\"'"));
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.640028,mp4a.40.2\"'"));
  // H.264 + AC3/EAC3 audio combinations
  EXPECT_EQ(kNot, CanPlay("'video/mp2t; codecs=\"avc1.640028,ac-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp2t; codecs=\"avc1.640028,ec-3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp2t; codecs=\"avc1.640028,mp4a.A5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp2t; codecs=\"avc1.640028,mp4a.A6\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp2t; codecs=\"avc1.640028,mp4a.a5\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/mp2t; codecs=\"avc1.640028,mp4a.a6\"'"));

  TestMPEGUnacceptableCombinations("video/mp2t");
  // This result is incorrect. See https://crbug.com/592889.
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"mp3\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest,
                       CodecSupportTest_Mpeg2Ts_LegacyAvc1_codec_ids) {
  // Old-style avc1/H.264 codec ids that are still being used by some HLS
  // streaming apps for backward compatibility.
  // H.264 baseline profile
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.10\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.13\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.20\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.22\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.30\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.32\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.40\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.66.42\"'"));
  // H.264 main profile
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.10\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.13\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.20\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.22\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.30\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.32\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.40\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.77.42\"'"));
  // H.264 high profile
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.10\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.13\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.20\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.22\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.30\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.32\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.40\"'"));
  EXPECT_EQ(kMp2tsProbably, CanPlay("'video/mp2t; codecs=\"avc1.100.42\"'"));

  // H.264 + AAC audio combinations
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.66.10,mp4a.40.2\"'"));
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.66.30,mp4a.40.2\"'"));
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.77.10,mp4a.40.2\"'"));
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.77.30,mp4a.40.2\"'"));
  EXPECT_EQ(kMp2tsProbably,
            CanPlay("'video/mp2t; codecs=\"avc1.100.40,mp4a.40.2\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Mpeg2TsAudio) {
  // audio/mp2t is currently not supported (see also crbug.com/556837).
  EXPECT_EQ(kNot, CanPlay("'audio/mp2t; codecs=\"mp4a.40.2\"'"));
}

// See more complete codec string testing in media/base/video_codecs_unittest.cc
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_NewVp9Variants) {
  const std::string kSupportedMimeTypes[] = {"video/webm", "video/mp4"};
  for (const auto& mime_type : kSupportedMimeTypes) {
// Profile 2 and 3 support is currently disabled on ARM and MIPS.
#if defined(ARCH_CPU_ARM_FAMILY) || defined(ARCH_CPU_MIPS_FAMILY)
#if defined(OS_ANDROID)
    const char* kVP9Profile2And3Probably =
        base::android::BuildInfo::GetInstance()->sdk_int() >=
                base::android::SDK_VERSION_P
            ? kProbably
            : kNot;
#else
    const char* kVP9Profile2And3Probably = kNot;
#endif
#else
    const char* kVP9Profile2And3Probably = kProbably;
#endif

    // E.g. "'video/webm; "
    std::string prefix = "'" + mime_type + "; ";

    // Malformed codecs string never allowed.
    EXPECT_EQ(kNot, CanPlay(prefix + "codecs=\"vp09.00.-1.08\"'"));

    // Test a few valid strings.
    EXPECT_EQ(kProbably, CanPlay(prefix + "codecs=\"vp09.00.10.08\"'"));
    EXPECT_EQ(kProbably,
              CanPlay(prefix + "codecs=\"vp09.00.10.08.00.01.01.01.00\"'"));
    EXPECT_EQ(kProbably,
              CanPlay(prefix + "codecs=\"vp09.00.10.08.01.02.02.02.00\"'"));

    // Profiles 0 and 1 are always supported supported. Profiles 2 and 3 are
    // only supported on certain architectures.
    EXPECT_EQ(kProbably, CanPlay(prefix + "codecs=\"vp09.01.10.08\"'"));
    EXPECT_EQ(kVP9Profile2And3Probably,
              CanPlay(prefix + "codecs=\"vp09.02.10.08\"'"));
    EXPECT_EQ(kVP9Profile2And3Probably,
              CanPlay(prefix + "codecs=\"vp09.03.10.08\"'"));
  }
}

}  // namespace content
