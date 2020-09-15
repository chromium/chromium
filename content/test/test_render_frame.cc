// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/data_url.h"
#include "services/network/public/cpp/not_implemented_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

class MockFrameHost : public mojom::FrameHost {
 public:
  MockFrameHost() {}
  ~MockFrameHost() override = default;

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  TakeLastCommitParams() {
    return std::move(last_commit_params_);
  }

  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
  TakeLastInterfaceProviderReceiver() {
    return std::move(last_interface_provider_receiver_);
  }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  TakeLastBrowserInterfaceBrokerReceiver() {
    return std::move(last_browser_interface_broker_receiver_);
  }

  void SetDidAddMessageToConsoleCallback(
      base::OnceCallback<void(const base::string16& msg)> callback) {
    did_add_message_to_console_callback_ = std::move(callback);
  }

  // Holds on to the receiver end of the InterfaceProvider interface whose
  // client end is bound to the corresponding RenderFrame's |remote_interfaces_|
  // to facilitate retrieving the most recent |interface_provider_receiver| in
  // tests.
  void PassLastInterfaceProviderReceiver(
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver) {
    last_interface_provider_receiver_ = std::move(interface_provider_receiver);
  }

  // Holds on to the receiver end of the BrowserInterfaceBroker interface whose
  // client end is bound to the corresponding RenderFrame's
  // |browser_interface_broker_proxy_| to facilitate retrieving the most recent
  // |browser_interface_broker_receiver| in tests.
  void PassLastBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver) {
    last_browser_interface_broker_receiver_ =
        std::move(browser_interface_broker_receiver);
  }

  void DidCommitProvisionalLoad(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params)
      override {
    last_commit_params_ = std::move(params);
    if (interface_params) {
      last_interface_provider_receiver_ =
          std::move(interface_params->interface_provider_receiver);
      last_browser_interface_broker_receiver_ =
          std::move(interface_params->browser_interface_broker_receiver);
    }
  }

  void ShowCreatedWindow(int32_t pending_widget_routing_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override {}

  void set_overlay_routing_token(const base::UnguessableToken& token) {
    overlay_routing_token_ = token;
  }

  size_t request_overlay_routing_token_called() {
    return request_overlay_routing_token_called_;
  }

  bool is_page_state_updated() const { return is_page_state_updated_; }

  bool is_url_opened() const { return is_url_opened_; }

 protected:
  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr,
                       CreateNewWindowCallback) override {
    NOTREACHED() << "We should never dispatch to the service side signature.";
  }

  bool CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       mojom::CreateNewWindowStatus* status,
                       mojom::CreateNewWindowReplyPtr* reply) override {
    *status = mojom::CreateNewWindowStatus::kSuccess;
    *reply = mojom::CreateNewWindowReply::New();
    MockRenderThread* mock_render_thread =
        static_cast<MockRenderThread*>(RenderThread::Get());
    mock_render_thread->OnCreateWindow(*params, reply->get());
    return true;
  }

  bool CreateNewWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
      int32_t* out_routing_id) override {
    MockRenderThread* mock_render_thread =
        static_cast<MockRenderThread*>(RenderThread::Get());
    *out_routing_id = mock_render_thread->GetNextRoutingID();
    return true;
  }

  void CreateNewWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
      CreateNewWidgetCallback callback) override {
    std::move(callback).Run(MSG_ROUTING_NONE);
  }

  void CreateNewFullscreenWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
      CreateNewFullscreenWidgetCallback callback) override {
    std::move(callback).Run(MSG_ROUTING_NONE);
  }

  void CreatePortal(mojo::PendingAssociatedReceiver<blink::mojom::Portal>,
                    mojo::PendingAssociatedRemote<blink::mojom::PortalClient>,
                    CreatePortalCallback callback) override {
    std::move(callback).Run(MSG_ROUTING_NONE, FrameReplicationState(),
                            blink::PortalToken(), base::UnguessableToken(),
                            base::UnguessableToken());
  }

  void AdoptPortal(const blink::PortalToken&,
                   AdoptPortalCallback callback) override {
    std::move(callback).Run(MSG_ROUTING_NONE, viz::FrameSinkId(),
                            FrameReplicationState(), base::UnguessableToken(),
                            base::UnguessableToken());
  }

  void IssueKeepAliveHandle(
      mojo::PendingReceiver<mojom::KeepAliveHandle> receiver) override {}

  void DidCommitSameDocumentNavigation(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params)
      override {
    last_commit_params_ = std::move(params);
  }

  void BeginNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>) override {}

  void SubresourceResponseStarted(const GURL& url,
                                  net::CertStatus cert_status) override {}

  void ResourceLoadComplete(
      blink::mojom::ResourceLoadInfoPtr resource_load_info) override {}

  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override {}

  void DidSetFramePolicyHeaders(
      network::mojom::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& feature_policy_header,
      const blink::DocumentPolicyFeatureState& document_policy_header)
      override {}

  void CancelInitialHistoryLoad() override {}

  void UpdateEncoding(const std::string& encoding_name) override {}

  void FrameSizeChanged(const gfx::Size& frame_size) override {}

  void UpdateState(const PageState& state) override {
    is_page_state_updated_ = true;
  }

  void OpenURL(mojom::OpenURLParamsPtr params) override {
    is_url_opened_ = true;
  }

  void DidStopLoading() override {}

  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& msg,
                              int32_t line_number,
                              const base::string16& source_id) override {
    if (did_add_message_to_console_callback_) {
      std::move(did_add_message_to_console_callback_).Run(msg);
    }
  }

#if defined(OS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override {}
#endif

 private:
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
      last_commit_params_;
  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
      last_interface_provider_receiver_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      last_browser_interface_broker_receiver_;

  base::OnceCallback<void(const base::string16& msg)>
      did_add_message_to_console_callback_;

  size_t request_overlay_routing_token_called_ = 0;
  base::Optional<base::UnguessableToken> overlay_routing_token_;

  bool is_page_state_updated_ = false;

  bool is_url_opened_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockFrameHost);
};

// static
RenderFrameImpl* TestRenderFrame::CreateTestRenderFrame(
    RenderFrameImpl::CreateParams params) {
  return new TestRenderFrame(std::move(params));
}

TestRenderFrame::TestRenderFrame(RenderFrameImpl::CreateParams params)
    : RenderFrameImpl(std::move(params)),
      mock_frame_host_(std::make_unique<MockFrameHost>()) {
  MockRenderThread* mock_render_thread =
      static_cast<MockRenderThread*>(RenderThread::Get());
  mock_frame_host_->PassLastInterfaceProviderReceiver(
      mock_render_thread->TakeInitialInterfaceProviderRequestForFrame(
          params.routing_id));
  mock_frame_host_->PassLastBrowserInterfaceBrokerReceiver(
      mock_render_thread->TakeInitialBrowserInterfaceBrokerReceiverForFrame(
          params.routing_id));
}

TestRenderFrame::~TestRenderFrame() {}

void TestRenderFrame::SetHTMLOverrideForNextNavigation(
    const std::string& html) {
  next_navigation_html_override_ = html;
}

void TestRenderFrame::Navigate(network::mojom::URLResponseHeadPtr head,
                               mojom::CommonNavigationParamsPtr common_params,
                               mojom::CommitNavigationParamsPtr commit_params) {
  mock_navigation_client_.reset();
  BindNavigationClient(
      mock_navigation_client_.BindNewEndpointAndPassDedicatedReceiver());
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factory_bundle =
      ChildPendingURLLoaderFactoryBundle::CreateFromDefaultFactoryImpl(
          std::make_unique<network::NotImplementedURLLoaderFactory>());
  CommitNavigation(std::move(common_params), std::move(commit_params),
                   std::move(head), mojo::ScopedDataPipeConsumerHandle(),
                   network::mojom::URLLoaderClientEndpointsPtr(),
                   std::move(pending_factory_bundle), base::nullopt,
                   blink::mojom::ControllerServiceWorkerInfoPtr(),
                   blink::mojom::ServiceWorkerContainerInfoForClientPtr(),
                   mojo::NullRemote() /* prefetch_loader_factory */,
                   base::UnguessableToken::Create(),
                   base::BindOnce(&MockFrameHost::DidCommitProvisionalLoad,
                                  base::Unretained(mock_frame_host_.get())));
}

void TestRenderFrame::Navigate(mojom::CommonNavigationParamsPtr common_params,
                               mojom::CommitNavigationParamsPtr commit_params) {
  Navigate(network::mojom::URLResponseHead::New(), std::move(common_params),
           std::move(commit_params));
}

void TestRenderFrame::NavigateWithError(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    int error_code,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<std::string>& error_page_content) {
  mock_navigation_client_.reset();
  BindNavigationClient(
      mock_navigation_client_.BindNewEndpointAndPassDedicatedReceiver());
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factory_bundle =
      ChildPendingURLLoaderFactoryBundle::CreateFromDefaultFactoryImpl(
          std::make_unique<network::NotImplementedURLLoaderFactory>());
  mock_navigation_client_->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      false /* has_stale_copy_in_cache */, error_code, resolve_error_info,
      error_page_content, std::move(pending_factory_bundle),
      base::BindOnce(&MockFrameHost::DidCommitProvisionalLoad,
                     base::Unretained(mock_frame_host_.get())));
}

void TestRenderFrame::Unload(
    int proxy_routing_id,
    bool is_loading,
    const FrameReplicationState& replicated_frame_state,
    const base::UnguessableToken& frame_token) {
  OnUnload(proxy_routing_id, is_loading, replicated_frame_state, frame_token);
}

void TestRenderFrame::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  if (next_navigation_html_override_.has_value()) {
    AssertNavigationCommits assert_navigation_commits(
        this, kMayReplaceInitialEmptyDocument);
    auto navigation_params = blink::WebNavigationParams::CreateWithHTMLString(
        next_navigation_html_override_.value(), info->url_request.Url());
    next_navigation_html_override_ = base::nullopt;
    frame_->CommitNavigation(std::move(navigation_params),
                             nullptr /* extra_data */);
    return;
  }
  if (info->navigation_policy == blink::kWebNavigationPolicyCurrentTab &&
      GetWebFrame()->Parent() && info->form.IsNull()) {
    AssertNavigationCommits assert_navigation_commits(
        this, kMayReplaceInitialEmptyDocument);
    // RenderViewTest::LoadHTML immediately commits navigation for the main
    // frame. However if the loaded html has an empty or data subframe,
    // BeginNavigation will be called from Blink and we should avoid
    // going through browser process in this case.
    GURL url = info->url_request.Url();
    auto navigation_params =
        blink::WebNavigationParams::CreateFromInfo(*info.get());
    if (!url.IsAboutBlank() && !url.IsAboutSrcdoc()) {
      std::string mime_type, charset, data;
      if (!net::DataURL::Parse(url, &mime_type, &charset, &data)) {
        // This case is only here to allow cluster fuzz pass any url,
        // to unblock further fuzzing.
        mime_type = "text/html";
        charset = "UTF-8";
      }
      blink::WebNavigationParams::FillStaticResponse(
          navigation_params.get(), blink::WebString::FromUTF8(mime_type),
          blink::WebString::FromUTF8(charset), data);
    }
    frame_->CommitNavigation(std::move(navigation_params),
                             nullptr /* extra_data */);
    return;
  }
  RenderFrameImpl::BeginNavigation(std::move(info));
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
TestRenderFrame::TakeLastCommitParams() {
  return mock_frame_host_->TakeLastCommitParams();
}

void TestRenderFrame::SetDidAddMessageToConsoleCallback(
    base::OnceCallback<void(const base::string16& msg)> callback) {
  mock_frame_host_->SetDidAddMessageToConsoleCallback(std::move(callback));
}

mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
TestRenderFrame::TakeLastInterfaceProviderReceiver() {
  return mock_frame_host_->TakeLastInterfaceProviderReceiver();
}

mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
TestRenderFrame::TakeLastBrowserInterfaceBrokerReceiver() {
  return mock_frame_host_->TakeLastBrowserInterfaceBrokerReceiver();
}

void TestRenderFrame::SimulateBeforeUnload(bool is_reload) {
  // This will execute the BeforeUnload event in this frame and all of its
  // local descendant frames, including children of remote frames. The browser
  // process will send separate IPCs to dispatch beforeunload in any
  // out-of-process child frames.
  frame_->DispatchBeforeUnloadEvent(is_reload);
}

bool TestRenderFrame::IsPageStateUpdated() const {
  return mock_frame_host_->is_page_state_updated();
}

bool TestRenderFrame::IsURLOpened() const {
  return mock_frame_host_->is_url_opened();
}

mojom::FrameHost* TestRenderFrame::GetFrameHost() {
  // Need to mock this interface directly without going through a binding,
  // otherwise calling its sync methods could lead to a deadlock.
  //
  // Imagine the following sequence of events take place:
  //
  //   1.) GetFrameHost() called for the first time
  //   1.1.) GetRemoteAssociatedInterfaces()->GetInterface(&frame_host_ptr_)
  //   1.1.1) ... plumbing ...
  //   1.1.2) Task posted to bind the request end to the Mock implementation
  //   1.2) The interface pointer end is returned to the caller
  //   2.) GetFrameHost()->CreateNewWindow(...) sync method invoked
  //   2.1.) Mojo sync request sent
  //   2.2.) Waiting for sync response while dispatching incoming sync requests
  //
  // Normally the sync Mojo request would be processed in 2.2. However, the
  // implementation is not yet bound at that point, and will never be, because
  // only sync IPCs are dispatched by 2.2, not posted tasks. So the sync request
  // is never dispatched, the response never arrives.
  //
  // Because the first invocation to GetFrameHost() may come while we are inside
  // a message loop already, pumping messags before 1.2 would constitute a
  // nested message loop and is therefore undesired.
  return mock_frame_host_.get();
}

}  // namespace content
