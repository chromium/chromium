// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

// External Clear Key is a test-only key system that has mostly the same
// functionality as Clear Key key system. Unlike Clear Key, which is implemented
// by AesDecryptor in the render process directly, External Clear Key is
// implemented by hosting a CDM that supports Clear Key (e.g. AesDecryptor) in a
// remote processes to cover the code path used by a real production CDM, which
// is otherwise hard to cover by tests.
// - When ENABLE_LIBRARY_CDMS is true, a "Clear Key CDM" that implements the
//   "Library CDM API" is hosted in the CDM/utility process to do decryption and
//   decoding. This covers MojoCdm, MojoDecryptor, CdmAdapter, CdmFileIO etc.
//   See //media/cdm/library_cdm/clear_key_cdm/README.md.
// - Otherwise when ENABLE_MOJO_CDM is true, External Clear Key is supported in
//   content/shell/ by using MojoCdm with AesDecryptor running in a remote
//   process, e.g. GPU or Browser, as specified by |mojo_media_host|. The
//   connection between the media pipeline and the CDM varies on different
//   platforms. For example, the media pipeline could choose the default
//   RendererImpl in the render process, which can use the remote CDM to do
//   decryption via MojoDecryptor. The media pipeline could also choose
//   MojoRenderer, which hosts a RendererImpl in the remote process, which uses
//   the Decryptor exposed by the AesDecryptor directly in the remote process.
//   See TestMojoMediaClient for details on this path.

// TODO (b/263310318) Enable on Android when Clear Key issues on Android are
// fixed.
#if BUILDFLAG(ENABLE_MOJO_CDM) && !BUILDFLAG(ENABLE_LIBRARY_CDMS)
#define SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL
#endif

namespace content {

// EME-specific test results and errors.
const char16_t kEmeKeyError[] = u"KEYERROR";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";
const char16_t kEmeNotSupportedError16[] = u"NOTSUPPORTEDERROR";

const char kDefaultEmePlayer[] = "eme_player.html";
const char kDefaultMseOnlyEmePlayer[] = "mse_different_containers.html";

// The type of video src used to load media.
enum class SrcType { SRC, MSE };

// Must be in sync with CONFIG_CHANGE_TYPE in eme_player_js/global.js
enum class ConfigChangeType {
  CLEAR_TO_CLEAR = 0,
  CLEAR_TO_ENCRYPTED = 1,
  ENCRYPTED_TO_CLEAR = 2,
  ENCRYPTED_TO_ENCRYPTED = 3,
};

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - SrcType: The type of video src used to load media, MSE or SRC.
// It is okay to run this test as a non-parameterized test, in this case,
// GetParam() should not be called.
class EncryptedMediaTest
    : public MediaBrowserTest,
      public testing::WithParamInterface<std::tuple<const char*, SrcType>> {
 public:
  // Can only be used in parameterized (*_P) tests.
  const std::string CurrentKeySystem() { return std::get<0>(GetParam()); }

  // Can only be used in parameterized (*_P) tests.
  SrcType CurrentSourceType() { return std::get<1>(GetParam()); }

  void TestSimplePlayback(const std::string& encrypted_media) {
    RunSimplePlaybackTest(encrypted_media, CurrentKeySystem(),
                          CurrentSourceType());
  }

  void TestFrameSizeChange() {
    RunTest("encrypted_frame_size_change.html",
            "frame_size_change-av_enc-v.webm", CurrentKeySystem(),
            CurrentSourceType(), media::kEndedTitle);
  }

  void TestConfigChange(ConfigChangeType config_change_type) {
    // TODO(xhwang): Even when config change is not supported we still start
    // content shell only to return directly here. We probably should not run
    // these test cases at all.
    if (CurrentSourceType() != SrcType::MSE) {
      GTEST_SKIP() << "Config change only happens when using MSE.";
    }

    base::StringPairs query_params;
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back(
        "configChangeType",
        base::NumberToString(static_cast<int>(config_change_type)));
    RunMediaTestPage("mse_config_change.html", query_params, media::kEndedTitle,
                     true);
  }

  void RunTest(const std::string& html_page,
               const std::string& media_file,
               const std::string& key_system,
               SrcType src_type,
               const std::string& expectation) {
    base::StringPairs query_params;
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType",
                              media::GetMimeTypeForFile(media_file));
    query_params.emplace_back("keySystem", key_system);
    if (src_type == SrcType::MSE) {
      query_params.emplace_back("useMSE", "1");
    }
    RunMediaTestPage(html_page, query_params, expectation, true);
  }

  void RunSimplePlaybackTest(const std::string& media_file,
                             const std::string& key_system,
                             SrcType src_type) {
    RunTest(kDefaultEmePlayer, media_file, key_system, src_type,
            media::kEndedTitle);
  }

  void RunMultipleFileTest(const std::string& video_file,
                           const std::string& audio_file,
                           const std::string& expected_title) {
    if (CurrentSourceType() != SrcType::MSE) {
      GTEST_SKIP() << "Can only play MP4 encrypted streams by MSE.";
    }

    base::StringPairs query_params;
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back("runEncrypted", "1");
    if (!video_file.empty()) {
      query_params.emplace_back("videoFile", video_file);
      query_params.emplace_back("videoFormat",
                                media::GetMimeTypeForFile(video_file));
    }
    if (!audio_file.empty()) {
      query_params.emplace_back("audioFile", audio_file);
      query_params.emplace_back("audioFormat",
                                media::GetMimeTypeForFile(audio_file));
    }

    RunMediaTestPage(kDefaultMseOnlyEmePlayer, query_params, expected_title,
                     true);
  }

 protected:
  // We want to fail quickly when a test fails because an error is encountered.
  void AddTitlesToAwait(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddTitlesToAwait(title_watcher);
    title_watcher->AlsoWaitForTitle(kEmeNotSupportedError16);
    title_watcher->AlsoWaitForTitle(kEmeKeyError);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaBrowserTest::SetUpCommandLine(command_line);
#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
    scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                          {});
#endif
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

using ::testing::Combine;
using ::testing::Values;

INSTANTIATE_TEST_SUITE_P(SRC_ClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kClearKeyKeySystem),
                                 Values(SrcType::SRC)));

INSTANTIATE_TEST_SUITE_P(MSE_ClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kClearKeyKeySystem),
                                 Values(SrcType::MSE)));

#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
INSTANTIATE_TEST_SUITE_P(SRC_ExternalClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kExternalClearKeyKeySystem),
                                 Values(SrcType::SRC)));

INSTANTIATE_TEST_SUITE_P(MSE_ExternalClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kExternalClearKeyKeySystem),
                                 Values(SrcType::MSE)));
#endif

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a_enc-a.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm");
}

// TODO(crbug.com/40784898): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Playback_VideoAudio_WebM DISABLED_Playback_VideoAudio_WebM
#else
#define MAYBE_Playback_VideoAudio_WebM Playback_VideoAudio_WebM
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, MAYBE_Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v_enc-v.webm");
}

// TODO(crbug.com/40116008): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       DISABLED_Playback_VideoOnly_WebM_Fullsample) {
  TestSimplePlayback("bear-320x240-v-vp9_fullsample_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_Subsample) {
  TestSimplePlayback("bear-320x240-v-vp9_subsample_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm");
}

// TODO(crbug.com/40784898): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Playback_VideoAudio_WebM_Opus \
  DISABLED_Playback_VideoAudio_WebM_Opus
#else
#define MAYBE_Playback_VideoAudio_WebM_Opus Playback_AudioOnly_WebM_Opus
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_Playback_VideoAudio_WebM_Opus) {
#if BUILDFLAG(IS_ANDROID)
  if (!media::MediaCodecUtil::IsOpusDecoderAvailable()) {
    GTEST_SKIP() << "Opus decoder not available";
  }
#endif
  TestSimplePlayback("bear-320x240-opus-a_enc-a.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
#if BUILDFLAG(IS_ANDROID)
  if (!media::MediaCodecUtil::IsOpusDecoderAvailable()) {
    GTEST_SKIP() << "Opus decoder not available";
  }
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm");
}

// TODO(crbug.com/40863269): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Playback_VideoClearAudio_WebM_Opus \
  DISABLED_Playback_VideoClearAudio_WebM_Opus
#else
#define MAYBE_Playback_VideoClearAudio_WebM_Opus \
  Playback_VideoClearAudio_WebM_Opus
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_Playback_VideoClearAudio_WebM_Opus) {
#if BUILDFLAG(IS_ANDROID)
  if (!media::MediaCodecUtil::IsOpusDecoderAvailable()) {
    GTEST_SKIP() << "Opus decoder not available";
  }
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4_FLAC) {
  RunMultipleFileTest(std::string(), "bear-flac-cenc.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4_OPUS) {
#if BUILDFLAG(IS_ANDROID)
  if (!media::MediaCodecUtil::IsOpusDecoderAvailable()) {
    GTEST_SKIP() << "Opus decoder not available";
  }
#endif
  RunMultipleFileTest(std::string(), "bear-opus-cenc.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_VP9) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    GTEST_SKIP() << "Can only play MP4 encrypted streams by MSE.";
  }

  TestSimplePlayback("bear-320x240-v_frag-vp9-cenc.mp4");
}

// TODO(crbug.com/40513452): Decide when it's supported on Android.
#if !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM_FAMILY))
// TODO(crbug.com/40187305): Failing on Mac.
// TODO(crbug.com/40208879): Failing on Fuchsia arm.
#define MAYBE_Playback_VideoOnly_WebM_VP9Profile2 \
  DISABLED_Playback_VideoOnly_WebM_VP9Profile2
#else
#define MAYBE_Playback_VideoOnly_WebM_VP9Profile2 \
  Playback_VideoOnly_WebM_VP9Profile2
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_Playback_VideoOnly_WebM_VP9Profile2) {
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.webm");
}

#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM_FAMILY))
// TODO(crbug.com/40805145): Failing on Mac.
// TODO(crbug.com/40208879): Failing on Fuchsia arm.
#define MAYBE_Playback_VideoOnly_MP4_VP9Profile2 \
  DISABLED_Playback_VideoOnly_MP4_VP9Profile2
#else
#define MAYBE_Playback_VideoOnly_MP4_VP9Profile2 \
  Playback_VideoOnly_MP4_VP9Profile2
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_Playback_VideoOnly_MP4_VP9Profile2) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    GTEST_SKIP() << "Can only play MP4 encrypted streams by MSE.";
  }

  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4");
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_AV1_DECODER)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1) {
  TestSimplePlayback("bear-av1-cenc.webm");
}

// TODO(crbug.com/40863206): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Playback_VideoOnly_WebM_AV1_10bit \
  DISABLED_Playback_VideoOnly_WebM_AV1_10bit
#else
#define MAYBE_Playback_VideoOnly_WebM_AV1_10bit \
  Playback_VideoOnly_WebM_AV1_10bit
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_Playback_VideoOnly_WebM_AV1_10bit) {
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_AV1) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    GTEST_SKIP() << "Can only play MP4 encrypted streams by MSE.";
  }

  TestSimplePlayback("bear-av1-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_AV1_10bit) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    GTEST_SKIP() << "Can only play MP4 encrypted streams by MSE.";
  }

  TestSimplePlayback("bear-av1-320x180-10bit-cenc.mp4");
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

// Strictly speaking this is not an "encrypted" media test. Keep it here for
// completeness.
// TODO(crbug.com/330190697): Flaky on Fuchsia, deflake and re-enable the test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ConfigChangeVideo_ClearToClear \
  DISABLED_ConfigChangeVideo_ClearToClear
#else
#define MAYBE_ConfigChangeVideo_ClearToClear ConfigChangeVideo_ClearToClear
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_ConfigChangeVideo_ClearToClear) {
  TestConfigChange(ConfigChangeType::CLEAR_TO_CLEAR);
}

// Failed on Android, see https://crbug.com/1014540.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ConfigChangeVideo_ClearToEncrypted \
  DISABLED_ConfigChangeVideo_ClearToEncrypted
#else
#define MAYBE_ConfigChangeVideo_ClearToEncrypted \
  ConfigChangeVideo_ClearToEncrypted
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_ConfigChangeVideo_ClearToEncrypted) {
  TestConfigChange(ConfigChangeType::CLEAR_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_EncryptedToClear) {
  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       ConfigChangeVideo_EncryptedToEncrypted) {
  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_ENCRYPTED);
}

// Fails on Android (https://crbug.com/778245 and https://crbug.com/1023638).
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FrameSizeChangeVideo DISABLED_FrameSizeChangeVideo
#else
#define MAYBE_FrameSizeChangeVideo FrameSizeChangeVideo
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, MAYBE_FrameSizeChangeVideo) {
  TestFrameSizeChange();
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CENC) {
  RunMultipleFileTest("bear-640x360-v_frag-cenc.mp4",
                      "bear-640x360-a_frag-cenc.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CBC1) {
  RunMultipleFileTest("bear-640x360-v_frag-cbc1.mp4", std::string(),
                      media::kErrorTitle);
}

// TODO(crbug.com/40863223): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Playback_Encryption_CENS DISABLED_Playback_Encryption_CENS
#else
#define MAYBE_Playback_Encryption_CENS Playback_Encryption_CENS
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, MAYBE_Playback_Encryption_CENS) {
  RunMultipleFileTest("bear-640x360-v_frag-cens.mp4", std::string(),
                      media::kErrorTitle);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CBCS) {
  RunMultipleFileTest("bear-640x360-v_frag-cbcs.mp4",
                      "bear-640x360-a_frag-cbcs.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_Encryption_CBCS_Video_CENC_Audio) {
  RunMultipleFileTest("bear-640x360-v_frag-cbcs.mp4",
                      "bear-640x360-a_frag-cenc.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_Encryption_CENC_Video_CBCS_Audio) {
  RunMultipleFileTest("bear-640x360-v_frag-cenc.mp4",
                      "bear-640x360-a_frag-cbcs.mp4", media::kEndedTitle);
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, UnknownKeySystemThrowsException) {
  RunTest(kDefaultEmePlayer, "bear-a_enc-a.webm", "com.example.foo",
          SrcType::MSE, kEmeNotSupportedError);
}

}  // namespace content
