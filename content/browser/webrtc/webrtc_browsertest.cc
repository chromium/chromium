// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

namespace content {

#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcBrowserTest DISABLED_WebRtcBrowserTest
#else
#define MAYBE_WebRtcBrowserTest WebRtcBrowserTest
#endif

// This class tests the scenario when permission to access mic or camera is
// granted.
class MAYBE_WebRtcBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcBrowserTest() {
#if BUILDFLAG(IS_ANDROID)
    // This test fails on Nexus 5 devices.
    // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
    // for details.
    scoped_feature_list_.InitAndDisableFeature(features::kWebRtcHWDecoding);
#endif
  }
  ~MAYBE_WebRtcBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  // Convenience function since most peerconnection-call.html tests just load
  // the page, and execute some javascript.
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    MakeTypicalCall(javascript, "/media/peerconnection-call.html");
  }

  void SetConfigurationTest(const std::string& javascript) {
    // This doesn't actually "make a call", it just loads the page, and executes
    // the javascript, expecting no errors to be thrown.
    MakeTypicalCall(javascript, "/media/peerconnection-setConfiguration.html");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CanSetupAudioAndVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}

// Flaky on Android and Linux ASAN https://crbug.com/1099365.
#if BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER))
#define MAYBE_NetworkProcessCrashRecovery DISABLED_NetworkProcessCrashRecovery
#else
#define MAYBE_NetworkProcessCrashRecovery NetworkProcessCrashRecovery
#endif

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_NetworkProcessCrashRecovery) {
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
//
// TODO(crbug.com/40930185): Re-enable this test.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CanSetupDefaultVideoCall) {
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

// TODO(crbug.com/40930185): Re-enable this test.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CanSetupVideoCallWith16To9AspectRatio) {
#if BUILDFLAG(IS_ANDROID)
  // Android requires 16x16 alignment for hardware encoding.
  constexpr int kExpectedAlignment = 16;
#else
  constexpr int kExpectedAlignment = 1;
#endif
  const std::string javascript = base::StringPrintf(
      "callAndExpectResolution({video: {mandatory: {minWidth: 640,"
      " maxWidth: 640, minAspectRatio: 1.777}}}, 640, 360, %d);",
      kExpectedAlignment);
  MakeTypicalPeerConnectionCall(javascript);
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40781953): This test is flakey on macOS.
#define MAYBE_CanSetupVideoCallWith4To3AspectRatio \
  DISABLED_CanSetupVideoCallWith4To3AspectRatio
#else
#define MAYBE_CanSetupVideoCallWith4To3AspectRatio \
  CanSetupVideoCallWith4To3AspectRatio
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupVideoCallWith4To3AspectRatio) {
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

// TODO(crbug.com/40637961): This test is a temporary replacement for:
// external/wpt/webrtc/RTCRtpReceiver-getSynchronizationSources.https.html
IN_PROC_BROWSER_TEST_F(
    MAYBE_WebRtcBrowserTest,
    EstablishVideoOnlyCallAndVerifyGetSynchronizationSourcesWorks) {
  MakeTypicalPeerConnectionCall(
      "testEstablishVideoOnlyCallAndVerifyGetSynchronizationSourcesWorks();");
}

// Flaky on Android: https://crbug.com/1366910.
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
// This test is to make sure HW H264 work normally on supported devices, since
// there is no SW H264 fallback available on Android.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CanSetupH264VideoCallOnSupportedDevice) {
  MakeTypicalPeerConnectionCall("CanSetupH264VideoCallOnSupportedDevice();");
}
#endif

}  // namespace content
