// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_frame_test_proxy.h"

#include "content/public/renderer/render_frame_observer.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"

namespace test_runner {

namespace {

void PrintFrameUserGestureStatus(WebTestDelegate* delegate,
                                 blink::WebLocalFrame* frame,
                                 const char* msg) {
  bool is_user_gesture =
      blink::WebUserGestureIndicator::IsProcessingUserGesture(frame);
  delegate->PrintMessage(std::string("Frame with user gesture \"") +
                         (is_user_gesture ? "true" : "false") + "\"" + msg);
}

class TestRenderFrameObserver : public content::RenderFrameObserver {
 public:
  TestRenderFrameObserver(content::RenderFrame* frame, WebViewTestProxy* proxy)
      : RenderFrameObserver(frame), web_view_test_proxy_(proxy) {}

  ~TestRenderFrameObserver() override {}

 private:
  TestRunner* test_runner() {
    return web_view_test_proxy_->test_interfaces()->GetTestRunner();
  }

  WebTestDelegate* delegate() { return web_view_test_proxy_->delegate(); }

  // content::RenderFrameObserver overrides.
  void OnDestruct() override { delete this; }

  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - DidStartNavigation\n");
    }

    if (test_runner()->ShouldDumpUserGestureInFrameLoadCallbacks()) {
      PrintFrameUserGestureStatus(delegate(), render_frame()->GetWebFrame(),
                                  " - in DidStartNavigation\n");
    }
  }

  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - ReadyToCommitNavigation\n");
    }
  }

  void DidFailProvisionalLoad() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - didFailProvisionalLoadWithError\n");
    }
  }

  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - didCommitLoadForFrame\n");
    }
  }

  void DidFinishDocumentLoad() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - didFinishDocumentLoadForFrame\n");
    }
  }

  void DidFinishLoad() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - didFinishLoadForFrame\n");
    }
  }

  void DidHandleOnloadEvents() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      WebFrameTestClient::PrintFrameDescription(delegate(),
                                                render_frame()->GetWebFrame());
      delegate()->PrintMessage(" - didHandleOnloadEventsForFrame\n");
    }
  }

  WebViewTestProxy* web_view_test_proxy_;
  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameObserver);
};

}  // namespace

WebFrameTestProxy::~WebFrameTestProxy() = default;

void WebFrameTestProxy::Initialize(
    WebTestInterfaces* interfaces,
    content::RenderViewImpl* render_view_for_frame) {
  // The RenderViewImpl will also be a test proxy type.
  auto* view_proxy_for_frame =
      static_cast<WebViewTestProxy*>(render_view_for_frame);

  test_client_ =
      interfaces->CreateWebFrameTestClient(view_proxy_for_frame, this);
  new TestRenderFrameObserver(this, view_proxy_for_frame);  // deletes itself.
}

void WebFrameTestProxy::UpdateAllLifecyclePhasesAndCompositeForTesting() {
  if (!IsLocalRoot())
    return;
  auto* widget = static_cast<WebWidgetTestProxy*>(GetLocalRootRenderWidget());
  widget->SynchronouslyComposite(/*do_raster=*/true);
}

// WebLocalFrameClient implementation.
blink::WebPlugin* WebFrameTestProxy::CreatePlugin(
    const blink::WebPluginParams& params) {
  blink::WebPlugin* plugin = test_client_->CreatePlugin(params);
  if (plugin)
    return plugin;
  return RenderFrameImpl::CreatePlugin(params);
}

void WebFrameTestProxy::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  test_client_->DidAddMessageToConsole(message, source_name, source_line,
                                       stack_trace);
  RenderFrameImpl::DidAddMessageToConsole(message, source_name, source_line,
                                          stack_trace);
}

void WebFrameTestProxy::DownloadURL(
    const blink::WebURLRequest& request,
    network::mojom::RedirectMode cross_origin_redirect_behavior,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  test_client_->DownloadURL(request, cross_origin_redirect_behavior,
                            mojo::ScopedMessagePipeHandle());
  RenderFrameImpl::DownloadURL(request, cross_origin_redirect_behavior,
                               std::move(blob_url_token));
}

void WebFrameTestProxy::DidReceiveTitle(const blink::WebString& title,
                                        blink::WebTextDirection direction) {
  test_client_->DidReceiveTitle(title, direction);
  RenderFrameImpl::DidReceiveTitle(title, direction);
}

void WebFrameTestProxy::DidChangeIcon(blink::WebIconURL::Type icon_type) {
  test_client_->DidChangeIcon(icon_type);
  RenderFrameImpl::DidChangeIcon(icon_type);
}

void WebFrameTestProxy::DidFailLoad(const blink::WebURLError& error,
                                    blink::WebHistoryCommitType commit_type) {
  test_client_->DidFailLoad(error, commit_type);
  RenderFrameImpl::DidFailLoad(error, commit_type);
}

void WebFrameTestProxy::DidStartLoading() {
  test_client_->DidStartLoading();
  RenderFrameImpl::DidStartLoading();
}

void WebFrameTestProxy::DidStopLoading() {
  RenderFrameImpl::DidStopLoading();
  test_client_->DidStopLoading();
}

void WebFrameTestProxy::DidChangeSelection(bool is_selection_empty) {
  test_client_->DidChangeSelection(is_selection_empty);
  RenderFrameImpl::DidChangeSelection(is_selection_empty);
}

void WebFrameTestProxy::DidChangeContents() {
  test_client_->DidChangeContents();
  RenderFrameImpl::DidChangeContents();
}

blink::WebEffectiveConnectionType
WebFrameTestProxy::GetEffectiveConnectionType() {
  if (test_client_->GetEffectiveConnectionType() !=
      blink::WebEffectiveConnectionType::kTypeUnknown) {
    return test_client_->GetEffectiveConnectionType();
  }
  return RenderFrameImpl::GetEffectiveConnectionType();
}

void WebFrameTestProxy::RunModalAlertDialog(const blink::WebString& message) {
  test_client_->RunModalAlertDialog(message);
}

bool WebFrameTestProxy::RunModalConfirmDialog(const blink::WebString& message) {
  return test_client_->RunModalConfirmDialog(message);
}

bool WebFrameTestProxy::RunModalPromptDialog(
    const blink::WebString& message,
    const blink::WebString& default_value,
    blink::WebString* actual_value) {
  return test_client_->RunModalPromptDialog(message, default_value,
                                            actual_value);
}

bool WebFrameTestProxy::RunModalBeforeUnloadDialog(bool is_reload) {
  return test_client_->RunModalBeforeUnloadDialog(is_reload);
}

void WebFrameTestProxy::ShowContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  test_client_->ShowContextMenu(context_menu_data);
  RenderFrameImpl::ShowContextMenu(context_menu_data);
}

void WebFrameTestProxy::DidDispatchPingLoader(const blink::WebURL& url) {
  // This is not implemented in RenderFrameImpl, so need to explicitly call
  // into the base proxy.
  test_client_->DidDispatchPingLoader(url);
  RenderFrameImpl::DidDispatchPingLoader(url);
}

void WebFrameTestProxy::WillSendRequest(blink::WebURLRequest& request) {
  RenderFrameImpl::WillSendRequest(request);
  test_client_->WillSendRequest(request);
}

void WebFrameTestProxy::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  if (test_client_->ShouldContinueNavigation(info.get()))
    RenderFrameImpl::BeginNavigation(std::move(info));
}

void WebFrameTestProxy::PostAccessibilityEvent(
    const blink::WebAXObject& object,
    ax::mojom::Event event,
    ax::mojom::EventFrom event_from) {
  test_client_->PostAccessibilityEvent(object, event, event_from);
  // Guard against the case where |this| was deleted as a result of an
  // accessibility listener detaching a frame. If that occurs, the
  // WebAXObject will be detached.
  if (object.IsDetached())
    return;  // |this| is invalid.
  RenderFrameImpl::PostAccessibilityEvent(object, event, event_from);
}

void WebFrameTestProxy::MarkWebAXObjectDirty(const blink::WebAXObject& object,
                                             bool subtree) {
  test_client_->MarkWebAXObjectDirty(object, subtree);
  // Guard against the case where |this| was deleted as a result of an
  // accessibility listener detaching a frame. If that occurs, the
  // WebAXObject will be detached.
  if (object.IsDetached())
    return;  // |this| is invalid.
  RenderFrameImpl::MarkWebAXObjectDirty(object, subtree);
}

void WebFrameTestProxy::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  test_client_->CheckIfAudioSinkExistsAndIsAuthorized(
      sink_id, std::move(completion_callback));
}

void WebFrameTestProxy::DidClearWindowObject() {
  test_client_->DidClearWindowObject();
  RenderFrameImpl::DidClearWindowObject();
}

}  // namespace test_runner
