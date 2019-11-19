// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_renderer_host.h"

#include <utility>

#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/content_browser_sanity_checker.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_frame_host_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_render_widget_host_factory.h"
#include "content/test/test_web_contents.h"
#include "net/base/mock_network_change_notifier.h"
#include "ui/base/material_design/material_design_controller.h"

#if defined(OS_ANDROID)
#include "ui/android/dummy_screen_android.h"
#include "ui/display/screen.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#include "ui/wm/core/default_activation_client.h"
#endif

#if defined(OS_MACOSX)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

namespace content {

// RenderFrameHostTester ------------------------------------------------------

// static
RenderFrameHostTester* RenderFrameHostTester::For(RenderFrameHost* host) {
  return static_cast<TestRenderFrameHost*>(host);
}

// static
bool RenderFrameHostTester::TestOnMessageReceived(RenderFrameHost* rfh,
                                                  const IPC::Message& msg) {
  return static_cast<RenderFrameHostImpl*>(rfh)->OnMessageReceived(msg);
}

// static
void RenderFrameHostTester::CommitPendingLoad(
    NavigationController* controller) {
  // This function is currently used by BrowserWithTestWindowTest. It would be
  // ideal to instead make the users of that class create TestWebContents
  // (rather than WebContentsImpl directly). It is not trivial to make
  // that change, so for now we have this extra function for
  // non-TestWebContents.
  auto navigation =
      NavigationSimulator::CreateFromPending(controller->GetWebContents());
  navigation->Commit();
}

// RenderViewHostTester -------------------------------------------------------

// static
RenderViewHostTester* RenderViewHostTester::For(RenderViewHost* host) {
  return static_cast<TestRenderViewHost*>(host);
}

// static
void RenderViewHostTester::SimulateFirstPaint(RenderViewHost* rvh) {
  static_cast<RenderViewHostImpl*>(rvh)
      ->GetWidget()
      ->OnFirstVisuallyNonEmptyPaint();
}

// static
bool RenderViewHostTester::HasTouchEventHandler(RenderViewHost* rvh) {
  RenderWidgetHostImpl* host_impl =
      RenderWidgetHostImpl::From(rvh->GetWidget());
  return host_impl->has_touch_handler();
}


// RenderViewHostTestEnabler --------------------------------------------------

RenderViewHostTestEnabler::RenderViewHostTestEnabler()
    : rph_factory_(new MockRenderProcessHostFactory()),
      rvh_factory_(new TestRenderViewHostFactory(rph_factory_.get())),
      rfh_factory_(new TestRenderFrameHostFactory()),
      rwhi_factory_(new TestRenderWidgetHostFactory()) {
  // A TaskEnvironment is needed on the main thread for Mojo bindings to
  // graphics services. Some tests have their own, so this only creates one
  // (single-threaded) when none exists. This means tests must ensure any
  // TaskEnvironment they make is created before the RenderViewHostTestEnabler.
  if (!base::MessageLoopCurrent::Get()) {
    task_environment_ =
        std::make_unique<base::test::SingleThreadTaskEnvironment>();
  }
#if !defined(OS_ANDROID)
  ImageTransportFactory::SetFactory(
      std::make_unique<TestImageTransportFactory>());
#else
  if (!screen_)
    screen_.reset(ui::CreateDummyScreenAndroid());
  display::Screen::SetScreenInstance(screen_.get());
#endif
#if defined(OS_MACOSX)
  if (base::ThreadTaskRunnerHandle::IsSet())
    ui::WindowResizeHelperMac::Get()->Init(base::ThreadTaskRunnerHandle::Get());
#endif  // OS_MACOSX
}

RenderViewHostTestEnabler::~RenderViewHostTestEnabler() {
#if defined(OS_MACOSX)
  ui::WindowResizeHelperMac::Get()->ShutdownForTests();
#endif  // OS_MACOSX
#if !defined(OS_ANDROID)
  // RenderWidgetHostView holds on to a reference to SurfaceManager, so it
  // must be shut down before the ImageTransportFactory.
  ImageTransportFactory::Terminate();
#else
  display::Screen::SetScreenInstance(nullptr);
#endif
}


// RenderViewHostTestHarness --------------------------------------------------

RenderViewHostTestHarness::~RenderViewHostTestHarness() {
}

NavigationController& RenderViewHostTestHarness::controller() {
  return web_contents()->GetController();
}

WebContents* RenderViewHostTestHarness::web_contents() {
  return contents_.get();
}

RenderViewHost* RenderViewHostTestHarness::rvh() {
  RenderViewHost* result = web_contents()->GetRenderViewHost();
  CHECK_EQ(result, web_contents()->GetMainFrame()->GetRenderViewHost());
  return result;
}

RenderViewHost* RenderViewHostTestHarness::pending_rvh() {
  return pending_main_rfh() ? pending_main_rfh()->GetRenderViewHost() : nullptr;
}

RenderViewHost* RenderViewHostTestHarness::active_rvh() {
  return pending_rvh() ? pending_rvh() : rvh();
}

RenderFrameHost* RenderViewHostTestHarness::main_rfh() {
  return web_contents()->GetMainFrame();
}

RenderFrameHost* RenderViewHostTestHarness::pending_main_rfh() {
  return static_cast<TestWebContents*>(web_contents())->GetPendingMainFrame();
}

BrowserContext* RenderViewHostTestHarness::browser_context() {
  return GetBrowserContext();
}

MockRenderProcessHost* RenderViewHostTestHarness::process() {
  return static_cast<MockRenderProcessHost*>(active_rvh()->GetProcess());
}

void RenderViewHostTestHarness::DeleteContents() {
  contents_.reset();
}

void RenderViewHostTestHarness::SetContents(
    std::unique_ptr<WebContents> contents) {
  contents_ = std::move(contents);
}

std::unique_ptr<WebContents>
RenderViewHostTestHarness::CreateTestWebContents() {
// Make sure we ran SetUp() already.
#if defined(OS_WIN)
  DCHECK(ole_initializer_ != NULL);
#endif
#if defined(USE_AURA)
  DCHECK(aura_test_helper_ != nullptr);
#endif

  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(GetBrowserContext());
  instance->GetProcess()->Init();

  return TestWebContents::Create(GetBrowserContext(), std::move(instance));
}
void RenderViewHostTestHarness::FocusWebContentsOnMainFrame() {
  TestWebContents* contents = static_cast<TestWebContents*>(web_contents());
  auto* root = contents->GetFrameTree()->root();
  contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());
}

void RenderViewHostTestHarness::NavigateAndCommit(
    const GURL& url,
    ui::PageTransition transition) {
  static_cast<TestWebContents*>(web_contents())
      ->NavigateAndCommit(url, transition);
}

void RenderViewHostTestHarness::SetUp() {
  ui::MaterialDesignController::Initialize();

  rvh_test_enabler_.reset(new RenderViewHostTestEnabler);
  if (factory_)
    rvh_test_enabler_->rvh_factory_->set_render_process_host_factory(factory_);

#if defined(OS_WIN)
  ole_initializer_.reset(new ui::ScopedOleInitializer());
#endif
#if defined(USE_AURA)
  ui::ContextFactory* context_factory =
      ImageTransportFactory::GetInstance()->GetContextFactory();
  ui::ContextFactoryPrivate* context_factory_private =
      ImageTransportFactory::GetInstance()->GetContextFactoryPrivate();

  aura_test_helper_.reset(new aura::test::AuraTestHelper());
  aura_test_helper_->SetUp(context_factory, context_factory_private);
  new wm::DefaultActivationClient(aura_test_helper_->root_window());
#endif

  sanity_checker_.reset(new ContentBrowserSanityChecker());

#if !defined(OS_ANDROID)
  network_change_notifier_ = net::test::MockNetworkChangeNotifier::Create();
#endif

  DCHECK(!browser_context_);
  browser_context_ = CreateBrowserContext();

  SetContents(CreateTestWebContents());
  BrowserSideNavigationSetUp();
}

void RenderViewHostTestHarness::TearDown() {
  BrowserSideNavigationTearDown();
  DeleteContents();
#if defined(USE_AURA)
  aura_test_helper_->TearDown();
#endif
  // Make sure that we flush any messages related to WebContentsImpl destruction
  // before we destroy the browser context.
  base::RunLoop().RunUntilIdle();

#if defined(OS_WIN)
  ole_initializer_.reset();
#endif

  // Delete any RenderProcessHosts before the BrowserContext goes away.
  if (rvh_test_enabler_->rph_factory_) {
    auto render_widget_hosts = RenderWidgetHost::GetRenderWidgetHosts();
    ASSERT_EQ(nullptr, render_widget_hosts->GetNextHost()) <<
        "Test is leaking at least one RenderWidgetHost.";
    rvh_test_enabler_->rph_factory_.reset();
  }

  rvh_test_enabler_.reset();

  // Release the browser context by posting itself on the end of the task
  // queue. This is preferable to immediate deletion because it will behave
  // properly if the |rph_factory_| reset above enqueued any tasks which
  // depend on |browser_context_|.
  base::DeleteSoon(FROM_HERE, {content::BrowserThread::UI},
                   browser_context_.release());

  // Although this isn't required by many, some subclasses members require that
  // the task environment is gone by the time that they are destroyed (akin to
  // browser shutdown).
  task_environment_.reset();
}

std::unique_ptr<BrowserContext>
RenderViewHostTestHarness::CreateBrowserContext() {
  return std::make_unique<TestBrowserContext>();
}

BrowserContext* RenderViewHostTestHarness::GetBrowserContext() {
  return browser_context_.get();
}

void RenderViewHostTestHarness::SetRenderProcessHostFactory(
    RenderProcessHostFactory* factory) {
  if (rvh_test_enabler_)
    rvh_test_enabler_->rvh_factory_->set_render_process_host_factory(factory);
  else
    factory_ = factory;
}

RenderViewHostTestHarness::RenderViewHostTestHarness(
    std::unique_ptr<BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {}

}  // namespace content
