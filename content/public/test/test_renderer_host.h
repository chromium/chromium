// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
#define CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "ui/base/page_transition_types.h"

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace aura {
namespace test {
class AuraTestHelper;
}
}  // namespace aura

namespace blink {
struct ParsedPermissionsPolicyDeclaration;
using ParsedPermissionsPolicy = std::vector<ParsedPermissionsPolicyDeclaration>;

namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace display {
#if BUILDFLAG(IS_ANDROID)
class Screen;
#endif
class ScopedNativeScreen;
}  // namespace display

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
class ContentBrowserConsistencyChecker;
class InputMsgWatcher;
class MockAgentSchedulingGroupHostFactory;
class MockRenderProcessHost;
class MockRenderProcessHostFactory;
class NavigationController;
class RenderProcessHostFactory;
class TestNavigationURLLoaderFactory;
class TestPageFactory;
class TestRenderFrameHostFactory;
class TestRenderViewHostFactory;
class TestRenderWidgetHostFactory;
class WebContents;

// An interface and utility for driving tests of RenderFrameHost.
class RenderFrameHostTester {
 public:
  enum class HeavyAdIssueType {
    kNetworkTotal,
    kCpuTotal,
    kCpuPeak,
    kAll,
  };

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
  static void CommitPendingLoad(NavigationController* controller);

  virtual ~RenderFrameHostTester() {}

  // Simulates initialization of the RenderFrame object in the renderer process
  // and ensures internal state of RenderFrameHost is ready for simulating
  // RenderFrame originated IPCs.
  virtual void InitializeRenderFrameIfNeeded() = 0;

  // Gives tests access to RenderFrameHostImpl::OnCreateChild. The returned
  // RenderFrameHost is owned by the parent RenderFrameHost.
  virtual RenderFrameHost* AppendChild(const std::string& frame_name) = 0;

  // Same as AppendChild above, but simulates a custom allow attribute being
  // used as the container policy.
  virtual RenderFrameHost* AppendChildWithPolicy(
      const std::string& frame_name,
      const blink::ParsedPermissionsPolicy& allow) = 0;

  // Same as AppendChild above, but simulates the `credentialless` attribute
  // being added.
  virtual RenderFrameHost* AppendCredentiallessChild(
      const std::string& frame_name) = 0;

  // Gives tests access to RenderFrameHostImpl::OnDetach. Destroys |this|.
  virtual void Detach() = 0;

  // Calls ProcessBeforeUnloadCompleted on this RenderFrameHost with the given
  // parameter.
  virtual void SimulateBeforeUnloadCompleted(bool proceed) = 0;

  // Simulates the mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame that
  // fires if you commit a cross-site navigation without making any network
  // requests.
  virtual void SimulateUnloadACK() = 0;

  // Simulates the frame receiving a user activation.
  virtual void SimulateUserActivation() = 0;

  // Gets all the console messages requested via
  // RenderFrameHost::AddMessageToConsole in this frame.
  virtual const std::vector<std::string>& GetConsoleMessages() = 0;

  // Clears the console messages logged in this frame.
  virtual void ClearConsoleMessages() = 0;

  // Get a count of the total number of heavy ad issues reported.
  virtual int GetHeavyAdIssueCount(HeavyAdIssueType type) = 0;

  // Simulates the receipt of a manifest URL.
  virtual void SimulateManifestURLUpdate(const GURL& manifest_url) = 0;

  // Creates and appends a fenced frame.
  virtual RenderFrameHost* AppendFencedFrame() = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Creates the HidService and binds `receiver`.
  virtual void CreateHidServiceForTesting(
      mojo::PendingReceiver<blink::mojom::HidService> receiever) = 0;
#endif  // !BUILDFLAG(IS_ANDROID)

  // Creates the WebUsbService and binds `receiver`.
  virtual void CreateWebUsbServiceForTesting(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) = 0;

  // Detaches the LocalFrame mojo connection to the renderer. This is useful
  // when tests override the creation logic for the LocalFrame and need the
  // connection to be re-initialized.
  virtual void ResetLocalFrame() = 0;
};

// An interface and utility for driving tests of RenderViewHost.
class RenderViewHostTester {
 public:
  // Retrieves the RenderViewHostTester that drives the specified
  // RenderViewHost.  The RenderViewHost must have been created while
  // RenderViewHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderViewHostTester* For(RenderViewHost* host);

  static std::unique_ptr<content::InputMsgWatcher> CreateInputWatcher(
      RenderViewHost* rvh,
      blink::WebInputEvent::Type type);

  static void SendTouchEvent(RenderViewHost* rvh,
                             blink::SyntheticWebTouchEvent* touch_event);

  virtual ~RenderViewHostTester() {}

  // Gives tests access to RenderViewHostImpl::CreateRenderView.
  virtual bool CreateTestRenderView() = 0;

  // Makes the WasHidden/WasShown calls to the RenderWidget that
  // tell it it has been hidden or restored from having been hidden.
  virtual void SimulateWasHidden() = 0;
  virtual void SimulateWasShown() = 0;

  // Promote ComputeWebPreferences to public.
  virtual blink::web_pref::WebPreferences TestComputeWebPreferences() = 0;
};

// You can instantiate only one class like this at a time.  During its
// lifetime, RenderViewHost and RenderFrameHost objects created may be used via
// RenderViewHostTester and RenderFrameHostTester respectively.
class RenderViewHostTestEnabler {
 public:
  // Whether this RenderViewHostTestEnabler should create
  // TestNavigationURLLoaderFactory or not.
  enum class NavigationURLLoaderFactoryType {
    // Create TestNavigationURLLoaderFactory.
    kTest,
    // Do not create TestRenderViewHostFactory. Useful for the tests which want
    // to mock or customise the NavigationURLLoader creation logic themselves.
    kNone,
  };
  explicit RenderViewHostTestEnabler(
      NavigationURLLoaderFactoryType navigation_url_loader_factory_type =
          NavigationURLLoaderFactoryType::kTest);

  RenderViewHostTestEnabler(const RenderViewHostTestEnabler&) = delete;
  RenderViewHostTestEnabler& operator=(const RenderViewHostTestEnabler&) =
      delete;

  ~RenderViewHostTestEnabler();

  friend class RenderViewHostTestHarness;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<display::Screen> screen_;
#endif
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<MockRenderProcessHostFactory> rph_factory_;
  std::unique_ptr<MockAgentSchedulingGroupHostFactory> asgh_factory_;
  std::unique_ptr<TestPageFactory> page_factory_;
  std::unique_ptr<TestRenderViewHostFactory> rvh_factory_;
  std::unique_ptr<TestRenderFrameHostFactory> rfh_factory_;
  std::unique_ptr<TestRenderWidgetHostFactory> rwhi_factory_;
  std::unique_ptr<TestNavigationURLLoaderFactory> loader_factory_;
};

// RenderViewHostTestHarness ---------------------------------------------------
class RenderViewHostTestHarness : public ::testing::Test {
 public:
  // Constructs a RenderViewHostTestHarness which uses |traits| to initialize
  // its BrowserTaskEnvironment.
  template <typename... TaskEnvironmentTraits>
  explicit RenderViewHostTestHarness(TaskEnvironmentTraits&&... traits)
      : RenderViewHostTestHarness(std::make_unique<BrowserTaskEnvironment>(
            std::forward<TaskEnvironmentTraits>(traits)...)) {}

  RenderViewHostTestHarness(const RenderViewHostTestHarness&) = delete;
  RenderViewHostTestHarness& operator=(const RenderViewHostTestHarness&) =
      delete;

  ~RenderViewHostTestHarness() override;

  NavigationController& controller();

  // The contents under test.
  WebContents* web_contents() const;

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // rvh() is equivalent to either of:
  //   web_contents()->GetPrimaryMainFrame()->GetRenderViewHost()
  //   web_contents()->GetRenderViewHost()
  RenderViewHost* rvh();

  // main_rfh() is equivalent to web_contents()->GetPrimaryMainFrame()
  RenderFrameHost* main_rfh();

  BrowserContext* browser_context();

  // Returns |main_rfh()|'s process.
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

  // Sets the focused frame to the `rfh` for tests that rely on the focused
  // frame not being null.
  void FocusWebContentsOnFrame(content::RenderFrameHost* rfh);

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
  aura::Window* root_window() { return aura_test_helper_->GetContext(); }
#endif

  // Replaces the RPH being used.
  void SetRenderProcessHostFactory(RenderProcessHostFactory* factory);

 private:
  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line.
  explicit RenderViewHostTestHarness(
      std::unique_ptr<BrowserTaskEnvironment> task_environment);

  std::unique_ptr<BrowserTaskEnvironment> task_environment_;

  std::unique_ptr<ContentBrowserConsistencyChecker> consistency_checker_;

  // TODO(crbug.com/40101830): This is a temporary work around to fix flakiness
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
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<display::ScopedNativeScreen> screen_;
#endif
#if defined(USE_AURA)
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
#endif
  raw_ptr<RenderProcessHostFactory> factory_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
