// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_browsertest.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/gpu_mojo_media_client_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_util.h"

// Proprietary codecs require acceleration on Android.
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#define REQUIRE_ACCELERATION_ON_ANDROID() \
  if (!is_accelerated())                  \
  return
#else
#define REQUIRE_ACCELERATION_ON_ANDROID()
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

#if BUILDFLAG(IS_ANDROID)
// Title set by android cleaner page after short timeout.
const char16_t kClean[] = u"CLEAN";
#endif

void MediaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);
  command_line->AppendSwitch(switches::kExposeInternalsForTesting);

  std::vector<base::test::FeatureRef> enabled_features = {
    media::kBuiltInH264Decoder,

#if BUILDFLAG(IS_ANDROID)
    features::kLogJsConsoleMessages,
#endif

#if BUILDFLAG(ENABLE_HLS_DEMUXER) && BUILDFLAG(USE_PROPRIETARY_CODECS)
    media::kBuiltInHlsPlayer,
#endif
  };

  std::vector<base::test::FeatureRef> disabled_features = {
    // Disable fallback after decode error to avoid unexpected test pass on
    // the fallback path.
    media::kFallbackAfterDecodeError,

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Disable out of process audio on Linux due to process spawn
    // failures. http://crbug.com/986021
    features::kAudioServiceOutOfProcess,
#endif
  };

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

void MediaBrowserTest::RunMediaTestPage(const std::string& html_page,
                                        const base::StringPairs& query_params,
                                        const std::string& expected_title,
                                        bool http) {
  GURL gurl;
  std::string query = media::GetURLQueryString(query_params);
  std::unique_ptr<net::EmbeddedTestServer> http_test_server;
  if (http) {
    http_test_server = std::make_unique<net::EmbeddedTestServer>();
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
  std::u16string result = title_watcher.WaitAndGetTitle();

  CleanupTest();
  return base::UTF16ToASCII(result);
}

void MediaBrowserTest::CleanupTest() {
#if BUILDFLAG(IS_ANDROID)
  // We only do this cleanup on Android, as a workaround for a test-only OOM
  // bug. See http://crbug.com/727542
  const std::u16string cleaner_title = kClean;
  TitleWatcher clean_title_watcher(shell()->web_contents(), cleaner_title);
  GURL cleaner_url = content::GetFileUrlWithQuery(
      media::GetTestDataFilePath("cleaner.html"), "");
  EXPECT_TRUE(NavigateToURL(shell(), cleaner_url));
  std::u16string cleaner_result = clean_title_watcher.WaitAndGetTitle();
  EXPECT_EQ(cleaner_result, cleaner_title);
#endif
}

std::string MediaBrowserTest::EncodeErrorMessage(
    const std::string& original_message) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(original_message, &buffer);
  return std::string(buffer.view());
}

void MediaBrowserTest::AddTitlesToAwait(content::TitleWatcher* title_watcher) {
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kEndedTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kErrorTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kErrorEventTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kFailedTitle));
}

void MediaBrowserTest::PreRunTestOnMainThread() {
  ContentBrowserTest::PreRunTestOnMainThread();
  media::AddSupplementalCodecsForTesting(GetGpuPreferencesFromCommandLine());
}

// Tests playback and seeking of an audio or video file. Test starts with
// playback then, after X seconds or the ended event fires, seeks near end of
// file; see player.html for details. The test completes when either the last
// 'ended' or an 'error' event fires.
class MediaTest : public testing::WithParamInterface<bool>,
                  public MediaBrowserTest {
 public:
  bool is_accelerated() { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (!is_accelerated())
      command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
    MediaBrowserTest::SetUpCommandLine(command_line);
  }

  void MaybePlayVideo(std::string_view codec_string,
                      const std::string& file_name) {
    constexpr char kTestVideoPlaybackScript[] = R"({
      const video = document.createElement('video');
      video.canPlayType('video/mp4; codecs=$1') === 'probably';
    })";
    EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
    content::WebContents* web_contents = shell()->web_contents();
    bool result =
        EvalJs(web_contents, JsReplace(kTestVideoPlaybackScript, codec_string))
            .ExtractBool();
    if (!result) {
      return;
    }

    PlayVideo(file_name);
  }

  // Play specified audio over http:// or file:// depending on |http| setting.
  void PlayAudio(const std::string& media_file, bool http = true) {
    PlayMedia("audio", media_file, http);
  }

  // Play specified video over http:// or file:// depending on |http| setting.
  void PlayVideo(const std::string& media_file, bool http = true) {
    PlayMedia("video", media_file, http);
  }

  void PlayMedia(const std::string& tag,
                 const std::string& media_file,
                 bool http) {
    base::StringPairs query_params;
    query_params.emplace_back(tag, media_file);
    RunMediaTestPage("player.html", query_params, media::kEndedTitle, http);
  }

  void RunErrorMessageTest(const std::string& tag,
                           const std::string& media_file,
                           const std::string& expected_error_substring) {
    base::StringPairs query_params;
    query_params.emplace_back(tag, media_file);
    query_params.emplace_back("error_substr",
                              EncodeErrorMessage(expected_error_substring));
    RunMediaTestPage("player.html", query_params, media::kErrorEventTitle,
                     true);
  }

  void RunVideoSizeTest(const char* media_file, int width, int height) {
    std::string expected_title = std::string(media::kEndedTitle) + " " +
                                 base::NumberToString(width) + " " +
                                 base::NumberToString(height);
    base::StringPairs query_params;
    query_params.emplace_back("video", media_file);
    query_params.emplace_back("sizetest", "true");
    RunMediaTestPage("player.html", query_params, expected_title, true);
  }
};

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWebm) {
  PlayVideo("bear.webm");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWebm_FileProtocol) {
  PlayVideo("bear.webm", false);
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusWebm) {
  PlayAudio("bear-opus.webm");
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusMp4) {
  PlayAudio("bear-opus.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusOgg) {
  PlayAudio("bear-opus.ogg");
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearOpusOgg_FileProtocol) {
  PlayAudio("bear-opus.ogg", false);
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm");
}

// We don't expect android devices to support highbit yet.
#if !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40242077): DEMUXER_ERROR_NO_SUPPORTED_STREAMS error on
// Fuchsia Arm64.
#if BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64)
#define MAYBE_VideoBearHighBitDepthVP9 DISABLED_VideoBearHighBitDepthVP9
#else
#define MAYBE_VideoBearHighBitDepthVP9 VideoBearHighBitDepthVP9
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_VideoBearHighBitDepthVP9) {
  PlayVideo("bear-320x180-hi10p-vp9.webm");
}

// TODO(crbug.com/40242077): DEMUXER_ERROR_NO_SUPPORTED_STREAMS error on
// Fuchsia Arm64.
#if BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64)
#define MAYBE_VideoBear12DepthVP9 DISABLED_VideoBear12DepthVP9
#else
#define MAYBE_VideoBear12DepthVP9 VideoBear12DepthVP9
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_VideoBear12DepthVP9) {
  // Hardware decode on does not reliably support 12-bit.
  if (is_accelerated())
    return;
  PlayVideo("bear-320x180-hi12p-vp9.webm");
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Vp9) {
  PlayVideo("bear-320x240-v_frag-vp9.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlacMp4) {
  PlayAudio("bear-flac.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlac192kHzMp4) {
  PlayAudio("bear-flac-192kHz.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS16be) {
  PlayAudio("bear_pcm_s16be.mov");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS24be) {
  PlayAudio("bear_pcm_s24be.mov");
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_HLS_DEMUXER)

IN_PROC_BROWSER_TEST_P(MediaTest, HLSSingleFileBear) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("bear-1280x720-hls-clear-mpl.m3u8");
}

IN_PROC_BROWSER_TEST_P(MediaTest, HLSMultivariantBitrateBear) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("hls/multi-bitrate-multivariant-bear/playlist.m3u8");
}

#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("bear.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentMp4) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("bear_silent.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearRotated0) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_0.mp4", 1280, 720);
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearRotated90) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_90.mp4", 720, 1280);
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearRotated180) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_180.mp4", 1280, 720);
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearRotated270) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_270.mp4", 720, 1280);
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAacH264) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("bear_h264_aac.3gp");
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
// HEVC video stream with 8-bit 422 range extension profile
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Hevc8bit422) {
  MaybePlayVideo("hev1.4.10.L93.9d.8",
                 "bear-1280x720-hevc-8bit-422-no-audio.mp4");
}

// HEVC video stream with 8-bit 444 range extension profile
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Hevc8bit444) {
  MaybePlayVideo("hev1.4.10.L93.9e.8",
                 "bear-1280x720-hevc-8bit-444-no-audio.mp4");
}

// HEVC video stream with 10-bit 422 range extension profile
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Hevc10bit422) {
  MaybePlayVideo("hev1.4.10.L93.9d.8",
                 "bear-1280x720-hevc-10bit-422-no-audio.mp4");
}

// HEVC video stream with 10-bit 444 range extension profile
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Hevc10bit444) {
  MaybePlayVideo("hev1.4.10.L93.9c.8",
                 "bear-1280x720-hevc-10bit-444-no-audio.mp4");
}

// HEVC video stream with 8-bit main profile
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Hevc8bit) {
  // TODO(crbug.com/40269930) : For Android, the `canPlayType()` test in
  // `MaybePlayVideo` should be reporting the correct status for HEVC. The below
  // `REQUIRE_ACCELERATION_ON_ANDROID` flag is a temporary fix.
  REQUIRE_ACCELERATION_ON_ANDROID();
  MaybePlayVideo("hev1.1.6.L93.90", "bear-1280x720-hevc-no-audio.mp4");
}

// HEVC video stream with 10-bit main10 profile
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4Hevc10bit) {
  // TODO(crbug.com/40269930) : For Android, the `canPlayType()` test in
  // `MaybePlayVideo` should be reporting the correct status for HEVC. The below
  // `REQUIRE_ACCELERATION_ON_ANDROID` flag is a temporary fix.
  REQUIRE_ACCELERATION_ON_ANDROID();
  MaybePlayVideo("hev1.2.4.L93.90", "bear-1280x720-hevc-10bit-no-audio.mp4");
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

// Android devices usually only support baseline, main and high.
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearHighBitDepthMp4) {
  PlayVideo("bear-320x180-hi10p.mp4");
}

#endif

// Android can't reliably load lots of videos on a page.
// See http://crbug.com/749265
// TODO(crbug.com/40774322): Flaky on Mac.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_LoadManyVideos DISABLED_LoadManyVideos
#else
#define MAYBE_LoadManyVideos LoadManyVideos
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_LoadManyVideos) {
  // Only run this test in one configuration.
  if (is_accelerated())
    return;
  base::StringPairs query_params;
  RunMediaTestPage("load_many_videos.html", query_params, media::kEndedTitle,
                   true);
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlac) {
  PlayAudio("bear.flac");
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioBearFlacOgg) {
  PlayAudio("bear-flac.ogg");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavAlaw) {
  PlayAudio("bear_alaw.wav");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavMulaw) {
  PlayAudio("bear_mulaw.wav");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm) {
  PlayAudio("bear_pcm.wav");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm3kHz) {
  PlayAudio("bear_3kHz.wav");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm192kHz) {
  PlayAudio("bear_192kHz.wav");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoTulipWebm) {
  PlayVideo("tulip2.webm");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoEbu3213Primary) {
  PlayVideo("ebu-3213-e-vp9.mp4");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorMissingResource) {
  RunErrorMessageTest("video", "nonexistent_file.webm",
                      "MEDIA_ELEMENT_ERROR: Format error");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorEmptySrcAttribute) {
  RunErrorMessageTest("video", "", "MEDIA_ELEMENT_ERROR: Empty src attribute");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorNoSupportedStreams) {
  RunErrorMessageTest("video", "no_streams.webm",
                      "DEMUXER_ERROR_NO_SUPPORTED_STREAMS: FFmpegDemuxer: no "
                      "supported streams");
}

// Covers tear-down when navigating away as opposed to browser exiting.
IN_PROC_BROWSER_TEST_P(MediaTest, Navigate) {
  PlayVideo("bear.webm");
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

IN_PROC_BROWSER_TEST_P(MediaTest, AudioOnly_XHE_AAC_MP4) {
  if (media::IsSupportedAudioType(
          {media::AudioCodec::kAAC, media::AudioCodecProfile::kXHE_AAC})) {
    PlayAudio("noise-xhe-aac.mp4");
  }
}

INSTANTIATE_TEST_SUITE_P(Default, MediaTest, ::testing::Bool());

}  // namespace content
