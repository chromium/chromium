// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_service_impl.h"

#include "base/command_line.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/haptics/haptics.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

// Binds the service directly, as the frame's interface broker would, to
// simulate a compromised renderer that bypasses the renderer-side gating.
mojo::Remote<blink::mojom::HapticsService> BindHapticsService(
    RenderFrameHost* render_frame_host) {
  mojo::Remote<blink::mojom::HapticsService> remote;
  HapticsServiceImpl::Create(render_frame_host,
                             remote.BindNewPipeAndPassReceiver());
  return remote;
}

class HapticsServiceImplBrowserTest : public ContentBrowserTest {
 public:
  HapticsServiceImplBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // The "haptics" permissions policy depends on the WebHaptics runtime
    // feature.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "WebHaptics");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  test::FencedFrameTestHelper& fenced_frame_helper() {
    return fenced_frame_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBrowserTest,
                       AllowedByPermissionsPolicy) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kHaptics));

  mojo::Remote<blink::mojom::HapticsService> remote = BindHapticsService(rfh);
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBrowserTest,
                       RejectedWhenBlockedByPermissionsPolicy) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/page-with-haptics-permissions-policy-disabled.html")));
  RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  ASSERT_FALSE(rfh->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kHaptics));

  RenderProcessHostBadIpcMessageWaiter kill_waiter(rfh->GetProcess());
  BindHapticsService(rfh);
  EXPECT_EQ(bad_message::HSI_PLAY_HAPTICS_BLOCKED_BY_PERMISSIONS_POLICY,
            kill_waiter.Wait());
}

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBrowserTest,
                       RejectedForInvalidIntensity) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  mojo::Remote<blink::mojom::HapticsService> remote = BindHapticsService(rfh);
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(rfh->GetProcess());
  remote->PlayHaptics(blink::mojom::HapticEffect::kHint, /*intensity=*/2.0);
  EXPECT_EQ(bad_message::HSI_PLAY_HAPTICS_INVALID_INTENSITY,
            kill_waiter.Wait());
}

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBrowserTest, RejectedInFencedFrame) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHost* fenced_frame_rfh = fenced_frame_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("/fenced_frames/basic.html"));
  ASSERT_TRUE(fenced_frame_rfh);
  ASSERT_TRUE(fenced_frame_rfh->IsNestedWithinFencedFrame());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(
      fenced_frame_rfh->GetProcess());
  BindHapticsService(fenced_frame_rfh);
  EXPECT_EQ(bad_message::HSI_PLAY_HAPTICS_IN_FENCED_FRAME, kill_waiter.Wait());
}

}  // namespace
}  // namespace content
