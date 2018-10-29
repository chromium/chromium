// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "media/webrtc/webrtc_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"

namespace content {

// This class tests the scenario when permission to access mic or camera is
// granted.
class WebRtcAudioBrowserTest : public WebRtcContentBrowserTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  WebRtcAudioBrowserTest() {
    std::vector<base::Feature> audio_service_oop_features = {
        features::kAudioServiceAudioStreams,
        features::kAudioServiceOutOfProcess};
    if (GetParam()) {
      // Force audio service out of process to enabled.
      audio_service_features_.InitWithFeatures(audio_service_oop_features, {});
    } else {
      // Force audio service out of process to disabled.
      audio_service_features_.InitWithFeatures({}, audio_service_oop_features);
    }
  }
  ~WebRtcAudioBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  // Convenience method for making calls that detect if audio os playing (which
  // has some special prerequisites, such that there needs to be an audio output
  // device on the executing machine).
  void MakeAudioDetectingPeerConnectionCall(const std::string& javascript) {
    if (!HasAudioOutputDevices()) {
      // Bots with no output devices will force the audio code into a state
      // where it doesn't manage to set either the low or high latency path.
      // This test will compute useless values in that case, so skip running on
      // such bots (see crbug.com/326338).
      LOG(INFO) << "Missing output devices: skipping test...";
      return;
    }

    ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream))
        << "Must run with fake devices since the test will explicitly look "
        << "for the fake device signal.";

    MakeTypicalCall(javascript, "/media/peerconnection-call-audio.html");
  }

 private:
  base::test::ScopedFeatureList audio_service_features_;
};

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       CanMakeVideoCallAndThenRenegotiateToAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndRenegotiateToAudio({audio: true, video:true}, {audio: true});");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioIsPlaying({audio:true, video:true});");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EstablishAudioOnlyCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioIsPlaying({audio:true});");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EstablishIsac16KCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callWithIsac16KAndEnsureAudioIsPlaying({audio:true});");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyRemoteMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteAudioTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyLocalMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalAudioTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EnsureLocalVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalVideoMutingDoesntMuteAudio();");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EnsureRemoteVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteVideoMutingDoesntMuteAudio();");
}

IN_PROC_BROWSER_TEST_P(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyUnmutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioTrackUnmutingWorks();");
}

// We run these tests with the audio service both in and out of the the browser
// process to have waterfall coverage while the feature rolls out. It should be
// removed after launch.
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
// Supported platforms.
INSTANTIATE_TEST_CASE_P(, WebRtcAudioBrowserTest, ::testing::Bool());
#elif defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#else
// Platforms where the out of process audio service isn't supported
INSTANTIATE_TEST_CASE_P(, WebRtcAudioBrowserTest, ::testing::Values(false));
#endif

}  // namespace content
