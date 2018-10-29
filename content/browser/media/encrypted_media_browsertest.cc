// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "third_party/libaom/av1_buildflags.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM) && !BUILDFLAG(ENABLE_LIBRARY_CDMS)
// When mojo CDM is enabled, External Clear Key is supported in //content/shell/
// by using mojo CDM with AesDecryptor running in the remote (e.g. GPU or
// Browser) process. When pepper CDM is supported, External Clear Key is
// supported in chrome/, which is tested in browser_tests.
#define SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL
#endif

namespace content {

// Available key systems.
const char kClearKeyKeySystem[] = "org.w3.clearkey";

#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
#endif

// EME-specific test results and errors.
const char kEmeKeyError[] = "KEYERROR";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";

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
            CurrentSourceType(), media::kEnded);
  }

  void TestConfigChange(ConfigChangeType config_change_type) {
    // TODO(xhwang): Even when config change is not supported we still start
    // content shell only to return directly here. We probably should not run
    // these test cases at all.
    if (CurrentSourceType() != SrcType::MSE) {
      DVLOG(0) << "Config change only happens when using MSE.";
      return;
    }

    base::StringPairs query_params;
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back(
        "configChangeType",
        base::IntToString(static_cast<int>(config_change_type)));
    RunMediaTestPage("mse_config_change.html", query_params, media::kEnded,
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
    if (src_type == SrcType::MSE)
      query_params.emplace_back("useMSE", "1");
    RunMediaTestPage(html_page, query_params, expectation, true);
  }

  void RunSimplePlaybackTest(const std::string& media_file,
                             const std::string& key_system,
                             SrcType src_type) {
    RunTest(kDefaultEmePlayer, media_file, key_system, src_type, media::kEnded);
  }

  void RunMultipleFileTest(const std::string& video_file,
                           const std::string& audio_file,
                           const std::string& expected_title) {
    if (CurrentSourceType() != SrcType::MSE) {
      DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
      return;
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
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeKeyError));
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

INSTANTIATE_TEST_CASE_P(SRC_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem),
                                Values(SrcType::SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem),
                                Values(SrcType::MSE)));

#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SrcType::SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SrcType::MSE)));
#endif

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a_enc-a.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_Fullsample) {
  TestSimplePlayback("bear-320x240-v-vp9_fullsample_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_Subsample) {
  TestSimplePlayback("bear-320x240-v-vp9_subsample_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-a_enc-a.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4_FLAC) {
  RunMultipleFileTest(std::string(), "bear-flac-cenc.mp4", media::kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_VP9) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-320x240-v_frag-vp9-cenc.mp4");
}

// TODO(crbug.com/707127): Decide when it's supported on Android.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_VideoOnly_WebM_VP9Profile2) {
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_VP9Profile2) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4");
}
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_AV1_DECODER)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1) {
  TestSimplePlayback("bear-av1-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1_10bit) {
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_AV1) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-av1-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_AV1_10bit) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.mp4");
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

// Strictly speaking this is not an "encrypted" media test. Keep it here for
// completeness.
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_ClearToClear) {
  TestConfigChange(ConfigChangeType::CLEAR_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_ClearToEncrypted) {
  TestConfigChange(ConfigChangeType::CLEAR_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_EncryptedToClear) {
  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       ConfigChangeVideo_EncryptedToEncrypted) {
  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
#if defined(OS_ANDROID)
  // https://crbug.com/778245
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_KITKAT) {
    DVLOG(0) << "Skipping test - FrameSizeChange is flaky on KitKat devices.";
    return;
  }
#endif

  TestFrameSizeChange();
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CENC) {
  RunMultipleFileTest("bear-640x360-v_frag-cenc.mp4",
                      "bear-640x360-a_frag-cenc.mp4", media::kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CBC1) {
  RunMultipleFileTest("bear-640x360-v_frag-cbc1.mp4", std::string(),
                      media::kError);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CENS) {
  RunMultipleFileTest("bear-640x360-v_frag-cens.mp4", std::string(),
                      media::kError);
}

#if !defined(OS_ANDROID)
// TODO(crbug.com/813845): Enable CBCS support on Chrome for Android.
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CBCS) {
  std::string expected_result =
      BUILDFLAG(ENABLE_CBCS_ENCRYPTION_SCHEME) ? media::kEnded : media::kError;
  RunMultipleFileTest("bear-640x360-v_frag-cbcs.mp4",
                      "bear-640x360-a_frag-cbcs.mp4", expected_result);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_Encryption_CBCS_Video_CENC_Audio) {
  std::string expected_result =
      BUILDFLAG(ENABLE_CBCS_ENCRYPTION_SCHEME) ? media::kEnded : media::kError;
  RunMultipleFileTest("bear-640x360-v_frag-cbcs.mp4",
                      "bear-640x360-a_frag-cenc.mp4", expected_result);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_Encryption_CENC_Video_CBCS_Audio) {
  std::string expected_result =
      BUILDFLAG(ENABLE_CBCS_ENCRYPTION_SCHEME) ? media::kEnded : media::kError;
  RunMultipleFileTest("bear-640x360-v_frag-cenc.mp4",
                      "bear-640x360-a_frag-cbcs.mp4", expected_result);
}
#endif  // !defined(OS_ANDROID)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, UnknownKeySystemThrowsException) {
  RunTest(kDefaultEmePlayer, "bear-a_enc-a.webm", "com.example.foo",
          SrcType::MSE, kEmeNotSupportedError);
}

}  // namespace content
