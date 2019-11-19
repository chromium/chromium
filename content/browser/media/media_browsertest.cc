// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_browsertest.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_util.h"

namespace content {

#if defined(OS_ANDROID)
// Title set by android cleaner page after short timeout.
const char kClean[] = "CLEAN";
#endif

void MediaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);
  command_line->AppendSwitch(switches::kExposeInternalsForTesting);

  std::vector<base::Feature> disabled_features = {
    // Disable fallback after decode error to avoid unexpected test pass on
    // the fallback path.
    media::kFallbackAfterDecodeError,

#if defined(OS_LINUX)
    // Disable out of process audio on Linux due to process spawn
    // failures. http://crbug.com/986021
    features::kAudioServiceOutOfProcess,
#endif
  };

  scoped_feature_list_.InitWithFeatures({/* enabled_features */},
                                        disabled_features);
}

void MediaBrowserTest::RunMediaTestPage(const std::string& html_page,
                                        const base::StringPairs& query_params,
                                        const std::string& expected_title,
                                        bool http) {
  GURL gurl;
  std::string query = media::GetURLQueryString(query_params);
  std::unique_ptr<net::EmbeddedTestServer> http_test_server;
  if (http) {
    http_test_server.reset(new net::EmbeddedTestServer);
    http_test_server->ServeFilesFromSourceDirectory(media::GetTestDataPath());
    CHECK(http_test_server->Start());
    gurl = http_test_server->GetURL("/" + html_page + "?" + query);
  } else {
    gurl = content::GetFileUrlWithQuery(media::GetTestDataFilePath(html_page),
                                        query);
  }
  std::string final_title = RunTest(gurl, expected_title);
  EXPECT_EQ(expected_title, final_title);
}

std::string MediaBrowserTest::RunTest(const GURL& gurl,
                                      const std::string& expected_title) {
  VLOG(0) << "Running test URL: " << gurl;
  TitleWatcher title_watcher(shell()->web_contents(),
                             base::ASCIIToUTF16(expected_title));
  AddTitlesToAwait(&title_watcher);
  EXPECT_TRUE(NavigateToURL(shell(), gurl));
  base::string16 result = title_watcher.WaitAndGetTitle();

  CleanupTest();
  return base::UTF16ToASCII(result);
}

void MediaBrowserTest::CleanupTest() {
#if defined(OS_ANDROID)
  // We only do this cleanup on Android, as a workaround for a test-only OOM
  // bug. See http://crbug.com/727542
  const base::string16 cleaner_title = base::ASCIIToUTF16(kClean);
  TitleWatcher clean_title_watcher(shell()->web_contents(), cleaner_title);
  GURL cleaner_url = content::GetFileUrlWithQuery(
      media::GetTestDataFilePath("cleaner.html"), "");
  EXPECT_TRUE(NavigateToURL(shell(), cleaner_url));
  base::string16 cleaner_result = clean_title_watcher.WaitAndGetTitle();
  EXPECT_EQ(cleaner_result, cleaner_title);
#endif
}

std::string MediaBrowserTest::EncodeErrorMessage(
    const std::string& original_message) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(original_message.data(), original_message.size(),
                          &buffer);
  return std::string(buffer.data(), buffer.length());
}

void MediaBrowserTest::AddTitlesToAwait(content::TitleWatcher* title_watcher) {
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kEnded));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kError));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kErrorEvent));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kFailed));
}

// Tests playback and seeking of an audio or video file over file or http based
// on a test parameter.  Test starts with playback, then, after X seconds or the
// ended event fires, seeks near end of file; see player.html for details.  The
// test completes when either the last 'ended' or an 'error' event fires.
class MediaTest : public testing::WithParamInterface<bool>,
                  public MediaBrowserTest {
 public:
  // Play specified audio over http:// or file:// depending on |http| setting.
  void PlayAudio(const std::string& media_file, bool http) {
    PlayMedia("audio", media_file, http);
  }

  // Play specified video over http:// or file:// depending on |http| setting.
  void PlayVideo(const std::string& media_file, bool http) {
    PlayMedia("video", media_file, http);
  }

  void PlayMedia(const std::string& tag,
                 const std::string& media_file,
                 bool http) {
    base::StringPairs query_params;
    query_params.emplace_back(tag, media_file);
    RunMediaTestPage("player.html", query_params, media::kEnded, http);
  }

  void RunErrorMessageTest(const std::string& tag,
                           const std::string& media_file,
                           const std::string& expected_error_substring,
                           bool http) {
    base::StringPairs query_params;
    query_params.emplace_back(tag, media_file);
    query_params.emplace_back("error_substr",
                              EncodeErrorMessage(expected_error_substring));
    RunMediaTestPage("player.html", query_params, media::kErrorEvent, http);
  }

  void RunVideoSizeTest(const char* media_file, int width, int height) {
    std::string expected;
    expected += base::NumberToString(width);
    expected += " ";
    expected += base::NumberToString(height);
    base::StringPairs query_params;
    query_params.emplace_back("video", media_file);
    RunMediaTestPage("player.html", query_params, expected, false);
  }
};

// Android doesn't support Theora.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearTheora) {
  PlayVideo("bear.ogv", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv", GetParam());
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWebm) {
  PlayVideo("bear.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusWebm) {
  PlayVideo("bear-opus.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusMp4) {
  PlayVideo("bear-opus.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusOgg) {
  PlayVideo("bear-opus.ogg", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm", GetParam());
}

// We don't expect android devices to support highbit yet.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearHighBitDepthVP9) {
  PlayVideo("bear-320x180-hi10p-vp9.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear12DepthVP9) {
  PlayVideo("bear-320x180-hi12p-vp9.webm", GetParam());
}
#endif

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Vp9) {
  PlayVideo("bear-320x240-v_frag-vp9.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlacMp4) {
  PlayAudio("bear-flac.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlac192kHzMp4) {
  PlayAudio("bear-flac-192kHz.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS16be) {
  PlayVideo("bear_pcm_s16be.mov", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS24be) {
  PlayVideo("bear_pcm_s24be.mov", GetParam());
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4) {
  PlayVideo("bear.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentMp4) {
  PlayVideo("bear_silent.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated0) {
  RunVideoSizeTest("bear_rotate_0.mp4", 1280, 720);
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated90) {
  RunVideoSizeTest("bear_rotate_90.mp4", 720, 1280);
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated180) {
  RunVideoSizeTest("bear_rotate_180.mp4", 1280, 720);
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated270) {
  RunVideoSizeTest("bear_rotate_270.mp4", 720, 1280);
}

#if !defined(OS_ANDROID)
// Android devices usually only support baseline, main and high.
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearHighBitDepthMp4) {
  PlayVideo("bear-320x180-hi10p.mp4", GetParam());
}

// Android can't reliably load lots of videos on a page.
// See http://crbug.com/749265
IN_PROC_BROWSER_TEST_F(MediaTest, LoadManyVideos) {
  base::StringPairs query_params;
  RunMediaTestPage("load_many_videos.html", query_params, media::kEnded, true);
}
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Mpeg4) {
  PlayVideo("bear_mpeg4_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Mpeg4Asp) {
  PlayVideo("bear_mpeg4asp_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Divx) {
  PlayVideo("bear_divx_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAacH264) {
  PlayVideo("bear_h264_aac.3gp", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAmrnbMpeg4) {
  PlayVideo("bear_mpeg4_amrnb.3gp", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavGsmms) {
  PlayAudio("bear_gsm_ms.wav", GetParam());
}
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlac) {
  PlayAudio("bear.flac", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlacOgg) {
  PlayVideo("bear-flac.ogg", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavAlaw) {
  PlayAudio("bear_alaw.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavMulaw) {
  PlayAudio("bear_mulaw.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm) {
  PlayAudio("bear_pcm.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm3kHz) {
  PlayAudio("bear_3kHz.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm192kHz) {
  PlayAudio("bear_192kHz.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoTulipWebm) {
  PlayVideo("tulip2.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorMissingResource) {
  RunErrorMessageTest("video", "nonexistent_file.webm",
                      "MEDIA_ELEMENT_ERROR: Format error", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorEmptySrcAttribute) {
  RunErrorMessageTest("video", "", "MEDIA_ELEMENT_ERROR: Empty src attribute",
                      GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorNoSupportedStreams) {
  RunErrorMessageTest(
      "video", "no_streams.webm",
      "DEMUXER_ERROR_NO_SUPPORTED_STREAMS: FFmpegDemuxer: no supported streams",
      GetParam());
}

// Covers tear-down when navigating away as opposed to browser exiting.
IN_PROC_BROWSER_TEST_F(MediaTest, Navigate) {
  PlayVideo("bear.webm", false);
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

INSTANTIATE_TEST_SUITE_P(File, MediaTest, ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(Http, MediaTest, ::testing::Values(true));

}  // namespace content
