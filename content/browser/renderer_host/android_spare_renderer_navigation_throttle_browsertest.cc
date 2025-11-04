// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/android_spare_renderer_navigation_throttle.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AndroidSpareRendererNavigationThrottleForTesting
    : public AndroidSpareRendererNavigationThrottle {
 public:
  AndroidSpareRendererNavigationThrottleForTesting(
      NavigationThrottleRegistry& registry,
      RenderProcessHost* rph)
      : AndroidSpareRendererNavigationThrottle(registry),
        render_process_host_for_testing_(rph) {}

  void WaitForResume() {
    if (!resume_called_) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 protected:
  RenderProcessHost* GetSpeculativeRenderProcessHost() override {
    return render_process_host_for_testing_;
  }

  void Resume() override {
    resume_called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  bool resume_called_ = false;
  base::OnceClosure quit_closure_;
  raw_ptr<RenderProcessHost> render_process_host_for_testing_;
};

class AndroidSpareRendererNavigationThrottleTest : public ContentBrowserTest {
 public:
  AndroidSpareRendererNavigationThrottleTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAndroidWarmUpSpareRendererWithTimeout,
        {
            {features::kAndroidSpareRendererAddNavigationThrottle.name, "true"},
        });
  }

 protected:
  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  BrowserContext* browser_context() const {
    return shell()->web_contents()->GetBrowserContext();
  }

  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  std::unique_ptr<MockNavigationThrottleRegistry>
  CreateMockNavigationThrottleRegistry() {
    const GURL url = embedded_test_server()->GetURL("/title1.html");
    TestNavigationManager navigation_manager(contents(), url);
    shell()->LoadURL(url);
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    NavigationRequest* request =
        contents()->GetPrimaryFrameTree().root()->navigation_request();
    return std::make_unique<MockNavigationThrottleRegistry>(
        request, MockNavigationThrottleRegistry::RegistrationMode::kHold);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AndroidSpareRendererNavigationThrottleTest,
                       SpareRendererNavigationIsThrottled) {
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      RenderProcessHostImpl::CreateSpareRenderProcessHost(browser_context(),
                                                          nullptr));
  rph->Init();
  EXPECT_TRUE(base::test::RunUntil([&]() { return rph->IsReady(); }));
  std::unique_ptr<MockNavigationThrottleRegistry> registry =
      CreateMockNavigationThrottleRegistry();
  std::unique_ptr<AndroidSpareRendererNavigationThrottleForTesting> throttle =
      std::make_unique<AndroidSpareRendererNavigationThrottleForTesting>(
          *registry, rph);

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
}

IN_PROC_BROWSER_TEST_F(AndroidSpareRendererNavigationThrottleTest,
                       NonSpareRendererNavigationNotThrottled) {
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      RenderProcessHostImpl::CreateRenderProcessHost(browser_context(),
                                                     nullptr));
  rph->Init();
  EXPECT_TRUE(base::test::RunUntil([&]() { return rph->IsReady(); }));
  std::unique_ptr<MockNavigationThrottleRegistry> registry =
      CreateMockNavigationThrottleRegistry();
  std::unique_ptr<AndroidSpareRendererNavigationThrottleForTesting> throttle =
      std::make_unique<AndroidSpareRendererNavigationThrottleForTesting>(
          *registry, rph);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

IN_PROC_BROWSER_TEST_F(AndroidSpareRendererNavigationThrottleTest,
                       SpareRendererNavigationNotThrottledAfterGraduate) {
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      RenderProcessHostImpl::CreateSpareRenderProcessHost(browser_context(),
                                                          nullptr));
  rph->Init();
  EXPECT_TRUE(base::test::RunUntil([&]() { return rph->IsReady(); }));
  rph->GraduateSpareToNormalRendererPriority();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !rph->ShouldThrottleNavigationForSpareRendererGraduation();
  }));
  std::unique_ptr<MockNavigationThrottleRegistry> registry =
      CreateMockNavigationThrottleRegistry();
  std::unique_ptr<AndroidSpareRendererNavigationThrottleForTesting> throttle =
      std::make_unique<AndroidSpareRendererNavigationThrottleForTesting>(
          *registry, rph);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

IN_PROC_BROWSER_TEST_F(AndroidSpareRendererNavigationThrottleTest,
                       SpareRendererNavigationUnthrottledAfterGraduate) {
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      RenderProcessHostImpl::CreateSpareRenderProcessHost(browser_context(),
                                                          nullptr));
  rph->Init();
  EXPECT_TRUE(base::test::RunUntil([&]() { return rph->IsReady(); }));
  std::unique_ptr<MockNavigationThrottleRegistry> registry =
      CreateMockNavigationThrottleRegistry();
  std::unique_ptr<AndroidSpareRendererNavigationThrottleForTesting> throttle =
      std::make_unique<AndroidSpareRendererNavigationThrottleForTesting>(
          *registry, rph);
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
  rph->GraduateSpareToNormalRendererPriority();
  throttle->WaitForResume();
}

}  // namespace content
