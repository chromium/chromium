// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_throttle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

namespace {

class ResumeOnNavigationDiscardThrottle : public NavigationThrottle,
                                          public WebContentsObserver {
 public:
  explicit ResumeOnNavigationDiscardThrottle(
      NavigationThrottleRegistry& registry)
      : NavigationThrottle(registry),
        WebContentsObserver(registry.GetNavigationHandle().GetWebContents()) {}
  // NavigationThrottle:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    defered_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
    return DEFER;
  }

  const char* GetNameForLogging() override {
    return "ResumeOnNavigationDiscardThrottle";
  }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* finished_navigation) override {
    if (finished_navigation == navigation_handle() &&
        finished_navigation->GetNavigationDiscardReason().has_value()) {
      Resume();
    }
  }

  void WaitUntilDefered() {
    if (!defered_) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  base::WeakPtr<ResumeOnNavigationDiscardThrottle> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  ~ResumeOnNavigationDiscardThrottle() override = default;

 private:
  bool defered_ = false;
  base::OnceClosure quit_closure_;

  base::WeakPtrFactory<ResumeOnNavigationDiscardThrottle> weak_factory_{this};
};

// Asks for the page to be closed from within a NavigationThrottle event, and
// records what is still around once the call returns.
class ClosePageThrottle : public NavigationThrottle {
 public:
  struct Observations {
    bool throttle_alive = false;
    bool web_contents_alive = false;
  };

  ClosePageThrottle(NavigationThrottleRegistry& registry,
                    Observations* observations)
      : NavigationThrottle(registry), observations_(observations) {}

  ~ClosePageThrottle() override = default;

  // NavigationThrottle:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    // Everything is read onto the stack first: closing the page cancels this
    // navigation, so `this` is gone by the time ClosePage() returns.
    Observations* observations = observations_;
    WebContents* web_contents = navigation_handle()->GetWebContents();
    base::WeakPtr<WebContents> weak_web_contents = web_contents->GetWeakPtr();
    base::WeakPtr<ClosePageThrottle> weak_this = weak_factory_.GetWeakPtr();

    web_contents->ClosePage();

    observations->throttle_alive = !!weak_this;
    observations->web_contents_alive = !!weak_web_contents;
    return CANCEL_AND_IGNORE;
  }

  const char* GetNameForLogging() override { return "ClosePageThrottle"; }

 private:
  raw_ptr<Observations> observations_;

  base::WeakPtrFactory<ClosePageThrottle> weak_factory_{this};
};

}  // namespace

class NavigationThrottleOnDiscardBrowserTest : public ContentBrowserTest {
 public:
  NavigationThrottleOnDiscardBrowserTest() = default;

 protected:
  void SetUp() override { ContentBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    CHECK(embedded_test_server()->Start());
    throttle_inserter_ = std::make_unique<TestNavigationThrottleInserter>(
        web_contents(),
        base::BindLambdaForTesting(
            [&](NavigationThrottleRegistry& registry) -> void {
              // Defers subframe navigations on `WillCommitWithoutUrlLoader` by
              // registering a `TestNavigationThrottle`, so that
              // NavigationRequest created in each test from
              // `CreatePausedSubframeNavigationRequest` wouldn't commit
              // immediately.
              auto throttle =
                  std::make_unique<ResumeOnNavigationDiscardThrottle>(registry);
              throttle_ = throttle->GetWeakPtr();
              registry.AddThrottle(std::move(throttle));
            }));
  }

  base::WeakPtr<ResumeOnNavigationDiscardThrottle> GetThrottle() {
    return throttle_;
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  FrameTreeNode* main_frame() {
    return web_contents()->GetPrimaryFrameTree().root();
  }

  RenderFrameHostImpl* current_frame_host() {
    return main_frame()->current_frame_host();
  }

 private:
  base::WeakPtr<ResumeOnNavigationDiscardThrottle> throttle_;
  std::unique_ptr<TestNavigationThrottleInserter> throttle_inserter_;
};

// Regression test for crbug.com/496792860
IN_PROC_BROWSER_TEST_F(NavigationThrottleOnDiscardBrowserTest,
                       ResumeOnDiscard) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  shell()->LoadURL(url);
  EXPECT_TRUE(base::test::RunUntil([&]() { return GetThrottle() != nullptr; }));
  GetThrottle()->WaitUntilDefered();
  // Now stop the web contents and discard the on-going navigation request.
  // The ResumeOnNavigationDiscardThrottle will resume the navigation but crash
  // should not happen.
  web_contents()->Stop();
}

class ClosePageFromThrottleBrowserTest : public ContentBrowserTest {
 public:
  ClosePageFromThrottleBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// ClosePage() cancels the navigation it is called from, which destroys the
// NavigationRequest and with it the NavigationThrottle that asked for the
// close. A throttle that closes the page it just intercepted a navigation for
// must therefore not touch `this` afterwards, and has to finish whatever it was
// doing off the stack. Closing the page itself still takes a round trip to the
// renderer to run the unload handlers.
IN_PROC_BROWSER_TEST_F(ClosePageFromThrottleBrowserTest,
                       ClosePageFromThrottleDestroysThrottle) {
  Shell* new_shell = CreateBrowser();
  WebContents* web_contents = new_shell->web_contents();
  ASSERT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title1.html")));

  ClosePageThrottle::Observations observations;
  TestNavigationThrottleInserter inserter(
      web_contents,
      base::BindLambdaForTesting([&](NavigationThrottleRegistry& registry) {
        registry.AddThrottle(
            std::make_unique<ClosePageThrottle>(registry, &observations));
      }));

  WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  new_shell->LoadURL(embedded_test_server()->GetURL("/title2.html"));
  destroyed_watcher.Wait();

  EXPECT_FALSE(observations.throttle_alive);
  EXPECT_TRUE(observations.web_contents_alive);
}

}  // namespace content
