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

#include <memory>
#include <utility>

#include "base/bind.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/fake_web_plugin.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
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

}  // namespace

void LoadFrameDontWait(WebLocalFrame* frame, const WebURL& url) {
  auto* impl = To<WebLocalFrameImpl>(frame);
  if (url.ProtocolIs("javascript")) {
    impl->LoadJavaScriptURL(url);
  } else {
    auto params = std::make_unique<WebNavigationParams>();
    params->url = url;
    params->navigation_timings.navigation_start = base::TimeTicks::Now();
    params->navigation_timings.fetch_start = base::TimeTicks::Now();
    params->is_browser_initiated = true;
    FillNavigationParamsResponse(params.get());
    impl->CommitNavigation(
        std::move(params), nullptr /* extra_data */,
        base::DoNothing::Once() /* call_before_attaching_new_document */);
  }
}

void LoadFrame(WebLocalFrame* frame, const std::string& url) {
  LoadFrameDontWait(frame, url_test_helpers::ToKURL(url));
  PumpPendingRequestsForFrameToLoad(frame);
}

void LoadHTMLString(WebLocalFrame* frame,
                    const std::string& html,
                    const WebURL& base_url,
                    const base::TickClock* clock) {
  auto* impl = To<WebLocalFrameImpl>(frame);
  std::unique_ptr<WebNavigationParams> navigation_params =
      WebNavigationParams::CreateWithHTMLString(html, base_url);
  navigation_params->tick_clock = clock;
  impl->CommitNavigation(
      std::move(navigation_params), nullptr /* extra_data */,
      base::DoNothing::Once() /* call_before_attaching_new_document */);
  PumpPendingRequestsForFrameToLoad(frame);
}

void LoadHistoryItem(WebLocalFrame* frame,
                     const WebHistoryItem& item,
                     mojom::FetchCacheMode cache_mode) {
  auto* impl = To<WebLocalFrameImpl>(frame);
  HistoryItem* history_item = item;
  auto params = std::make_unique<WebNavigationParams>();
  params->url = history_item->Url();
  params->frame_load_type = WebFrameLoadType::kBackForward;
  params->history_item = item;
  params->navigation_timings.navigation_start = base::TimeTicks::Now();
  params->navigation_timings.fetch_start = base::TimeTicks::Now();
  FillNavigationParamsResponse(params.get());
  impl->CommitNavigation(
      std::move(params), nullptr /* extra_data */,
      base::DoNothing::Once() /* call_before_attaching_new_document */);
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

void FillNavigationParamsResponse(WebNavigationParams* params) {
  KURL kurl(params->url);
  // Empty documents and srcdoc will be handled by DocumentLoader.
  if (DocumentLoader::WillLoadUrlAsEmpty(kurl) || kurl.IsAboutSrcdocURL())
    return;
  Platform::Current()->GetURLLoaderMockFactory()->FillNavigationParamsResponse(
      params);
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
  auto* frame =
      To<WebLocalFrameImpl>(parent.CreateLocalChild(scope, client, nullptr));
  client->Bind(frame, std::move(owned_client));
  return frame;
}

WebLocalFrameImpl* CreateLocalChild(
    WebLocalFrame& parent,
    WebTreeScopeType scope,
    std::unique_ptr<TestWebFrameClient> self_owned) {
  DCHECK(self_owned);
  TestWebFrameClient* client = self_owned.get();
  auto* frame =
      To<WebLocalFrameImpl>(parent.CreateLocalChild(scope, client, nullptr));
  client->Bind(frame, std::move(self_owned));
  return frame;
}

WebLocalFrameImpl* CreateProvisional(WebRemoteFrame& old_frame,
                                     TestWebFrameClient* client) {
  std::unique_ptr<TestWebFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  auto* frame = To<WebLocalFrameImpl>(WebLocalFrame::CreateProvisional(
      client, nullptr, &old_frame, FramePolicy()));
  client->Bind(frame, std::move(owned_client));
  std::unique_ptr<TestWebWidgetClient> widget_client;
  // Create a local root, if necessary.
  if (!frame->Parent()) {
    widget_client = std::make_unique<TestWebWidgetClient>();
    // TODO(dcheng): The main frame widget currently has a special case.
    // Eliminate this once WebView is no longer a WebWidget.
    WebFrameWidget::CreateForMainFrame(widget_client.get(), frame);
  } else if (frame->Parent()->IsWebRemoteFrame()) {
    widget_client = std::make_unique<TestWebWidgetClient>();
    WebFrameWidget* frame_widget =
        WebFrameWidget::CreateForChildLocalRoot(widget_client.get(), frame);
    // The WebWidget requires an AnimationHost to be set, either by the
    // WebWidgetClient itself or by someone else. We do that here.
    frame_widget->SetAnimationHost(widget_client->animation_host());
    frame_widget->Resize(WebSize());
  }
  if (widget_client)
    client->BindWidgetClient(std::move(widget_client));
  return frame;
}

WebRemoteFrameImpl* CreateRemote(TestWebRemoteFrameClient* client) {
  std::unique_ptr<TestWebRemoteFrameClient> owned_client;
  client = CreateDefaultClientIfNeeded(client, owned_client);
  auto* frame = MakeGarbageCollected<WebRemoteFrameImpl>(
      WebTreeScopeType::kDocument, client,
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      client->GetAssociatedInterfaceProvider());
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
  auto* frame = To<WebLocalFrameImpl>(parent.CreateLocalChild(
      WebTreeScopeType::kDocument, name, FramePolicy(), client, nullptr,
      previous_sibling, properties, FrameOwnerElementType::kIframe, nullptr));
  client->Bind(frame, std::move(owned_client));

  std::unique_ptr<TestWebWidgetClient> owned_widget_client;
  widget_client =
      CreateDefaultClientIfNeeded(widget_client, owned_widget_client);
  WebFrameWidget* frame_widget =
      WebFrameWidget::CreateForChildLocalRoot(widget_client, frame);
  // The WebWidget requires an AnimationHost to be set, either by the
  // WebWidgetClient itself or by someone else. We do that here.
  frame_widget->SetAnimationHost(widget_client->animation_host());
  // Set an initial size for subframes.
  if (frame->Parent())
    frame_widget->Resize(WebSize());
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
      WebTreeScopeType::kDocument, name, FramePolicy(),
      FrameOwnerElementType::kIframe, client,
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      client->GetAssociatedInterfaceProvider(), nullptr));
  client->Bind(frame, std::move(owned_client));
  if (!security_origin)
    security_origin = SecurityOrigin::CreateUniqueOpaque();
  frame->GetFrame()->GetSecurityContext()->SetReplicatedOrigin(
      std::move(security_origin));
  return frame;
}

WebViewHelper::WebViewHelper()
    : web_view_(nullptr), platform_(Platform::Current()) {}

WebViewHelper::~WebViewHelper() {
  // Close the WebViewImpl before the WebViewClient/WebWidgetClient are
  // destroyed.
  Reset();
}

WebViewImpl* WebViewHelper::InitializeWithOpener(
    WebFrame* opener,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
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

  test_web_widget_client_ = CreateDefaultClientIfNeeded(
      web_widget_client, owned_test_web_widget_client_);
  // TODO(danakj): Make this part of attaching the main frame's WebFrameWidget.
  // This happens before CreateForMainFrame as the WebFrameWidget binding to the
  // WebLocalFrameImpl sets up animations.
  web_view_->SetAnimationHost(test_web_widget_client_->animation_host());
  // TODO(dcheng): The main frame widget currently has a special case.
  // Eliminate this once WebView is no longer a WebWidget.
  blink::WebFrameWidget::CreateForMainFrame(test_web_widget_client_, frame);
  // We inform the WebView when it has a local main frame attached once the
  // WebFrame it fully set up and the WebWidgetClient is initialized (which is
  // the case by this point).
  web_view_->DidAttachLocalMainFrame();

  web_view_->SetDeviceScaleFactor(
      test_web_widget_client_->GetScreenInfo().device_scale_factor);

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

WebViewImpl* WebViewHelper::InitializeWithSettings(
    void (*update_settings_func)(WebSettings*)) {
  return InitializeWithOpener(nullptr, nullptr, nullptr, nullptr,
                              update_settings_func);
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
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client) {
  Reset();

  InitializeWebView(web_view_client, nullptr);

  std::unique_ptr<TestWebRemoteFrameClient> owned_web_remote_frame_client;
  web_remote_frame_client = CreateDefaultClientIfNeeded(
      web_remote_frame_client, owned_web_remote_frame_client);
  WebRemoteFrameImpl* frame = WebRemoteFrameImpl::CreateMainFrame(
      web_view_, web_remote_frame_client,
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      web_remote_frame_client->GetAssociatedInterfaceProvider(), nullptr);
  web_remote_frame_client->Bind(frame,
                                std::move(owned_web_remote_frame_client));
  if (!security_origin)
    security_origin = SecurityOrigin::CreateUniqueOpaque();
  frame->GetFrame()->GetSecurityContext()->SetReplicatedOrigin(
      std::move(security_origin));

  test_web_widget_client_ = CreateDefaultClientIfNeeded(
      web_widget_client, owned_test_web_widget_client_);
  web_view_->SetAnimationHost(test_web_widget_client_->animation_host());

  return web_view_;
}

void WebViewHelper::LoadAhem() {
  LocalFrame* local_frame =
      To<LocalFrame>(WebFrame::ToCoreFrame(*LocalMainFrame()));
  DCHECK(local_frame);
  RenderingTest::LoadAhem(*local_frame);
}

void WebViewHelper::Reset() {
  DCHECK_EQ(platform_, Platform::Current())
      << "Platform::Current() should be the same for the life of a test, "
         "including shutdown.";

  if (test_web_view_client_)
    test_web_view_client_->DestroyChildViews();
  if (web_view_) {
    DCHECK(!TestWebFrameClient::IsLoading());
    web_view_->Close();
    web_view_ = nullptr;
  }
  test_web_view_client_ = nullptr;
}

WebLocalFrameImpl* WebViewHelper::LocalMainFrame() const {
  return To<WebLocalFrameImpl>(web_view_->MainFrame());
}

WebRemoteFrameImpl* WebViewHelper::RemoteMainFrame() const {
  return ToWebRemoteFrameImpl(web_view_->MainFrame());
}

void WebViewHelper::Resize(WebSize size) {
  GetWebView()->Resize(size);
}

void WebViewHelper::InitializeWebView(TestWebViewClient* web_view_client,
                                      class WebView* opener) {
  test_web_view_client_ =
      CreateDefaultClientIfNeeded(web_view_client, owned_test_web_view_client_);
  web_view_ = static_cast<WebViewImpl*>(
      WebView::Create(test_web_view_client_,
                      /*is_hidden=*/false,
                      /*compositing_enabled=*/true, opener));
  // This property must be set at initialization time, it is not supported to be
  // changed afterward, and does nothing.
  web_view_->GetSettings()->SetViewportEnabled(viewport_enabled_);
  web_view_->GetSettings()->SetJavaScriptEnabled(true);
  web_view_->GetSettings()->SetPluginsEnabled(true);
  // Enable (mocked) network loads of image URLs, as this simplifies
  // the completion of resource loads upon test shutdown & helps avoid
  // dormant loads trigger Resource leaks for image loads.
  //
  // Consequently, all external image resources must be mocked.
  web_view_->GetSettings()->SetLoadsImagesAutomatically(true);

  // If a test turned off this settings, opened WebViews should propagate that.
  if (opener) {
    web_view_->GetSettings()->SetAllowUniversalAccessFromFileURLs(
        static_cast<WebViewImpl*>(opener)
            ->GetPage()
            ->GetSettings()
            .GetAllowUniversalAccessFromFileURLs());
  }

  web_view_->SetDefaultPageScaleLimits(1, 4);
}

int TestWebFrameClient::loads_in_progress_ = 0;

TestWebFrameClient::TestWebFrameClient()
    : interface_provider_(new service_manager::InterfaceProvider()),
      associated_interface_provider_(new AssociatedInterfaceProvider(nullptr)),
      effective_connection_type_(WebEffectiveConnectionType::kTypeUnknown) {}

TestWebFrameClient::~TestWebFrameClient() = default;

void TestWebFrameClient::Bind(WebLocalFrame* frame,
                              std::unique_ptr<TestWebFrameClient> self_owned) {
  DCHECK(!frame_);
  DCHECK(!self_owned || self_owned.get() == this);
  frame_ = To<WebLocalFrameImpl>(frame);
  self_owned_ = std::move(self_owned);
}

void TestWebFrameClient::BindWidgetClient(
    std::unique_ptr<WebWidgetClient> client) {
  DCHECK(!owned_widget_client_);
  owned_widget_client_ = std::move(client);
}

void TestWebFrameClient::FrameDetached(DetachType type) {
  if (frame_->FrameWidget())
    frame_->FrameWidget()->Close();

  owned_widget_client_.reset();
  frame_->Close();
  self_owned_.reset();
}

WebLocalFrame* TestWebFrameClient::CreateChildFrame(
    WebLocalFrame* parent,
    WebTreeScopeType scope,
    const WebString& name,
    const WebString& fallback_name,
    const FramePolicy&,
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

void TestWebFrameClient::BeginNavigation(
    std::unique_ptr<WebNavigationInfo> info) {
  navigation_callback_.Cancel();
  if (DocumentLoader::WillLoadUrlAsEmpty(info->url_request.Url()) &&
      !frame_->HasCommittedFirstRealLoad()) {
    CommitNavigation(std::move(info));
    return;
  }

  if (!frame_->WillStartNavigation(
          *info, false /* is_history_navigation_in_new_child_frame */))
    return;

  navigation_callback_.Reset(
      base::BindOnce(&TestWebFrameClient::CommitNavigation,
                     weak_factory_.GetWeakPtr(), std::move(info)));
  frame_->GetTaskRunner(blink::TaskType::kInternalLoading)
      ->PostTask(FROM_HERE, navigation_callback_.callback());
}

void TestWebFrameClient::CommitNavigation(
    std::unique_ptr<WebNavigationInfo> info) {
  if (!frame_)
    return;
  auto params = WebNavigationParams::CreateFromInfo(*info);
  if (info->archive_status != WebNavigationInfo::ArchiveStatus::Present)
    FillNavigationParamsResponse(params.get());
  frame_->CommitNavigation(
      std::move(params), nullptr /* extra_data */,
      base::DoNothing::Once() /* call_before_attaching_new_document */);
}

WebEffectiveConnectionType TestWebFrameClient::GetEffectiveConnectionType() {
  return effective_connection_type_;
}

void TestWebFrameClient::SetEffectiveConnectionTypeForTesting(
    WebEffectiveConnectionType effective_connection_type) {
  effective_connection_type_ = effective_connection_type;
}

void TestWebFrameClient::DidAddMessageToConsole(
    const WebConsoleMessage& message,
    const WebString& source_name,
    unsigned source_line,
    const WebString& stack_trace) {
  console_messages_.push_back(message.text);
}

WebPlugin* TestWebFrameClient::CreatePlugin(const WebPluginParams& params) {
  return new FakeWebPlugin(params);
}

AssociatedInterfaceProvider*
TestWebFrameClient::GetRemoteNavigationAssociatedInterfaces() {
  return associated_interface_provider_.get();
}

TestWebRemoteFrameClient::TestWebRemoteFrameClient()
    : associated_interface_provider_(new AssociatedInterfaceProvider(nullptr)) {
}

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
  settings.use_layer_lists = true;

  layer_tree_view_ = std::make_unique<content::LayerTreeView>(
      specified_delegate ? specified_delegate : &delegate_,
      Thread::Current()->GetTaskRunner(),
      /*compositor_thread=*/nullptr, &test_task_graph_runner_,
      &fake_thread_scheduler_);
  layer_tree_view_->Initialize(settings,
                               std::make_unique<cc::TestUkmRecorderFactory>());
  return layer_tree_view_.get();
}

TestWebWidgetClient::TestWebWidgetClient(
    content::LayerTreeViewDelegate* delegate) {
  layer_tree_view_ = layer_tree_view_factory_.Initialize(delegate);
  animation_host_ = layer_tree_view_->animation_host();
}

void TestWebWidgetClient::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  layer_tree_host()->SetRootLayer(std::move(layer));
}

void TestWebWidgetClient::SetBackgroundColor(SkColor color) {
  layer_tree_host()->set_background_color(color);
}

void TestWebWidgetClient::SetPageScaleStateAndLimits(
    float page_scale_factor,
    bool is_pinch_gesture_active,
    float minimum,
    float maximum) {
  layer_tree_host()->SetPageScaleFactorAndLimits(page_scale_factor, minimum,
                                                 maximum);
}

void TestWebWidgetClient::InjectGestureScrollEvent(
    WebGestureDevice device,
    const WebFloatSize& delta,
    ScrollGranularity granularity,
    cc::ElementId scrollable_area_element_id,
    WebInputEvent::Type injected_type) {
  InjectedScrollGestureData data{delta, granularity, scrollable_area_element_id,
                                 injected_type};
  injected_scroll_gesture_data_.push_back(data);
}

void TestWebWidgetClient::SetHaveScrollEventHandlers(bool have_handlers) {
  have_scroll_event_handlers_ = have_handlers;
}

void TestWebWidgetClient::SetEventListenerProperties(
    cc::EventListenerClass event_class,
    cc::EventListenerProperties properties) {
  layer_tree_host()->SetEventListenerProperties(event_class, properties);
}

cc::EventListenerProperties TestWebWidgetClient::EventListenerProperties(
    cc::EventListenerClass event_class) const {
  return layer_tree_host()->event_listener_properties(event_class);
}

std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
TestWebWidgetClient::DeferMainFrameUpdate() {
  return layer_tree_host()->DeferMainFrameUpdate();
}

void TestWebWidgetClient::StartDeferringCommits(base::TimeDelta timeout) {
  layer_tree_host()->StartDeferringCommits(timeout);
}

void TestWebWidgetClient::StopDeferringCommits(
    cc::PaintHoldingCommitTrigger trigger) {
  layer_tree_host()->StopDeferringCommits(trigger);
}

void TestWebWidgetClient::RegisterSelection(
    const cc::LayerSelection& selection) {
  layer_tree_host()->RegisterSelection(selection);
}

void TestWebWidgetClient::DidMeaningfulLayout(
    WebMeaningfulLayout meaningful_layout) {
  switch (meaningful_layout) {
    case WebMeaningfulLayout::kVisuallyNonEmpty:
      visually_non_empty_layout_count_++;
      break;
    case WebMeaningfulLayout::kFinishedParsing:
      finished_parsing_layout_count_++;
      break;
    case WebMeaningfulLayout::kFinishedLoading:
      finished_loading_layout_count_++;
      break;
  }
}

void TestWebWidgetClient::SetBrowserControlsShownRatio(float top_ratio,
                                                       float bottom_ratio) {
  layer_tree_host()->SetBrowserControlsShownRatio(top_ratio, bottom_ratio);
}

void TestWebWidgetClient::SetBrowserControlsHeight(float top_height,
                                                   float bottom_height,
                                                   bool shrink_viewport) {
  layer_tree_host()->SetBrowserControlsHeight(top_height, bottom_height,
                                              shrink_viewport);
}

viz::FrameSinkId TestWebWidgetClient::GetFrameSinkId() {
  return viz::FrameSinkId();
}

void TestWebViewClient::DestroyChildViews() {
  child_web_views_.clear();
}

WebView* TestWebViewClient::CreateView(WebLocalFrame* opener,
                                       const WebURLRequest&,
                                       const WebWindowFeatures&,
                                       const WebString& name,
                                       WebNavigationPolicy,
                                       WebSandboxFlags,
                                       const FeaturePolicy::FeatureState&,
                                       const SessionStorageNamespaceId&) {
  auto webview_helper = std::make_unique<WebViewHelper>();
  WebView* result = webview_helper->InitializeWithOpener(opener);
  child_web_views_.push_back(std::move(webview_helper));
  return result;
}

}  // namespace frame_test_helpers
}  // namespace blink
