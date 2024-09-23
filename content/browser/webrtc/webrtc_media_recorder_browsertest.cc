// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
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
    {true, "video/webm;codecs=VP8"},
    {true, "video/webm;codecs=VP9"},
    {false, ""},  // Instructs the platform to choose any accelerated codec.
    {false, "video/webm;codecs=VP8"},
    {false, "video/webm;codecs=VP9"},
};

static const EncodingParameters kProprietaryEncodingParameters[] = {
    {true, "video/x-matroska;codecs=AVC1"},
    {false, "video/x-matroska;codecs=AVC1"},
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
  }
};

// TODO(crbug/361123384): Re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Start DISABLED_Start
#else
#define MAYBE_Start Start
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_Start) {
  MakeTypicalCall("testStartAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartAndStop) {
  MakeTypicalCall("testStartStopAndRecorderState();", kMediaRecorderHtmlFile);
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1222675
#define MAYBE_StartAndDataAvailable DISABLED_StartAndDataAvailable
#else
#define MAYBE_StartAndDataAvailable StartAndDataAvailable
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_StartAndDataAvailable) {
  MakeTypicalCall(base::StringPrintf("testStartAndDataAvailable(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

// TODO(crbug.com/40559669): It seems to be flaky on Android. More details in
// the bug.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_StartWithTimeSlice DISABLED_StartWithTimeSlice
#else
#define MAYBE_StartWithTimeSlice StartWithTimeSlice
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_StartWithTimeSlice) {
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

// TODO(crbug.com/40903193): Seems the test is not working quite well on
// android-12l-x64-dbg-tests.
#if (BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)) || BUILDFLAG(IS_ANDROID)
// https://crbug.com/1222675
#define MAYBE_ResumeAndDataAvailable DISABLED_ResumeAndDataAvailable
#else
#define MAYBE_ResumeAndDataAvailable ResumeAndDataAvailable
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_ResumeAndDataAvailable) {
  MakeTypicalCall(base::StringPrintf("testResumeAndDataAvailable(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, Pause) {
  MakeTypicalCall("testPauseAndRecorderState();", kMediaRecorderHtmlFile);
}

// TODO(crbug.com/40450139): Flaky on TSAN bots.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PauseStop DISABLED_PauseStop
#else
#define MAYBE_PauseStop PauseStop
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_PauseStop) {
  MakeTypicalCall("testPauseStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       PausePreventsDataavailableFromBeingFired) {
  MakeTypicalCall("testPausePreventsDataavailableFromBeingFired();",
                  kMediaRecorderHtmlFile);
}

// TODO (crbug.com/736268): Flaky on Linux TSan bots.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_IllegalPauseThrowsDOMError DISABLED_IllegalPauseThrowsDOMError
#else
#define MAYBE_IllegalPauseThrowsDOMError IllegalPauseThrowsDOMError
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       MAYBE_IllegalPauseThrowsDOMError) {
  MakeTypicalCall("testIllegalPauseThrowsDOMError();", kMediaRecorderHtmlFile);
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1222675
#define MAYBE_TwoChannelAudioRecording DISABLED_TwoChannelAudioRecording
#else
#define MAYBE_TwoChannelAudioRecording TwoChannelAudioRecording
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       MAYBE_TwoChannelAudioRecording) {
  MakeTypicalCall("testTwoChannelAudio();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, RecordWithTransparency) {
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

// Flaky on Linux Tsan (crbug.com/736268)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_IllegalRequestDataThrowsDOMError \
  DISABLED_IllegalRequestDataThrowsDOMError
#else
#define MAYBE_IllegalRequestDataThrowsDOMError IllegalRequestDataThrowsDOMError
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest,
                       MAYBE_IllegalRequestDataThrowsDOMError) {
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
#elif BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// Fails on Mac/Arm, https://crbug.com/1222675
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
