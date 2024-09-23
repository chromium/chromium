// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

SimTest::SimTest(
    std::optional<base::test::TaskEnvironment::TimeSource> time_source)
    : task_environment_(
          time_source.has_value()
              ? time_source.value()
              : base::test::TaskEnvironment::TimeSource::DEFAULT) {
  Document::SetForceSynchronousParsingForTesting(true);
  // Threaded animations are usually enabled for blink. However these tests use
  // synchronous compositing, which can not run threaded animations.
  bool was_threaded_animation_enabled =
      content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(false);
  // If this fails, we'd be resetting IsThreadedAnimationEnabled() to the wrong
  // thing in the destructor.
  DCHECK(was_threaded_animation_enabled);
}

SimTest::~SimTest() {
  // Clear lazily loaded style sheets.
  CSSDefaultStyleSheets::Instance().PrepareForLeakDetection();
  Document::SetForceSynchronousParsingForTesting(false);
  content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(true);
  WebCache::Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
}

void SimTest::SetUp() {
  Test::SetUp();

  // SimCompositor overrides the LayerTreeViewDelegate to respond to
  // BeginMainFrame(), which will update and paint the main frame of the
  // WebViewImpl given to SetWebView().
  network_ = std::make_unique<SimNetwork>();
  compositor_ = std::make_unique<SimCompositor>();
  web_frame_client_ = CreateWebFrameClientForMainFrame();
  page_ = std::make_unique<SimPage>();
  web_view_helper_ =
      std::make_unique<frame_test_helpers::WebViewHelper>(WTF::BindRepeating(
          &SimTest::CreateWebFrameWidget, base::Unretained(this)));
  // These tests don't simulate a browser interface and hence fetching code
  // caching doesn't work in these tests. Currently tests that use this testing
  // set up don't test / need code caches. Disable code caches for these tests.
  DocumentLoader::DisableCodeCacheForTesting();

  web_view_helper_->Initialize(web_frame_client_.get());
  compositor_->SetWebView(WebView());
  page_->SetPage(WebView().GetPage());
  local_frame_root_ = WebView().MainFrameImpl();
  compositor_->SetLayerTreeHost(
      local_frame_root_->FrameWidgetImpl()->LayerTreeHostForTesting());

  ResizeView(gfx::Size(300, 200));
}

void SimTest::TearDown() {
  // Pump the message loop to process the load event.
  //
  // Use RunUntilIdle() instead of blink::test::RunPendingTask(), because
  // blink::test::RunPendingTask() posts directly to
  // scheduler::GetSingleThreadTaskRunnerForTesting(), which makes it
  // incompatible with a TestingPlatformSupportWithMockScheduler.
  base::RunLoop().RunUntilIdle();

  // Shut down this stuff before settings change to keep the world
  // consistent, and before the subclass tears down.
  web_view_helper_.reset();
  page_.reset();
  web_frame_client_.reset();
  compositor_.reset();
  network_.reset();
  local_frame_root_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

void SimTest::InitializeRemote() {
  web_view_helper_->InitializeRemote();
  compositor_->SetWebView(WebView());
  page_->SetPage(WebView().GetPage());
  web_frame_client_ =
      std::make_unique<frame_test_helpers::TestWebFrameClient>();
  local_frame_root_ = web_view_helper_->CreateLocalChild(
      *WebView().MainFrame()->ToWebRemoteFrame(), "local_frame_root",
      WebFrameOwnerProperties(), nullptr, web_frame_client_.get());
  compositor_->SetLayerTreeHost(
      local_frame_root_->FrameWidgetImpl()->LayerTreeHostForTesting());
}

void SimTest::InitializeFencedFrameRoot(
    blink::FencedFrame::DeprecatedFencedFrameMode mode) {
  web_view_helper_->InitializeWithOpener(/*opener=*/nullptr,
                                         /*frame_client=*/nullptr,
                                         /*view_client=*/nullptr,
                                         /*update_settings_func=*/nullptr,
                                         mode);
  compositor_->SetWebView(WebView());
  page_->SetPage(WebView().GetPage());
  web_frame_client_ =
      std::make_unique<frame_test_helpers::TestWebFrameClient>();
  local_frame_root_ = WebView().MainFrameImpl();
  compositor_->SetLayerTreeHost(
      local_frame_root_->FrameWidgetImpl()->LayerTreeHostForTesting());
}

void SimTest::InitializePrerenderPageRoot() {
  web_view_helper_->InitializeWithOpener(
      /*opener=*/nullptr,
      /*frame_client=*/nullptr,
      /*view_client=*/nullptr,
      /*update_settings_func=*/nullptr,
      /*fenced_frame_mode=*/std::nullopt,
      /*is_prerendering=*/true);
  compositor_->SetWebView(WebView());
  page_->SetPage(WebView().GetPage());
  web_frame_client_ =
      std::make_unique<frame_test_helpers::TestWebFrameClient>();
  local_frame_root_ = WebView().MainFrameImpl();
  compositor_->SetLayerTreeHost(
      local_frame_root_->FrameWidgetImpl()->LayerTreeHostForTesting());
}

void SimTest::LoadURL(const String& url_string) {
  KURL url(url_string);
  frame_test_helpers::LoadFrameDontWait(local_frame_root_.Get(), url);
  if (DocumentLoader::WillLoadUrlAsEmpty(url) || url.ProtocolIsData()) {
    // Empty documents and data urls are not using mocked out SimRequests,
    // but instead load data directly.
    frame_test_helpers::PumpPendingRequestsForFrameToLoad(
        local_frame_root_.Get());
  }
}

LocalDOMWindow& SimTest::Window() {
  return *GetDocument().domWindow();
}

SimPage& SimTest::GetPage() {
  return *page_;
}

Document& SimTest::GetDocument() {
  return *WebView().MainFrameImpl()->GetFrame()->GetDocument();
}

WebViewImpl& SimTest::WebView() {
  return *web_view_helper_->GetWebView();
}

WebLocalFrameImpl& SimTest::MainFrame() {
  return *WebView().MainFrameImpl();
}

WebLocalFrameImpl& SimTest::LocalFrameRoot() {
  return *local_frame_root_;
}

frame_test_helpers::TestWebFrameClient& SimTest::WebFrameClient() {
  return *web_frame_client_;
}

frame_test_helpers::TestWebFrameWidget& SimTest::GetWebFrameWidget() {
  return *static_cast<frame_test_helpers::TestWebFrameWidget*>(
      local_frame_root_->FrameWidgetImpl());
}

SimCompositor& SimTest::Compositor() {
  return *compositor_;
}

frame_test_helpers::WebViewHelper& SimTest::WebViewHelper() {
  return *web_view_helper_;
}

Vector<String>& SimTest::ConsoleMessages() {
  return web_frame_client_->ConsoleMessages();
}

void SimTest::ResizeView(const gfx::Size& size) {
  web_view_helper_->Resize(size);
}

frame_test_helpers::TestWebFrameWidget* SimTest::CreateWebFrameWidget(
    base::PassKey<WebLocalFrame> pass_key,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page) {
  return MakeGarbageCollected<frame_test_helpers::TestWebFrameWidget>(
      std::move(pass_key), std::move(frame_widget_host),
      std::move(frame_widget), std::move(widget_host), std::move(widget),
      std::move(task_runner), frame_sink_id, hidden, never_composited,
      is_for_child_local_root, is_for_nested_main_frame, is_for_scalable_page);
}

std::unique_ptr<frame_test_helpers::TestWebFrameClient>
SimTest::CreateWebFrameClientForMainFrame() {
  return std::make_unique<frame_test_helpers::TestWebFrameClient>();
}

void SimTest::SetPreferCompositingToLCDText(bool enabled) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextForTesting(enabled);
}

}  // namespace blink
