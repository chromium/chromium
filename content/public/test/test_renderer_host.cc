// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_renderer_host.h"

#include <utility>

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_image_transport_factory.h"
#include "content/public/test/test_utils.h"
#include "content/test/content_browser_consistency_checker.h"
#include "content/test/test_navigation_url_loader_factory.h"
#include "content/test/test_page_factory.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_frame_host_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_render_widget_host_factory.h"
#include "content/test/test_web_contents.h"
#include "net/base/mock_network_change_notifier.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "ui/display/screen.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/dummy_screen_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

#if BUILDFLAG(IS_MAC)
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
  auto navigation = NavigationSimulator::CreateFromPending(*controller);
  navigation->Commit();
}

// RenderViewHostTester -------------------------------------------------------

// static
RenderViewHostTester* RenderViewHostTester::For(RenderViewHost* host) {
  return static_cast<TestRenderViewHost*>(host);
}

// static
std::unique_ptr<content::InputMsgWatcher>
RenderViewHostTester::CreateInputWatcher(RenderViewHost* rvh,
                                         blink::WebInputEvent::Type type) {
  RenderWidgetHostImpl* host_impl =
      RenderWidgetHostImpl::From(rvh->GetWidget());
  return std::make_unique<content::InputMsgWatcher>(host_impl, type);
}

// static
void RenderViewHostTester::SendTouchEvent(
    RenderViewHost* rvh,
    blink::SyntheticWebTouchEvent* touch_event) {
  RenderWidgetHostImpl* host_impl =
      RenderWidgetHostImpl::From(rvh->GetWidget());
  auto* input_event_router = host_impl->delegate()->GetInputEventRouter();
  input_event_router->RouteTouchEvent(host_impl->GetView(), touch_event,
                                      ui::LatencyInfo());
}

// RenderViewHostTestEnabler --------------------------------------------------

RenderViewHostTestEnabler::RenderViewHostTestEnabler(
    NavigationURLLoaderFactoryType url_loader_factory_type)
    : rph_factory_(new MockRenderProcessHostFactory()),
      asgh_factory_(new MockAgentSchedulingGroupHostFactory()),
      page_factory_(new TestPageFactory()),
      rvh_factory_(new TestRenderViewHostFactory(rph_factory_.get(),
                                                 asgh_factory_.get())),
      rfh_factory_(new TestRenderFrameHostFactory()),
      rwhi_factory_(new TestRenderWidgetHostFactory()),
      loader_factory_(url_loader_factory_type ==
                              NavigationURLLoaderFactoryType::kTest
                          ? new TestNavigationURLLoaderFactory()
                          : nullptr) {
  // A TaskEnvironment is needed on the main thread for Mojo bindings to
  // graphics services. Some tests have their own, so this only creates one
  // (single-threaded) when none exists. This means tests must ensure any
  // TaskEnvironment they make is created before the RenderViewHostTestEnabler.
  if (!base::CurrentThread::Get()) {
    task_environment_ =
        std::make_unique<base::test::SingleThreadTaskEnvironment>();
  }
#if !BUILDFLAG(IS_ANDROID)
  ImageTransportFactory::SetFactory(
      std::make_unique<TestImageTransportFactory>());
#else
  if (!screen_)
    screen_.reset(ui::CreateDummyScreenAndroid());
  display::Screen::SetScreenInstance(screen_.get());
#endif
#if BUILDFLAG(IS_MAC)
  if (base::SingleThreadTaskRunner::HasCurrentDefault())
    ui::WindowResizeHelperMac::Get()->Init(
        base::SingleThreadTaskRunner::GetCurrentDefault());
#endif  // BUILDFLAG(IS_MAC)
}

RenderViewHostTestEnabler::~RenderViewHostTestEnabler() {
#if BUILDFLAG(IS_MAC)
  ui::WindowResizeHelperMac::Get()->ShutdownForTests();
#endif  // BUILDFLAG(IS_MAC)
#if !BUILDFLAG(IS_ANDROID)
  // RenderWidgetHostView holds on to a reference to SurfaceManager, so it
  // must be shut down before the ImageTransportFactory.
  ImageTransportFactory::Terminate();
#else
  display::Screen::SetScreenInstance(nullptr);
#endif
}


// RenderViewHostTestHarness --------------------------------------------------

RenderViewHostTestHarness::~RenderViewHostTestHarness() {
  DCHECK(!task_environment_) << "TearDown() was not called.";
}

NavigationController& RenderViewHostTestHarness::controller() {
  return web_contents()->GetController();
}

WebContents* RenderViewHostTestHarness::web_contents() const {
  return contents_.get();
}

RenderViewHost* RenderViewHostTestHarness::rvh() {
  return web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
}

RenderFrameHost* RenderViewHostTestHarness::main_rfh() {
  return web_contents()->GetPrimaryMainFrame();
}

BrowserContext* RenderViewHostTestHarness::browser_context() {
  return GetBrowserContext();
}

MockRenderProcessHost* RenderViewHostTestHarness::process() {
  auto* contents = static_cast<TestWebContents*>(web_contents());
  return contents->GetPrimaryMainFrame()->GetProcess();
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
#if BUILDFLAG(IS_WIN)
  DCHECK(ole_initializer_);
#endif
#if defined(USE_AURA)
  DCHECK(aura_test_helper_);
#endif

  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(GetBrowserContext());
  instance->GetProcess()->Init();

  return TestWebContents::Create(GetBrowserContext(), std::move(instance));
}
void RenderViewHostTestHarness::FocusWebContentsOnMainFrame() {
  FocusWebContentsOnFrame(web_contents()->GetPrimaryMainFrame());
}

void RenderViewHostTestHarness::FocusWebContentsOnFrame(
    content::RenderFrameHost* rfh) {
  content::FocusWebContentsOnFrame(web_contents(), rfh);
}

void RenderViewHostTestHarness::NavigateAndCommit(
    const GURL& url,
    ui::PageTransition transition) {
  static_cast<TestWebContents*>(web_contents())
      ->NavigateAndCommit(url, transition);
}

void RenderViewHostTestHarness::SetUp() {
  rvh_test_enabler_ = std::make_unique<RenderViewHostTestEnabler>();
  if (factory_)
    rvh_test_enabler_->rvh_factory_->set_render_process_host_factory(factory_);

#if BUILDFLAG(IS_WIN)
  ole_initializer_ = std::make_unique<ui::ScopedOleInitializer>();
#endif
#if BUILDFLAG(IS_MAC)
  screen_ = std::make_unique<display::ScopedNativeScreen>();
#endif

#if defined(USE_AURA)
  aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
      ImageTransportFactory::GetInstance()->GetContextFactory());
  aura_test_helper_->SetUp();
#endif

  consistency_checker_ = std::make_unique<ContentBrowserConsistencyChecker>();

  network_change_notifier_ = net::test::MockNetworkChangeNotifier::Create();

  DCHECK(!browser_context_);
  browser_context_ = CreateBrowserContext();

  SetContents(CreateTestWebContents());

  // Create GpuDataManagerImpl here so it always runs on the main thread.
  GpuDataManagerImpl::GetInstance();
}

void RenderViewHostTestHarness::TearDown() {
  DeleteContents();
#if defined(USE_AURA)
  aura_test_helper_->TearDown();
#endif
  // Make sure that we flush any messages related to WebContentsImpl destruction
  // before we destroy the browser context.
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_WIN)
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
  GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, browser_context_.release());

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
