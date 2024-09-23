// Copyright 2016 The Chromium Authors
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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "media/webrtc/webrtc_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

const char kAudioConstraints[] = "audio: {echoCancellation: {exact: false}}";
const char kVideoConstraints[] = "video:true";

namespace content {

// This class tests the scenario when permission to access mic or camera is
// granted.
class WebRtcAudioBrowserTest : public WebRtcContentBrowserTestBase {
 public:
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

  std::string BuildConstraints(const char* audio, const char* video) {
    DCHECK(audio);
    DCHECK(video);

    std::string audio_str(audio);
    std::string video_str(video);
    if (!audio_str.empty() && !video_str.empty())
      return "{" + audio_str + "," + video_str + "}";
    if (!audio_str.empty())
      return "{" + audio_str + "}";

    return "{" + video_str + "}";
  }

 private:
  base::test::ScopedFeatureList audio_service_features_;
};

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       CanMakeVideoCallAndThenRenegotiateToAudio) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  std::string audio_only_constraints = BuildConstraints(kAudioConstraints, "");
  MakeAudioDetectingPeerConnectionCall("callAndRenegotiateToAudio(" +
                                       constraints + ", " +
                                       audio_only_constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndEnsureAudioIsPlaying) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall("callAndEnsureAudioIsPlaying(" +
                                       constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EstablishAudioOnlyCallAndEnsureAudioIsPlaying) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall("callAndEnsureAudioIsPlaying(" +
                                       constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyRemoteMutingWorks) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteAudioTrackMutingWorks(" + constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyLocalMutingWorks) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalAudioTrackMutingWorks(" + constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EnsureLocalVideoMuteDoesntMuteAudio) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalVideoMutingDoesntMuteAudio(" + constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EnsureRemoteVideoMuteDoesntMuteAudio) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteVideoMutingDoesntMuteAudio(" + constraints + ");");
}

IN_PROC_BROWSER_TEST_F(WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyUnmutingWorks) {
  std::string constraints =
      BuildConstraints(kAudioConstraints, kVideoConstraints);
  MakeAudioDetectingPeerConnectionCall("callAndEnsureAudioTrackUnmutingWorks(" +
                                       constraints + ");");
}

// TODO(crbug.com/40637961): This test is a temporary replacement for:
// external/wpt/webrtc/RTCRtpReceiver-getSynchronizationSources.https.html
IN_PROC_BROWSER_TEST_F(
    WebRtcAudioBrowserTest,
    EstablishAudioOnlyCallAndVerifyGetSynchronizationSourcesWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "testEstablishAudioOnlyCallAndVerifyGetSynchronizationSourcesWorks();");
}

}  // namespace content
