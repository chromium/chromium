// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_policy_container_host.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/policy_container_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/data_url.h"
#include "services/network/public/cpp/not_implemented_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/test/test_web_frame_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

// static
mojo::PendingAssociatedReceiver<mojom::Frame>
TestRenderFrame::CreateStubFrameReceiver() {
  mojo::PendingAssociatedRemote<mojom::Frame> pending_remote;
  mojo::PendingAssociatedReceiver<mojom::Frame> pending_receiver =
      pending_remote.InitWithNewEndpointAndPassReceiver();
  return pending_receiver;
}

// static
mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
TestRenderFrame::CreateStubBrowserInterfaceBrokerRemote() {
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> pending_remote;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  return pending_remote;
}

// static
mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
TestRenderFrame::CreateStubAssociatedInterfaceProviderRemote() {
  mojo::AssociatedRemote<blink::mojom::AssociatedInterfaceProvider> remote;
  mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
      pending_receiver = remote.BindNewEndpointAndPassDedicatedReceiver();
  return remote.Unbind();
}

class MockFrameHost : public mojom::FrameHost {
 public:
  MockFrameHost() {}

  MockFrameHost(const MockFrameHost&) = delete;
  MockFrameHost& operator=(const MockFrameHost&) = delete;

  ~MockFrameHost() override = default;

  mojom::DidCommitProvisionalLoadParamsPtr TakeLastCommitParams() {
    return std::move(last_commit_params_);
  }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  TakeLastBrowserInterfaceBrokerReceiver() {
    return std::move(last_browser_interface_broker_receiver_);
  }

  // The frame in the renderer sends a BrowserInterfaceBroker Receiver to the
  // browser process. The test harness (in MockRenderThread) will stash away
  // those pending Receivers. This sets the pending Receiver that was sent for
  // the browser process to bind when initially creating the frame.
  void SetInitialBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver) {
    last_browser_interface_broker_receiver_ =
        std::move(browser_interface_broker_receiver);
  }

  void DidCommitProvisionalLoad(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params)
      override {
    last_commit_params_ = std::move(params);
    if (interface_params) {
      last_browser_interface_broker_receiver_ =
          std::move(interface_params->browser_interface_broker_receiver);
    }
  }

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
    NOTREACHED_IN_MIGRATION()
        << "We should never dispatch to the service side signature.";
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

  void CreateChildFrame(
      const blink::LocalFrameToken& frame_token,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          associated_interface_provider_receiver,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const blink::FramePolicy& frame_policy,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
      blink::FrameOwnerElementType owner_type,
      ukm::SourceId document_ukm_source_id) override {
    // If we get here, then the remote end of
    // `associated_interface_provider_receiver` is expected to be usable even
    // though we don't pass this receiver over an existing mojo pipe. Simulate
    // doing so by allowing unassociated usage, so that the remote can be used
    // without blowing up.
    associated_interface_provider_receiver.EnableUnassociatedUsage();
    MockPolicyContainerHost mock_policy_container_host;
    mock_policy_container_host.BindWithNewEndpoint(
        std::move(policy_container_bind_params->receiver));
    MockRenderThread* mock_render_thread =
        static_cast<MockRenderThread*>(RenderThread::Get());
    mock_render_thread->OnCreateChildFrame(
        frame_token, std::move(frame_remote),
        std::move(browser_interface_broker_receiver));
  }

  void DidCommitSameDocumentNavigation(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_doc_params)
      override {
    last_commit_params_ = std::move(params);
  }

  void DidOpenDocumentInputStream(const GURL& url) override {}

  void BeginNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>,
      mojo::PendingRemote<blink::mojom::NavigationStateKeepAliveHandle>,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>)
      override {}

  void SubresourceResponseStarted(const url::SchemeHostPort& final_response_url,
                                  net::CertStatus cert_status) override {}

  void ResourceLoadComplete(
      blink::mojom::ResourceLoadInfoPtr resource_load_info) override {}

  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override {}

  void CancelInitialHistoryLoad() override {}

  void UpdateEncoding(const std::string& encoding_name) override {}

  void UpdateState(const blink::PageState& state) override {
    is_page_state_updated_ = true;
  }

  void OpenURL(blink::mojom::OpenURLParamsPtr params) override {
    is_url_opened_ = true;
  }

  void DidStopLoading() override {}

#if BUILDFLAG(IS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override {}
#endif

 private:
  mojom::DidCommitProvisionalLoadParamsPtr last_commit_params_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      last_browser_interface_broker_receiver_;

  size_t request_overlay_routing_token_called_ = 0;
  std::optional<base::UnguessableToken> overlay_routing_token_;

  bool is_page_state_updated_ = false;

  bool is_url_opened_ = false;
};

// static
RenderFrameImpl* TestRenderFrame::CreateTestRenderFrame(
    RenderFrameImpl::CreateParams params) {
  return new TestRenderFrame(std::move(params));
}

TestRenderFrame::TestRenderFrame(RenderFrameImpl::CreateParams params)
    : RenderFrameImpl(std::move(params)),
      mock_frame_host_(std::make_unique<MockFrameHost>()) {
}

TestRenderFrame::~TestRenderFrame() {}

void TestRenderFrame::SetHTMLOverrideForNextNavigation(
    const std::string& html) {
  next_navigation_html_override_ = html;
}

void TestRenderFrame::Navigate(
    network::mojom::URLResponseHeadPtr head,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params) {
  mock_navigation_client_.reset();
  BindNavigationClient(
      mock_navigation_client_.BindNewEndpointAndPassDedicatedReceiver());
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factory_bundle =
      blink::ChildPendingURLLoaderFactoryBundle::CreateFromDefaultFactoryImpl(
          network::NotImplementedURLLoaderFactory::Create());

  MockPolicyContainerHost mock_policy_container_host;
  CommitNavigation(
      std::move(common_params), std::move(commit_params), std::move(head),
      mojo::ScopedDataPipeConsumerHandle(),
      network::mojom::URLLoaderClientEndpointsPtr(),
      std::move(pending_factory_bundle),
      /*subresource_overrides=*/std::nullopt,
      blink::mojom::ControllerServiceWorkerInfoPtr(),
      blink::mojom::ServiceWorkerContainerInfoForClientPtr(),
      /*subresource_proxying_loader_factory=*/mojo::NullRemote(),
      /*keep_alive_loader_factory=*/mojo::NullRemote(),
      /*fetch_later_loader_factory=*/mojo::NullAssociatedRemote(),
      /*document_token=*/blink::DocumentToken(),
      /*devtools_navigation_token=*/base::UnguessableToken::Create(),
      /*base_auction_nonce=*/base::Uuid::GenerateRandomV4(),
      blink::ParsedPermissionsPolicy(),
      blink::mojom::PolicyContainer::New(
          blink::mojom::PolicyContainerPolicies::New(),
          mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote()),
      /*code_cache_host=*/mojo::NullRemote(),
      /*code_cache_host_for_background=*/mojo::NullRemote(),
      /*cookie_manager_info=*/nullptr,
      /*storage_info=*/nullptr,
      base::BindOnce(&MockFrameHost::DidCommitProvisionalLoad,
                     base::Unretained(mock_frame_host_.get())));
}

void TestRenderFrame::Navigate(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params) {
  Navigate(network::mojom::URLResponseHead::New(), std::move(common_params),
           std::move(commit_params));
}

void TestRenderFrame::NavigateWithError(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    int error_code,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<std::string>& error_page_content) {
  mock_navigation_client_.reset();
  BindNavigationClient(
      mock_navigation_client_.BindNewEndpointAndPassDedicatedReceiver());
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factory_bundle =
      blink::ChildPendingURLLoaderFactoryBundle::CreateFromDefaultFactoryImpl(
          network::NotImplementedURLLoaderFactory::Create());
  mock_navigation_client_->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      /*has_stale_copy_in_cache=*/false, error_code,
      /*extended_error_code=*/0, resolve_error_info, error_page_content,
      std::move(pending_factory_bundle), blink::DocumentToken(),
      CreateStubPolicyContainer(),
      /*alternative_error_page_info=*/nullptr,
      base::BindOnce(&MockFrameHost::DidCommitProvisionalLoad,
                     base::Unretained(mock_frame_host_.get())));
}

void TestRenderFrame::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  if (next_navigation_html_override_.has_value()) {
    AssertNavigationCommits assert_navigation_commits(
        this, kMayReplaceInitialEmptyDocument);
    auto navigation_params =
        blink::WebNavigationParams::CreateWithHTMLStringForTesting(
            next_navigation_html_override_.value(), info->url_request.Url());
    MockPolicyContainerHost mock_policy_container_host;
    navigation_params->requestor_origin = info->url_request.RequestorOrigin();
    navigation_params->policy_container =
        std::make_unique<blink::WebPolicyContainer>(
            blink::WebPolicyContainerPolicies(),
            mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
    next_navigation_html_override_ = std::nullopt;
    DCHECK(!static_cast<GURL>(info->url_request.Url()).IsAboutSrcdoc());
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
    MockPolicyContainerHost mock_policy_container_host;
    navigation_params->policy_container =
        std::make_unique<blink::WebPolicyContainer>(
            blink::WebPolicyContainerPolicies(),
            mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
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
    if (url.IsAboutSrcdoc()) {
      navigation_params->fallback_base_url = info->requestor_base_url;
    }

    navigation_params->policy_container->policies.sandbox_flags =
        navigation_params->frame_policy->sandbox_flags;

    if (url.IsAboutSrcdoc()) {
      blink::TestWebFrameHelper::FillStaticResponseForSrcdocNavigation(
          GetWebFrame(), navigation_params.get());
    }

    frame_->CommitNavigation(std::move(navigation_params),
                             nullptr /* extra_data */);
    return;
  }
  RenderFrameImpl::BeginNavigation(std::move(info));
}

mojom::DidCommitProvisionalLoadParamsPtr
TestRenderFrame::TakeLastCommitParams() {
  return mock_frame_host_->TakeLastCommitParams();
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

void TestRenderFrame::BindToFrame(blink::WebNavigationControl* frame) {
  RenderFrameImpl::BindToFrame(frame);
  MockRenderThread* mock_render_thread =
      static_cast<MockRenderThread*>(RenderThread::Get());
  mock_frame_host_->SetInitialBrowserInterfaceBrokerReceiver(
      mock_render_thread->TakeInitialBrowserInterfaceBrokerReceiverForFrame(
          frame->GetLocalFrameToken()));
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
