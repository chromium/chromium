// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_service_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/browser/back_forward_cache_browsertest.h"
#include "content/browser/bad_message.h"
#include "content/browser/haptics/haptics_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/haptics/haptics.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

class MockHapticsManager : public HapticsManager {
 public:
  MockHapticsManager() = default;
  MockHapticsManager(const MockHapticsManager&) = delete;
  MockHapticsManager& operator=(const MockHapticsManager&) = delete;
  ~MockHapticsManager() override = default;

  MOCK_METHOD(void,
              PlayHaptics,
              (blink::mojom::HapticEffect, double),
              (override));

  base::WeakPtr<MockHapticsManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockHapticsManager> weak_ptr_factory_{this};
};

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
    HapticsServiceImpl::SetHapticsManagerFactoryForTesting(
        base::BindLambdaForTesting([this]() -> std::unique_ptr<HapticsManager> {
          auto mock = std::make_unique<MockHapticsManager>();
          mock_ = mock->GetWeakPtr();
          return mock;
        }));
  }

  void TearDownOnMainThread() override {
    HapticsServiceImpl::SetHapticsManagerFactoryForTesting(
        HapticsServiceImpl::HapticsManagerFactory());
    mock_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  test::FencedFrameTestHelper& fenced_frame_helper() {
    return fenced_frame_helper_;
  }

 protected:
  MockHapticsManager* mock_manager() { return mock_.get(); }

 private:
  base::WeakPtr<MockHapticsManager> mock_;
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

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBrowserTest,
                       ForwardsToBackendWhenAllowed) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  // playHaptics() requires sticky user activation; a real gesture grants it.
  ASSERT_TRUE(ExecJs(rfh, "true"));

  mojo::Remote<blink::mojom::HapticsService> remote = BindHapticsService(rfh);
  ASSERT_TRUE(mock_manager());
  constexpr double kTestIntensity = 0.5;
  base::test::TestFuture<blink::mojom::HapticEffect, double> future;
  EXPECT_CALL(*mock_manager(), PlayHaptics)
      .WillOnce(base::test::InvokeFuture(future));
  remote->PlayHaptics(blink::mojom::HapticEffect::kEdge, kTestIntensity);

  EXPECT_EQ(future.Get<0>(), blink::mojom::HapticEffect::kEdge);
  EXPECT_NEAR(future.Get<1>(), kTestIntensity, 1.0e-3);
}

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBrowserTest,
                       DroppedWithoutUserActivation) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  mojo::Remote<blink::mojom::HapticsService> remote = BindHapticsService(rfh);
  ASSERT_TRUE(mock_manager());
  EXPECT_CALL(*mock_manager(), PlayHaptics).Times(0);
  remote->PlayHaptics(blink::mojom::HapticEffect::kHint, /*intensity=*/1.0);
  remote.FlushForTesting();
}

// Fixture for the inactive-frame case, which needs the back/forward cache to
// produce a real non-active RenderFrameHost.
class HapticsServiceImplBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "WebHaptics");
  }

  void SetUpOnMainThread() override {
    BackForwardCacheBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    HapticsServiceImpl::SetHapticsManagerFactoryForTesting(
        base::BindLambdaForTesting([this]() -> std::unique_ptr<HapticsManager> {
          auto mock = std::make_unique<MockHapticsManager>();
          mock_ = mock->GetWeakPtr();
          return mock;
        }));
  }

  void TearDownOnMainThread() override {
    HapticsServiceImpl::SetHapticsManagerFactoryForTesting(
        HapticsServiceImpl::HapticsManagerFactory());
    mock_.reset();
    BackForwardCacheBrowserTest::TearDownOnMainThread();
  }

 protected:
  MockHapticsManager* mock_manager() { return mock_.get(); }

 private:
  base::WeakPtr<MockHapticsManager> mock_;
};

IN_PROC_BROWSER_TEST_F(HapticsServiceImplBackForwardCacheBrowserTest,
                       DroppedWhenFrameNotActive) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHost* rfh_a = current_frame_host();
  // Grant sticky activation before caching so the inactive lifecycle state is
  // the only reason the call is dropped.
  ASSERT_TRUE(ExecJs(rfh_a, "true"));
  RenderFrameHostWrapper rfh_a_wrapper(rfh_a);

  // Navigate away; the previous document enters the back/forward cache.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(rfh_a_wrapper.IsDestroyed());
  ASSERT_EQ(rfh_a_wrapper->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);

  mojo::Remote<blink::mojom::HapticsService> remote =
      BindHapticsService(rfh_a_wrapper.get());
  ASSERT_TRUE(mock_manager());
  EXPECT_CALL(*mock_manager(), PlayHaptics).Times(0);
  remote->PlayHaptics(blink::mojom::HapticEffect::kHint, /*intensity=*/1.0);
  remote.FlushForTesting();
}

}  // namespace
}  // namespace content
