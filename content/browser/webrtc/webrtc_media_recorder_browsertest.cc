// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"

namespace {

static const char kMediaRecorderHtmlFile[] = "/media/mediarecorder_test.html";

static struct EncodingParameters {
  bool disable_accelerator;
  std::string mime_type;
} const kEncodingParameters[] = {
    {true, "video/webm;codecs=vp8"},
    {true, "video/webm;codecs=vp9"},
    {false, ""},  // Instructs the platform to choose any accelerated codec.
    {false, "video/webm;codecs=vp8"},
    {false, "video/webm;codecs=vp9"},
};

static const EncodingParameters kProprietaryEncodingParameters[] = {
    {true, "video/x-matroska;codecs=avc1"},
    {false, "video/x-matroska;codecs=avc1"},
    {true, "video/mp4;codecs=avc1"},
    {false, "video/mp4;codecs=avc1"},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    {true, "video/x-matroska;codecs=hvc1.1.6.L186.B0"},
    {false, "video/x-matroska;codecs=hvc1.1.6.L186.B0"},
    {true, "video/mp4;codecs=hvc1.1.6.L186.B0"},
    {false, "video/mp4;codecs=hvc1.1.6.L186.B0"},
#endif
};

}  // namespace

namespace content {

// This class tests the recording of a media stream.
class WebRtcMediaRecorderTest
    : public WebRtcContentBrowserTestBase,
      public testing::WithParamInterface<struct EncodingParameters> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);

    AppendUseFakeUIForMediaStreamFlag();

    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    if (GetParam().disable_accelerator) {
      command_line->AppendSwitch(switches::kDisableAcceleratedVideoEncode);
    }

    scoped_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
            media::kMediaRecorderHEVCSupport
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, Start) {
  MakeTypicalCall("testStartAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartAndStop) {
  MakeTypicalCall("testStartStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartAndDataAvailable) {
  MakeTypicalCall(base::StringPrintf("testStartAndDataAvailable(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartWithTimeSlice) {
  MakeTypicalCall(base::StringPrintf("testStartWithTimeSlice(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, Resume) {
  MakeTypicalCall("testResumeAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, NoResumeWhenRecorderInactive) {
  MakeTypicalCall("testIllegalResumeThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, ResumeAndDataAvailable) {
  MakeTypicalCall(base::StringPrintf("testResumeAndDataAvailable(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, Pause) {
  MakeTypicalCall("testPauseAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, PauseStop) {
  MakeTypicalCall("testPauseStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       PausePreventsDataavailableFromBeingFired) {
  MakeTypicalCall("testPausePreventsDataavailableFromBeingFired();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, IllegalPauseThrowsDOMError) {
  MakeTypicalCall("testIllegalPauseThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, TwoChannelAudioRecording) {
  MakeTypicalCall("testTwoChannelAudio();", kMediaRecorderHtmlFile);
}

#if BUILDFLAG(IS_MAC)
// TODO(https://crbug.com/379271425): Re-enable once flakiness is addressed.
#define MAYBE_RecordWithTransparency DISABLED_RecordWithTransparency
#else
#define MAYBE_RecordWithTransparency RecordWithTransparency
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_RecordWithTransparency) {
  MakeTypicalCall(base::StringPrintf("testRecordWithTransparency(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       IllegalStartWhileRecordingThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInRecordingStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       IllegalStartWhilePausedThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInPausedStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       IllegalRequestDataThrowsDOMError) {
  MakeTypicalCall("testIllegalRequestDataThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

#if BUILDFLAG(IS_ANDROID)
// These tests are flakily timing out on emulators (https://crbug.com/716691)
// and/or under Android ASAN (https://crbug.com/693565);
#define MAYBE_PeerConnection DISABLED_PeerConnection
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    defined(THREAD_SANITIZER)
// Flaky on Linux TSan, https://crbug.com/694373.
#define MAYBE_PeerConnection DISABLED_PeerConnection
#elif BUILDFLAG(IS_WIN) && !defined(NDEBUG)
// Fails on Win7 debug, https://crbug.com/703844.
#define MAYBE_PeerConnection DISABLED_PeerConnection
#elif BUILDFLAG(IS_MAC)
// Fails on Mac, https://crbug.com/1222675
#define MAYBE_PeerConnection DISABLED_PeerConnection
#elif BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_X86_64)
// Flaky on Fuchsia-x64, https://crbug.com/1408820
#define MAYBE_PeerConnection DISABLED_PeerConnection
#else
#define MAYBE_PeerConnection PeerConnection
#endif

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_PeerConnection) {
  MakeTypicalCall(base::StringPrintf("testRecordRemotePeerConnection(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

// Flaky on Linux Tsan (crbug.com/736268)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AddingTrackToMediaStreamFiresErrorEvent \
  DISABLED_AddingTrackToMediaStreamFiresErrorEvent
#elif BUILDFLAG(IS_ANDROID)
// Flaky on Android (crbug.com/1174634).
#define MAYBE_AddingTrackToMediaStreamFiresErrorEvent \
  DISABLED_AddingTrackToMediaStreamFiresErrorEvent
#else
#define MAYBE_AddingTrackToMediaStreamFiresErrorEvent \
  AddingTrackToMediaStreamFiresErrorEvent
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       MAYBE_AddingTrackToMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testAddingTrackToMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       RemovingTrackFromMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testRemovingTrackFromMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

INSTANTIATE_TEST_SUITE_P(OpenCodec,
                         WebRtcMediaRecorderTest,
                         testing::ValuesIn(kEncodingParameters));

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

INSTANTIATE_TEST_SUITE_P(ProprietaryCodec,
                         WebRtcMediaRecorderTest,
                         testing::ValuesIn(kProprietaryEncodingParameters));

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace content
