// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
#define CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace aura {
namespace test {
class AuraTestHelper;
}
}

namespace display {
class Screen;
}

namespace net {
namespace test {
class MockNetworkChangeNotifier;
}
}  // namespace net

namespace ui {
class ScopedOleInitializer;
}

namespace content {

class BrowserContext;
class ContentBrowserSanityChecker;
class MockRenderProcessHost;
class MockRenderProcessHostFactory;
class NavigationController;
class RenderProcessHostFactory;
class TestRenderFrameHostFactory;
class TestRenderViewHostFactory;
class TestRenderWidgetHostFactory;
class WebContents;
struct WebPreferences;

// An interface and utility for driving tests of RenderFrameHost.
class RenderFrameHostTester {
 public:
  // Retrieves the RenderFrameHostTester that drives the specified
  // RenderFrameHost. The RenderFrameHost must have been created while
  // RenderFrameHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderFrameHostTester* For(RenderFrameHost* host);

  // Calls the RenderFrameHost's private OnMessageReceived function with the
  // given message.
  static bool TestOnMessageReceived(RenderFrameHost* rfh,
                                    const IPC::Message& msg);

  // Commit the load pending in the given |controller| if any.
  // TODO(ahemery): This should take a WebContents directly.
  static void CommitPendingLoad(NavigationController* controller);

  virtual ~RenderFrameHostTester() {}

  // Simulates initialization of the RenderFrame object in the renderer process
  // and ensures internal state of RenderFrameHost is ready for simulating
  // RenderFrame originated IPCs.
  virtual void InitializeRenderFrameIfNeeded() = 0;

  // Gives tests access to RenderFrameHostImpl::OnCreateChild. The returned
  // RenderFrameHost is owned by the parent RenderFrameHost.
  virtual RenderFrameHost* AppendChild(const std::string& frame_name) = 0;

  // Gives tests access to RenderFrameHostImpl::OnDetach. Destroys |this|.
  virtual void Detach() = 0;

  // Calls OnBeforeUnloadACK on this RenderFrameHost with the given parameter.
  virtual void SendBeforeUnloadACK(bool proceed) = 0;

  // Simulates the SwapOut_ACK that fires if you commit a cross-site
  // navigation without making any network requests.
  virtual void SimulateSwapOutACK() = 0;

  // Set the feature policy header for the RenderFrameHost for test. Currently
  // this is limited to setting an allowlist for a single feature. This function
  // can be generalized as needed. Setting a header policy should only be done
  // once per navigation of the RFH.
  virtual void SimulateFeaturePolicyHeader(
      blink::mojom::FeaturePolicyFeature feature,
      const std::vector<url::Origin>& allowlist) = 0;

  // Gets all the console messages requested via
  // RenderFrameHost::AddMessageToConsole in this frame.
  virtual const std::vector<std::string>& GetConsoleMessages() = 0;
};

// An interface and utility for driving tests of RenderViewHost.
class RenderViewHostTester {
 public:
  // Retrieves the RenderViewHostTester that drives the specified
  // RenderViewHost.  The RenderViewHost must have been created while
  // RenderViewHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderViewHostTester* For(RenderViewHost* host);

  static void SimulateFirstPaint(RenderViewHost* rvh);

  // Returns whether the underlying web-page has any touch-event handlers.
  static bool HasTouchEventHandler(RenderViewHost* rvh);

  virtual ~RenderViewHostTester() {}

  // Gives tests access to RenderViewHostImpl::CreateRenderView.
  virtual bool CreateTestRenderView(const base::string16& frame_name,
                                    int opener_frame_route_id,
                                    int proxy_routing_id,
                                    bool created_with_opener) = 0;

  // Makes the WasHidden/WasShown calls to the RenderWidget that
  // tell it it has been hidden or restored from having been hidden.
  virtual void SimulateWasHidden() = 0;
  virtual void SimulateWasShown() = 0;

  // Promote ComputeWebPreferences to public.
  virtual WebPreferences TestComputeWebPreferences() = 0;
};

// You can instantiate only one class like this at a time.  During its
// lifetime, RenderViewHost and RenderFrameHost objects created may be used via
// RenderViewHostTester and RenderFrameHostTester respectively.
class RenderViewHostTestEnabler {
 public:
  RenderViewHostTestEnabler();
  ~RenderViewHostTestEnabler();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestEnabler);
  friend class RenderViewHostTestHarness;

#if defined(OS_ANDROID)
  std::unique_ptr<display::Screen> screen_;
#endif
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<MockRenderProcessHostFactory> rph_factory_;
  std::unique_ptr<TestRenderViewHostFactory> rvh_factory_;
  std::unique_ptr<TestRenderFrameHostFactory> rfh_factory_;
  std::unique_ptr<TestRenderWidgetHostFactory> rwhi_factory_;
};

// RenderViewHostTestHarness ---------------------------------------------------
class RenderViewHostTestHarness : public testing::Test {
 public:
  // Constructs a RenderViewHostTestHarness which uses |traits| to initialize
  // its BrowserTaskEnvironment.
  template <typename... TaskEnvironmentTraits>
  explicit RenderViewHostTestHarness(TaskEnvironmentTraits&&... traits)
      : RenderViewHostTestHarness(std::make_unique<BrowserTaskEnvironment>(
            std::forward<TaskEnvironmentTraits>(traits)...)) {}

  ~RenderViewHostTestHarness() override;

  NavigationController& controller();

  // The contents under test.
  WebContents* web_contents();

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // rvh() is equivalent to either of:
  //   web_contents()->GetMainFrame()->GetRenderViewHost()
  //   web_contents()->GetRenderViewHost()
  RenderViewHost* rvh();

  // pending_rvh() is equivalent to:
  //   WebContentsTester::For(web_contents())->GetPendingRenderViewHost()
  RenderViewHost* pending_rvh();

  // active_rvh() is equivalent to pending_rvh() ? pending_rvh() : rvh()
  RenderViewHost* active_rvh();

  // main_rfh() is equivalent to web_contents()->GetMainFrame()
  RenderFrameHost* main_rfh();

  // pending_main_rfh() is equivalent to:
  //   WebContentsTester::For(web_contents())->GetPendingMainFrame()
  RenderFrameHost* pending_main_rfh();

  BrowserContext* browser_context();
  MockRenderProcessHost* process();

  // Frees the current WebContents for tests that want to test destruction.
  void DeleteContents();

  // Sets the current WebContents for tests that want to alter it. Takes
  // ownership of the WebContents passed.
  void SetContents(std::unique_ptr<WebContents> contents);

  // Creates a new test-enabled WebContents. Ownership passes to the
  // caller.
  std::unique_ptr<WebContents> CreateTestWebContents();

  // Cover for |contents()->NavigateAndCommit(url)|. See
  // WebContentsTester::NavigateAndCommit for details.
  // Optional parameter transition allows transition type to be controlled for
  // greater flexibility for tests.
  void NavigateAndCommit(
      const GURL& url,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK);

  // Sets the focused frame to the main frame of the WebContents for tests that
  // rely on the focused frame not being null.
  void FocusWebContentsOnMainFrame();

 protected:
  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // Derived classes should override this method to use a custom BrowserContext.
  // It is invoked by SetUp after threads were started.
  // RenderViewHostTestHarness will take ownership of the returned
  // BrowserContext.
  virtual std::unique_ptr<BrowserContext> CreateBrowserContext();

  // Derived classes can override this method to have the test harness use a
  // different BrowserContext than the one owned by this class. This is most
  // useful for off-the-record contexts, which are usually owned by the original
  // context.
  virtual BrowserContext* GetBrowserContext();

  BrowserTaskEnvironment* task_environment() { return task_environment_.get(); }

#if defined(USE_AURA)
  aura::Window* root_window() { return aura_test_helper_->root_window(); }
#endif

  // Replaces the RPH being used.
  void SetRenderProcessHostFactory(RenderProcessHostFactory* factory);

 private:
  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line.
  explicit RenderViewHostTestHarness(
      std::unique_ptr<BrowserTaskEnvironment> task_environment);

  std::unique_ptr<BrowserTaskEnvironment> task_environment_;

  std::unique_ptr<ContentBrowserSanityChecker> sanity_checker_;

  // TODO(crbug.com/1011275): This is a temporary work around to fix flakiness
  // on tests. The default behavior of the network stack is to allocate a
  // leaking SystemDnsConfigChangeNotifier. This holds on to a set of
  // FilePathWatchers on Posix and ObjectWatchers on Windows that outlive
  // the message queues of the task_environment_ and may post messages after
  // their death.
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;

  std::unique_ptr<BrowserContext> browser_context_;

  // This must be placed before |contents_| such that it will be destructed
  // after it. See https://crbug.com/770451
  std::unique_ptr<RenderViewHostTestEnabler> rvh_test_enabler_;

  std::unique_ptr<WebContents> contents_;
#if defined(OS_WIN)
  std::unique_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif
#if defined(USE_AURA)
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
#endif
  RenderProcessHostFactory* factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
