// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

namespace content {

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcBrowserTest DISABLED_WebRtcBrowserTest
#else
#define MAYBE_WebRtcBrowserTest WebRtcBrowserTest
#endif

// This class tests the scenario when permission to access mic or camera is
// granted.
class MAYBE_WebRtcBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcBrowserTest() {}
  ~MAYBE_WebRtcBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  // Convenience function since most peerconnection-call.html tests just load
  // the page, kick off some javascript and wait for the title to change to OK.
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    MakeTypicalCall(javascript, "/media/peerconnection-call.html");
  }

  void SetConfigurationTest(const std::string& javascript) {
    // This doesn't actually "make a call", it just loads the page, executes
    // the javascript and waits for "OK".
    MakeTypicalCall(javascript, "/media/peerconnection-setConfiguration.html");
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CanSetupAudioAndVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NetworkProcessCrashRecovery) {
  if (!IsOutOfProcessNetworkService())
    return;
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
  SimulateNetworkServiceCrash();
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupDefaultVideoCallWithOldGetUserMedia) {
  MakeTypicalPeerConnectionCall("oldStyleCall();");
}

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupDefaultVideoCall) {
  MakeTypicalPeerConnectionCall(
      "callAndExpectResolution({video: true}, 640, 480);");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallWith1To1AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 320,"
      " maxWidth: 320, minHeight: 320, maxHeight: 320}}}, 320, 320);";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallWith16To9AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 640,"
      " maxWidth: 640, minAspectRatio: 1.777}}}, 640, 360);";
  MakeTypicalPeerConnectionCall(javascript);
}


IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallWith4To3AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: { minWidth: 320,"
      "maxWidth: 320, minAspectRatio: 1.333, maxAspectRatio: 1.333}}}, 320,"
      " 240);";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallAndDisableLocalVideo) {
  const std::string javascript =
      "callAndDisableLocalVideo({video: true});";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CanSetupCallAndSendDtmf) {
  MakeTypicalPeerConnectionCall("callAndSendDtmf(\'123,ABC\');");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
  const char* kJavascript =
      "callEmptyThenAddOneStreamAndRenegotiate({video: true, audio: true});";
  MakeTypicalPeerConnectionCall(kJavascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanMakeAudioCallAndThenRenegotiateToVideo) {
  const char* kJavascript =
      "callAndRenegotiateToVideo({audio: true}, {audio: true, video:true});";
  MakeTypicalPeerConnectionCall(kJavascript);
}

// This test makes a call between pc1 and pc2 where a video only stream is sent
// from pc1 to pc2. The stream sent from pc1 to pc2 is cloned from the stream
// received on pc2 to test that cloning of remote video and audio tracks works
// as intended and is sent back to pc1.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CanForwardRemoteStream) {
#if defined (OS_ANDROID)
  // This test fails on Nexus 5 devices.
  // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
  // for details.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebRtcHWDecoding);
#endif
  MakeTypicalPeerConnectionCall(
      "callAndForwardRemoteStream({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       NoCrashWhenConnectChromiumSinkToRemoteTrack) {
  MakeTypicalPeerConnectionCall("ConnectChromiumSinkToRemoteAudioTrack();");
}

// This test will make a complete PeerConnection-based call but remove the
// MSID and bundle attribute from the initial offer to verify that
// video is playing for the call even if the initiating client don't support
// MSID. http://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02
// Fails with TSAN. https://crbug.com/756568
#if defined(THREAD_SANITIZER)
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle \
  DISABLED_CanSetupAudioAndVideoCallWithoutMsidAndBundle
#else
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle \
  CanSetupAudioAndVideoCallWithoutMsidAndBundle
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  MakeTypicalPeerConnectionCall("callWithoutMsidAndBundle();");
}

// This test will modify the SDP offer to an unsupported codec, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       NegotiateUnsupportedVideoCodec) {
  MakeTypicalPeerConnectionCall("negotiateUnsupportedVideoCodec();");
}

// This test will modify the SDP offer to use no encryption, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NegotiateNonCryptoCall) {
  MakeTypicalPeerConnectionCall("negotiateNonCryptoCall();");
}

// This test can negotiate an SDP offer that includes a b=AS:xx to control
// the bandwidth for audio and video
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NegotiateOfferWithBLine) {
  MakeTypicalPeerConnectionCall("negotiateOfferWithBLine();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CallWithNewVideoMediaStream) {
  MakeTypicalPeerConnectionCall("callWithNewVideoMediaStream();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia. When video is flowing, the VideoTrack is removed and an
// AudioTrack is added instead.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallAndModifyStream) {
  MakeTypicalPeerConnectionCall(
      "callWithNewVideoMediaStreamLaterSwitchToAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  MakeTypicalPeerConnectionCall("addTwoMediaStreamsToOneConnection();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallAndVerifyVideoMutingWorks) {
  MakeTypicalPeerConnectionCall("callAndEnsureVideoTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CreateOfferWithOfferOptions) {
  MakeTypicalPeerConnectionCall("testCreateOfferOptions();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallInsideIframe) {
  MakeTypicalPeerConnectionCall("callInsideIframe({video: true, audio:true});");
}

// Tests that SetConfiguration succeeds and triggers an ICE restart on the next
// offer as described by JSEP.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, SetConfiguration) {
  SetConfigurationTest("testSetConfiguration();");
}

// Tests the error conditions of SetConfiguration as described by webrtc-pc.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, SetConfigurationErrors) {
  SetConfigurationTest("testSetConfigurationErrors();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, ApplyConstraints) {
  MakeTypicalPeerConnectionCall("testApplyConstraints();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       GetSettingsReportsValuesForRemoteTracks) {
  MakeTypicalPeerConnectionCall(
      "testGetSettingsReportsValuesForRemoteTracks();");
}

// TODO(crbug.com/988432): This test is a temporary replacement for:
// external/wpt/webrtc/RTCRtpReceiver-getSynchronizationSources.https.html
IN_PROC_BROWSER_TEST_F(
    MAYBE_WebRtcBrowserTest,
    EstablishVideoOnlyCallAndVerifyGetSynchronizationSourcesWorks) {
  MakeTypicalPeerConnectionCall(
      "testEstablishVideoOnlyCallAndVerifyGetSynchronizationSourcesWorks();");
}

#if defined(OS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
// This test is to make sure HW H264 work normally on supported devices, since
// there is no SW H264 fallback available on Android.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupH264VideoCallOnSupportedDevice) {
  MakeTypicalPeerConnectionCall("CanSetupH264VideoCallOnSupportedDevice();");
}
#endif

}  // namespace content
