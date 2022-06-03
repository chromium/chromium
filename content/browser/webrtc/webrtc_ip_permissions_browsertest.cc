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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

namespace content {

namespace {
const char kPeerConnectionHtml[] = "/media/peerconnection-call.html";
}  // namespace

// Disable these test cases for Android since in some bots, there exists only
// the loopback interface.
#if defined(OS_ANDROID)
#define MAYBE_WebRtcIPPermissionGrantedTest \
  DISABLED_WebRtcIPPermissionGrantedTest
#define MAYBE_WebRtcIPPermissionDeniedTest DISABLED_WebRtcIPPermissionDeniedTest
#define MAYBE_WebRtcIPPolicyPublicAndPrivateInterfacesTest \
  DISABLED_WebRtcIPPolicyPublicAndPrivateInterfacesTest
#define MAYBE_WebRtcIPPolicyPublicInterfaceOnlyTest \
  DISABLED_WebRtcIPPolicyPublicInterfaceOnlyTest
#define MAYBE_WebRtcIPPolicyDisableUdpTest DISABLED_WebRtcIPPolicyDisableUdpTest
#else
#define MAYBE_WebRtcIPPermissionGrantedTest WebRtcIPPermissionGrantedTest
#define MAYBE_WebRtcIPPermissionDeniedTest WebRtcIPPermissionDeniedTest
#define MAYBE_WebRtcIPPolicyPublicAndPrivateInterfacesTest \
  WebRtcIPPolicyPublicAndPrivateInterfacesTest
#define MAYBE_WebRtcIPPolicyPublicInterfaceOnlyTest \
  WebRtcIPPolicyPublicInterfaceOnlyTest
#define MAYBE_WebRtcIPPolicyDisableUdpTest WebRtcIPPolicyDisableUdpTest
#endif

// This class tests the scenario when permission to access mic or camera is
// denied.
class MAYBE_WebRtcIPPermissionGrantedTest
    : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcIPPermissionGrantedTest() {}
  ~MAYBE_WebRtcIPPermissionGrantedTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    AppendUseFakeUIForMediaStreamFlag();
    command_line->AppendSwitchASCII(switches::kForceWebRtcIPHandlingPolicy,
                                    blink::kWebRTCIPHandlingDefault);
  }
};

// Loopback interface is the non-default private interface. Test that when
// device permission is granted, we should have loopback candidates.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcIPPermissionGrantedTest,
                       GatherLocalCandidates) {
  MakeTypicalCall("callWithDevicePermissionGranted();", kPeerConnectionHtml);
}

// This class tests the scenario when permission to access mic or camera is
// denied.
class MAYBE_WebRtcIPPermissionDeniedTest : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcIPPermissionDeniedTest() {}
  ~MAYBE_WebRtcIPPermissionDeniedTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceWebRtcIPHandlingPolicy,
                                    blink::kWebRTCIPHandlingDefault);
  }
};

// Test that when device permission is denied, only non-default interfaces are
// gathered even if the policy is "default".
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcIPPermissionDeniedTest,
                       GatherLocalCandidates) {
  MakeTypicalCall("callAndExpectNonLoopbackCandidates();", kPeerConnectionHtml);
}

// This class tests the scenario when ip handling policy is set to "public and
// private interfaces", the non-default private candidate is not gathered.
class MAYBE_WebRtcIPPolicyPublicAndPrivateInterfacesTest
    : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcIPPolicyPublicAndPrivateInterfacesTest() {}
  ~MAYBE_WebRtcIPPolicyPublicAndPrivateInterfacesTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    AppendUseFakeUIForMediaStreamFlag();
    command_line->AppendSwitchASCII(
        switches::kForceWebRtcIPHandlingPolicy,
        blink::kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces);
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcIPPolicyPublicAndPrivateInterfacesTest,
                       GatherLocalCandidates) {
  MakeTypicalCall("callAndExpectNonLoopbackCandidates();", kPeerConnectionHtml);
}

// This class tests the scenario when ip handling policy is set to "public
// interface only", there is no candidate gathered as there is no stun server
// specified.
class MAYBE_WebRtcIPPolicyPublicInterfaceOnlyTest
    : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcIPPolicyPublicInterfaceOnlyTest() {}
  ~MAYBE_WebRtcIPPolicyPublicInterfaceOnlyTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    AppendUseFakeUIForMediaStreamFlag();
    command_line->AppendSwitchASCII(
        switches::kForceWebRtcIPHandlingPolicy,
        blink::kWebRTCIPHandlingDefaultPublicInterfaceOnly);
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcIPPolicyPublicInterfaceOnlyTest,
                       GatherLocalCandidates) {
  MakeTypicalCall("callWithNoCandidateExpected();", kPeerConnectionHtml);
}

// This class tests the scenario when ip handling policy is set to "disable
// non-proxied udp", there is no candidate gathered as there is no stun server
// specified.
class MAYBE_WebRtcIPPolicyDisableUdpTest : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcIPPolicyDisableUdpTest() {}
  ~MAYBE_WebRtcIPPolicyDisableUdpTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    AppendUseFakeUIForMediaStreamFlag();
    command_line->AppendSwitchASCII(
        switches::kForceWebRtcIPHandlingPolicy,
        blink::kWebRTCIPHandlingDisableNonProxiedUdp);
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcIPPolicyDisableUdpTest,
                       GatherLocalCandidates) {
  MakeTypicalCall("callWithNoCandidateExpected();", kPeerConnectionHtml);
}

}  // namespace content
