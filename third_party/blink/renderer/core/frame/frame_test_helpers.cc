/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"

#include <utility>

#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace frame_test_helpers {

namespace {

// The frame test helpers coordinate frame loads in a carefully choreographed
// dance. Since the parser is threaded, simply spinning the run loop once is not
// enough to ensure completion of a load. Instead, the following pattern is
// used to ensure that tests see the final state:
// 1. Starts a load.
// 2. Enter the run loop.
// 3. Posted task triggers the load, and starts pumping pending resource
//    requests using runServeAsyncRequestsTask().
// 4. TestWebFrameClient watches for DidStartLoading/DidStopLoading calls,
//    keeping track of how many loads it thinks are in flight.
// 5. While RunServeAsyncRequestsTask() observes TestWebFrameClient to still
//    have loads in progress, it posts itself back to the run loop.
// 6. When RunServeAsyncRequestsTask() notices there are no more loads in
//    progress, it exits the run loop.
// 7. At this point, all parsing, resource loads, and layout should be finished.

void RunServeAsyncRequestsTask(scoped_refptr<base::TaskRunner> task_runner) {
  // TODO(kinuko,toyoshim): Create a mock factory and use it instead of
  // getting the platform's one. (crbug.com/751425)
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  if (TestWebFrameClient::IsLoading()) {
    task_runner->PostTask(FROM_HERE,
                          WTF::Bind(&RunServeAsyncRequestsTask, task_runner));
  } else {
    test::ExitRunLoop();
  }
}

// Helper to create a default test client if the supplied client pointer is
// null. The |owned_client| is used to store the client if it must be created.
// In both cases the client to be used is returned.
template <typename T>
T* CreateDefaultClientIfNeeded(T* client, std::unique_ptr<T>& owned_client) {
  if (client)
    return client;
  owned_client = std::make_unique<T>();
  return owned_client.get();
}

std::unique_ptr<WebNavigationParams> BuildDummyNavigationParams() {
  std::unique_ptr<WebNavigationParams> navigation_params =
      std::make_unique<WebNavigationParams>();
  navigation_params->navigation_timings.navigation_start =
      base::TimeTicks::Now();
  navigation_params->navigation_timings.fetch_start = base::TimeTicks::Now();
  return navigation_params;
}

}  // namespace

void LoadFrame(WebLocalFrame* frame, const std::string& url) {
  WebURL web_url(url_test_helpers::ToKURL(url));
  if (web_url.ProtocolIs("javascript")) {
    frame->LoadJavaScriptURL(web_url);
  } else {
    frame->CommitNavigation(
        WebURLRequest(web_url), blink::WebFrameLoadType::kStandard,
        blink::WebHistoryItem(), false, base::UnguessableToken::Create(),
        BuildDummyNavigationParams(), nullptr /* extra_data */);
  }
  PumpPendingRequestsForFrameToLoad(frame);
}

void LoadHTMLString(WebLocalFrame* frame,
                    const std::string& html,
                    const WebURL& base_url) {
  frame->LoadHTMLString(WebData(html.data(), html.size()), base_url);
  PumpPendingRequestsForFrameToLoad(frame);
}

void LoadHistoryItem(WebLocalFrame* frame,
                     const WebHistoryItem& item,
                     mojom::FetchCacheMode cache_mode) {
  HistoryItem* history_item = item;
  frame->CommitNavigation(
      WrappedResourceRequest(history_item->GenerateResourceRequest(cache_mode)),
      WebFrameLoadType::kBackForward, item, false /* is_client_redirect */,
      base::UnguessableToken::Create(), BuildDummyNavigationParams(),
      nullptr /* extra_data */);
  PumpPendingRequestsForFrameToLoad(frame);
}

void ReloadFrame(WebLocalFrame* frame) {
  frame->StartReload(WebFrameLoadType::kReload);
  PumpPendingRequestsForFrameToLoad(frame);
}

void ReloadFrameBypassingCache(WebLocalFrame* frame) {
  frame->StartReload(WebFrameLoadType::kReloadBypassingCache);
  PumpPendingRequestsForFrameToLoad(frame);
}

void PumpPendingRequestsForFrameToLoad(WebLocalFrame* frame) {
  scoped_refptr<base::TaskRunner> task_runner =
      frame->GetTaskRunner(blink::TaskType::kInternalTest);
  task_runner->PostTask(FROM_HERE,
                        WTF::Bind(&RunServeAsyncRequestsTask, task_runner));
  test::EnterRunLoop();
}

WebMouseEvent CreateMouseEvent(WebInputEvent::Type type,
                               WebMouseEvent::Button button,
                               const IntPoint& point,
                               int modifiers) {
  WebMouseEvent result(type, modifiers,
                       WebInputEvent::GetStaticTimeStampForTests());
  result.pointer_type = WebPointerProperties::PointerType::kMouse;
  result.SetPositionInWidget(point.X(), point.Y());
  result.SetPositionInScreen(point.X(), point.Y());
  result.button = button;
  result.click_count = 1;
  return result;
}

WebLocalFrameImpl* CreateLocalChild(WebLocalFrame& parent,
                                    WebTreeScopeType scope,
                                    TestWebFrameClient* client) {
  std::unique_ptr<TestWebFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  WebLocalFrameImpl* frame =
      ToWebLocalFrameImpl(parent.CreateLocalChild(scope, client, nullptr));
  client->Bind(frame, std::move(owned_client));
  return frame;
}

WebLocalFrameImpl* CreateLocalChild(
    WebLocalFrame& parent,
    WebTreeScopeType scope,
    std::unique_ptr<TestWebFrameClient> self_owned) {
  DCHECK(self_owned);
  TestWebFrameClient* client = self_owned.get();
  WebLocalFrameImpl* frame =
      ToWebLocalFrameImpl(parent.CreateLocalChild(scope, client, nullptr));
  client->Bind(frame, std::move(self_owned));
  return frame;
}

WebLocalFrameImpl* CreateProvisional(WebRemoteFrame& old_frame,
                                     TestWebFrameClient* client) {
  std::unique_ptr<TestWebFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  WebLocalFrameImpl* frame =
      ToWebLocalFrameImpl(WebLocalFrame::CreateProvisional(
          client, nullptr, &old_frame, WebSandboxFlags::kNone,
          ParsedFeaturePolicy()));
  client->Bind(frame, std::move(owned_client));
  // Create a local root, if necessary.
  if (!frame->Parent()) {
    // TODO(dcheng): The main frame widget currently has a special case.
    // Eliminate this once WebView is no longer a WebWidget.
    WebWidgetClient* widget_client =
        frame->ViewImpl()->Client()->WidgetClient();
    WebFrameWidget::CreateForMainFrame(widget_client, frame);
  } else if (frame->Parent()->IsWebRemoteFrame()) {
    auto widget_client = std::make_unique<TestWebWidgetClient>();
    WebFrameWidget* frame_widget =
        WebFrameWidget::CreateForChildLocalRoot(widget_client.get(), frame);
    frame_widget->Resize(WebSize());
    // The WebWidget requires a LayerTreeView to be set, either by the
    // WebWidgetClient itself or by someone else. We do that here.
    frame_widget->SetLayerTreeView(widget_client->layer_tree_view());
    client->BindWidgetClient(std::move(widget_client));
  }
  return frame;
}

WebRemoteFrameImpl* CreateRemote(TestWebRemoteFrameClient* client) {
  std::unique_ptr<TestWebRemoteFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  auto* frame = WebRemoteFrameImpl::Create(WebTreeScopeType::kDocument, client);
  client->Bind(frame, std::move(owned_client));
  return frame;
}

WebLocalFrameImpl* CreateLocalChild(WebRemoteFrame& parent,
                                    const WebString& name,
                                    const WebFrameOwnerProperties& properties,
                                    WebFrame* previous_sibling,
                                    TestWebFrameClient* client,
                                    TestWebWidgetClient* widget_client) {
  std::unique_ptr<TestWebFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  WebLocalFrameImpl* frame = ToWebLocalFrameImpl(parent.CreateLocalChild(
      WebTreeScopeType::kDocument, name, WebSandboxFlags::kNone, client,
      nullptr, previous_sibling, ParsedFeaturePolicy(), properties,
      FrameOwnerElementType::kIframe, nullptr));
  client->Bind(frame, std::move(owned_client));

  std::unique_ptr<TestWebWidgetClient> owned_widget_client;
  widget_client =
      CreateDefaultClientIfNeeded(widget_client, owned_widget_client);
  WebFrameWidget* frame_widget =
      WebFrameWidget::CreateForChildLocalRoot(widget_client, frame);
  // Set an initial size for subframes.
  if (frame->Parent())
    frame_widget->Resize(WebSize());
  // The WebWidget requires a LayerTreeView to be set, either by the
  // WebWidgetClient itself or by someone else. We do that here.
  frame_widget->SetLayerTreeView(widget_client->layer_tree_view());
  client->BindWidgetClient(std::move(owned_widget_client));
  return frame;
}

WebRemoteFrameImpl* CreateRemoteChild(
    WebRemoteFrame& parent,
    const WebString& name,
    scoped_refptr<SecurityOrigin> security_origin,
    TestWebRemoteFrameClient* client) {
  std::unique_ptr<TestWebRemoteFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  auto* frame = ToWebRemoteFrameImpl(parent.CreateRemoteChild(
      WebTreeScopeType::kDocument, name, WebSandboxFlags::kNone,
      ParsedFeaturePolicy(), FrameOwnerElementType::kIframe, client, nullptr));
  client->Bind(frame, std::move(owned_client));
  if (!security_origin)
    security_origin = SecurityOrigin::CreateUniqueOpaque();
  frame->GetFrame()->GetSecurityContext()->SetReplicatedOrigin(
      std::move(security_origin));
  return frame;
}

WebViewHelper::WebViewHelper() : web_view_(nullptr) {}

WebViewHelper::~WebViewHelper() {
  Reset();
}

WebViewImpl* WebViewHelper::InitializeWithOpener(
    WebFrame* opener,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* test_web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  Reset();

  InitializeWebView(web_view_client, opener ? opener->View() : nullptr);
  if (update_settings_func)
    update_settings_func(web_view_->GetSettings());

  std::unique_ptr<TestWebFrameClient> owned_web_frame_client;
  web_frame_client =
      CreateDefaultClientIfNeeded(web_frame_client, owned_web_frame_client);
  WebLocalFrame* frame = WebLocalFrame::CreateMainFrame(
      web_view_, web_frame_client, nullptr, opener);
  web_frame_client->Bind(frame, std::move(owned_web_frame_client));

  // TODO(dcheng): The main frame widget currently has a special case.
  // Eliminate this once WebView is no longer a WebWidget.
  WebWidgetClient* web_widget_client = test_web_widget_client;
  if (!web_widget_client)
    web_widget_client = test_web_view_client_->WidgetClient();
  blink::WebFrameWidget::CreateForMainFrame(web_widget_client, frame);
  // Set an initial size for subframes.
  if (frame->Parent())
    frame->FrameWidget()->Resize(WebSize());

  return web_view_;
}

WebViewImpl* WebViewHelper::Initialize(
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  return InitializeWithOpener(nullptr, web_frame_client, web_view_client,
                              web_widget_client, update_settings_func);
}

WebViewImpl* WebViewHelper::InitializeAndLoad(
    const std::string& url,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  Initialize(web_frame_client, web_view_client, web_widget_client,
             update_settings_func);

  LoadFrame(GetWebView()->MainFrameImpl(), url);

  return GetWebView();
}

WebViewImpl* WebViewHelper::InitializeRemote(
    TestWebRemoteFrameClient* web_remote_frame_client,
    scoped_refptr<SecurityOrigin> security_origin,
    TestWebViewClient* web_view_client) {
  Reset();

  InitializeWebView(web_view_client, nullptr);

  std::unique_ptr<TestWebRemoteFrameClient> owned_web_remote_frame_client;
  web_remote_frame_client = CreateDefaultClientIfNeeded(
      web_remote_frame_client, owned_web_remote_frame_client);
  WebRemoteFrameImpl* frame = WebRemoteFrameImpl::CreateMainFrame(
      web_view_, web_remote_frame_client, nullptr);
  web_remote_frame_client->Bind(frame,
                                std::move(owned_web_remote_frame_client));
  if (!security_origin)
    security_origin = SecurityOrigin::CreateUniqueOpaque();
  frame->GetFrame()->GetSecurityContext()->SetReplicatedOrigin(
      std::move(security_origin));
  return web_view_;
}

void WebViewHelper::LoadAhem() {
  LocalFrame* local_frame =
      ToLocalFrame(WebFrame::ToCoreFrame(*LocalMainFrame()));
  DCHECK(local_frame);
  RenderingTest::LoadAhem(*local_frame);
}

void WebViewHelper::Reset() {
  if (web_view_) {
    DCHECK(!TestWebFrameClient::IsLoading());
    web_view_->Close();
    web_view_ = nullptr;
  }
}

WebLocalFrameImpl* WebViewHelper::LocalMainFrame() const {
  return ToWebLocalFrameImpl(web_view_->MainFrame());
}

WebRemoteFrameImpl* WebViewHelper::RemoteMainFrame() const {
  return ToWebRemoteFrameImpl(web_view_->MainFrame());
}

void WebViewHelper::Resize(WebSize size) {
  test_web_view_client_->ClearAnimationScheduled();
  GetWebView()->Resize(size);
  EXPECT_FALSE(test_web_view_client_->AnimationScheduled());
  test_web_view_client_->ClearAnimationScheduled();
}

void WebViewHelper::InitializeWebView(TestWebViewClient* web_view_client,
                                      class WebView* opener) {
  web_view_client =
      CreateDefaultClientIfNeeded(web_view_client, owned_test_web_view_client_);
  web_view_ = static_cast<WebViewImpl*>(
      WebView::Create(web_view_client, web_view_client,
                      mojom::PageVisibilityState::kVisible, opener));
  web_view_->GetSettings()->SetJavaScriptEnabled(true);
  web_view_->GetSettings()->SetPluginsEnabled(true);
  // Enable (mocked) network loads of image URLs, as this simplifies
  // the completion of resource loads upon test shutdown & helps avoid
  // dormant loads trigger Resource leaks for image loads.
  //
  // Consequently, all external image resources must be mocked.
  web_view_->GetSettings()->SetLoadsImagesAutomatically(true);

  web_view_->SetLayerTreeView(web_view_client->layer_tree_view());
  web_view_->SetDeviceScaleFactor(
      web_view_client->GetScreenInfo().device_scale_factor);
  web_view_->SetDefaultPageScaleLimits(1, 4);

  test_web_view_client_ = web_view_client;
}

int TestWebFrameClient::loads_in_progress_ = 0;

TestWebFrameClient::TestWebFrameClient()
    : interface_provider_(new service_manager::InterfaceProvider()) {}

void TestWebFrameClient::Bind(WebLocalFrame* frame,
                              std::unique_ptr<TestWebFrameClient> self_owned) {
  DCHECK(!frame_);
  DCHECK(!self_owned || self_owned.get() == this);
  frame_ = frame;
  self_owned_ = std::move(self_owned);
}

void TestWebFrameClient::BindWidgetClient(
    std::unique_ptr<WebWidgetClient> client) {
  DCHECK(!owned_widget_client_);
  owned_widget_client_ = std::move(client);
}

void TestWebFrameClient::FrameDetached(DetachType type) {
  if (frame_->FrameWidget()) {
    frame_->FrameWidget()->WillCloseLayerTreeView();
    frame_->FrameWidget()->Close();
  }

  owned_widget_client_.reset();
  frame_->Close();
  self_owned_.reset();
}

WebLocalFrame* TestWebFrameClient::CreateChildFrame(
    WebLocalFrame* parent,
    WebTreeScopeType scope,
    const WebString& name,
    const WebString& fallback_name,
    WebSandboxFlags sandbox_flags,
    const ParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties,
    FrameOwnerElementType owner_type) {
  return CreateLocalChild(*parent, scope);
}

void TestWebFrameClient::DidStartLoading() {
  ++loads_in_progress_;
}

void TestWebFrameClient::DidStopLoading() {
  DCHECK_GT(loads_in_progress_, 0);
  --loads_in_progress_;
}

void TestWebFrameClient::DidCreateDocumentLoader(
    WebDocumentLoader* document_loader) {
}

TestWebRemoteFrameClient::TestWebRemoteFrameClient() = default;

void TestWebRemoteFrameClient::Bind(
    WebRemoteFrame* frame,
    std::unique_ptr<TestWebRemoteFrameClient> self_owned) {
  DCHECK(!frame_);
  DCHECK(!self_owned || self_owned.get() == this);
  frame_ = frame;
  self_owned_ = std::move(self_owned);
}

void TestWebRemoteFrameClient::FrameDetached(DetachType type) {
  frame_->Close();
  self_owned_.reset();
}

content::LayerTreeView* LayerTreeViewFactory::Initialize() {
  return Initialize(/*delegate=*/nullptr);
}

content::LayerTreeView* LayerTreeViewFactory::Initialize(
    content::LayerTreeViewDelegate* specified_delegate) {
  cc::LayerTreeSettings settings;
  // Use synchronous compositing so that the MessageLoop becomes idle and the
  // test makes progress.
  settings.single_thread_proxy_scheduler = false;
  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;
  // Both BlinkGenPropertyTrees and SlimmingPaintV2 should imply layer lists in
  // the compositor. Some code across the boundaries makes assumptions based on
  // this so ensure tests run using this configuration as well.
  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() ||
      RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    settings.use_layer_lists = true;
  }

  layer_tree_view_ = std::make_unique<content::LayerTreeView>(
      specified_delegate ? specified_delegate : &delegate_,
      Platform::Current()->CurrentThread()->GetTaskRunner(),
      /*compositor_thread=*/nullptr, &test_task_graph_runner_,
      &fake_renderer_scheduler_);
  layer_tree_view_->Initialize(settings,
                               std::make_unique<cc::TestUkmRecorderFactory>());
  return layer_tree_view_.get();
}

TestWebWidgetClient::TestWebWidgetClient() {
  layer_tree_view_ = layer_tree_view_factory_.Initialize();
}

TestWebViewClient::TestWebViewClient(content::LayerTreeViewDelegate* delegate) {
  layer_tree_view_ = layer_tree_view_factory_.Initialize(delegate);
}

}  // namespace frame_test_helpers
}  // namespace blink
