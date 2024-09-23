// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

class MediaSourceTest : public MediaBrowserTest {
 public:
  void TestSimplePlayback(const std::string& media_file,
                          const std::string& media_type,
                          const std::string& expectation) {
    base::StringPairs query_params;
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType", media_type);
    RunMediaTestPage("media_source_player.html", query_params, expectation,
                     true);
  }

  void TestSimplePlayback(const std::string& media_file,
                          const std::string& expectation) {
    TestSimplePlayback(media_file, media::GetMimeTypeForFile(media_file),
                       expectation);
  }

  base::StringPairs GetAudioVideoQueryParams(const std::string& audio_file,
                                             const std::string& video_file) {
    base::StringPairs params;
    params.emplace_back("audioFile", audio_file);
    params.emplace_back("audioFormat", media::GetMimeTypeForFile(audio_file));
    params.emplace_back("videoFile", video_file);
    params.emplace_back("videoFormat", media::GetMimeTypeForFile(video_file));
    return params;
  }
};

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240.webm", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-video-only.webm", media::kEndedTitle);
}

// TODO(servolk): Android is supposed to support AAC in ADTS container with
// 'audio/aac' mime type, but for some reason playback fails on trybots due to
// some issue in OMX AAC decoder (crbug.com/528361)
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_AAC_ADTS) {
  TestSimplePlayback("sfx.adts", media::kEndedTitle);
}
#endif

// Opus is not supported in Android as of now.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_Opus_WebM) {
  TestSimplePlayback("bear-opus.webm", media::kEndedTitle);
}
#endif

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-320x240-audio-only.webm", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_MP3) {
  TestSimplePlayback("sfx.mp3", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(
    MediaSourceTest,
    Playback_AudioOnly_MP3_With_Codecs_Parameter_Should_Fail) {
  // We override the correct media type for this file with one which erroneously
  // includes a codecs parameter that is valid for progressive but invalid for
  // MSE type support.
  DCHECK_EQ(media::GetMimeTypeForFile("sfx.mp3"), "audio/mpeg");
  TestSimplePlayback("sfx.mp3", "audio/mpeg; codecs=\"mp3\"",
                     media::kFailedTitle);
}

// Test the case where test file and mime type mismatch.
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Type_Error) {
  const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMAudioOnly,
                     media::kErrorEventTitle);
}

// Flaky test crbug.com/246308
// Test changed to skip checks resulting in flakiness. Proper fix still needed.
// TODO(crbug.com/330132631): Flaky on Fuchsia, deflake and re-enable the test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ConfigChangeVideo DISABLED_ConfigChangeVideo
#else
#define MAYBE_ConfigChangeVideo ConfigChangeVideo
#endif
IN_PROC_BROWSER_TEST_F(MediaSourceTest, MAYBE_ConfigChangeVideo) {
  RunMediaTestPage("mse_config_change.html", base::StringPairs(),
                   media::kEndedTitle, true);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Video_MP4_Audio_WEBM) {
  auto query_params = GetAudioVideoQueryParams("bear-320x240-audio-only.webm",
                                               "bear-640x360-v_frag.mp4");
  RunMediaTestPage("mse_different_containers.html", std::move(query_params),
                   media::kEndedTitle, true);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Video_WEBM_Audio_MP4) {
  auto query_params = GetAudioVideoQueryParams("bear-640x360-a_frag.mp4",
                                               "bear-320x240-video-only.webm");
  RunMediaTestPage("mse_different_containers.html", std::move(query_params),
                   media::kEndedTitle, true);
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_FLAC_MP4) {
  TestSimplePlayback("bear-flac_frag.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_XHE_AAC_MP4) {
  if (media::IsSupportedAudioType(
          {media::AudioCodec::kAAC, media::AudioCodecProfile::kXHE_AAC})) {
    TestSimplePlayback("noise-xhe-aac.mp4", media::kEndedTitle);
  }
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioVideo_Mp2t) {
  TestSimplePlayback("bear-1280x720.ts", media::kEndedTitle);
}
#endif
#endif

}  // namespace content
