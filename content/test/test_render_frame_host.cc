// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/uuid.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.mojom.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/storage_access_api/status.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "ui/base/page_transition_types.h"

namespace content {

TestRenderFrameHostCreationObserver::TestRenderFrameHostCreationObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TestRenderFrameHostCreationObserver::~TestRenderFrameHostCreationObserver() =
    default;

void TestRenderFrameHostCreationObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  last_created_frame_ = render_frame_host;
}

void TestRenderFrameHostCreationObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (last_created_frame_ == render_frame_host) {
    last_created_frame_ = nullptr;
  }
}

TestRenderFrameHost::TestRenderFrameHost(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int32_t routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    const blink::LocalFrameToken& frame_token,
    const blink::DocumentToken& document_token,
    base::UnguessableToken devtools_frame_token,
    RenderFrameHostImpl::LifecycleStateImpl lifecycle_state,
    scoped_refptr<BrowsingContextState> browsing_context_state)
    : RenderFrameHostImpl(site_instance,
                          render_view_host,
                          delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          std::move(frame_remote),
                          frame_token,
                          document_token,
                          devtools_frame_token,
                          /*renderer_initiated_creation_of_main_frame=*/false,
                          lifecycle_state,
                          browsing_context_state,
                          frame_tree_node->frame_owner_element_type(),
                          frame_tree_node->parent(),
                          frame_tree_node->fenced_frame_status()),
      child_creation_observer_(
          WebContents::FromRenderViewHost(render_view_host.get())),
      simulate_history_list_was_cleared_(false),
      last_commit_was_error_page_(false) {}

TestRenderFrameHost::~TestRenderFrameHost() = default;

void TestRenderFrameHost::FlushLocalFrameMessages() {
  // Force creation of `local_frame_`.
  GetAssociatedLocalFrame();
  local_frame_.FlushForTesting();
}

TestRenderViewHost* TestRenderFrameHost::GetRenderViewHost() const {
  return static_cast<TestRenderViewHost*>(
      RenderFrameHostImpl::GetRenderViewHost());
}

TestPage& TestRenderFrameHost::GetPage() {
  return static_cast<TestPage&>(RenderFrameHostImpl::GetPage());
}

MockRenderProcessHost* TestRenderFrameHost::GetProcess() const {
  return static_cast<MockRenderProcessHost*>(RenderFrameHostImpl::GetProcess());
}

MockAgentSchedulingGroupHost& TestRenderFrameHost::GetAgentSchedulingGroup() {
  return static_cast<MockAgentSchedulingGroupHost&>(
      RenderFrameHostImpl::GetAgentSchedulingGroup());
}

TestRenderWidgetHost* TestRenderFrameHost::GetRenderWidgetHost() {
  return static_cast<TestRenderWidgetHost*>(
      RenderFrameHostImpl::GetRenderWidgetHost());
}

void TestRenderFrameHost::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  console_messages_.push_back(message);
  RenderFrameHostImpl::AddMessageToConsole(level, message);
}

void TestRenderFrameHost::ReportInspectorIssue(
    blink::mojom::InspectorIssueInfoPtr issue) {
  if (issue->code == blink::mojom::InspectorIssueCode::kHeavyAdIssue) {
    switch (issue->details->heavy_ad_issue_details->reason) {
      case blink::mojom::HeavyAdReason::kNetworkTotalLimit:
        heavy_ad_issue_network_count_++;
        break;
      case blink::mojom::HeavyAdReason::kCpuTotalLimit:
        heavy_ad_issue_cpu_total_count_++;
        break;
      case blink::mojom::HeavyAdReason::kCpuPeakLimit:
        heavy_ad_issue_cpu_peak_count_++;
        break;
    }
  } else if (issue->code ==
             blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue) {
    ++federated_auth_counts_[issue->details->federated_auth_request_details
                                 ->status];
  } else if (issue->code == blink::mojom::InspectorIssueCode::
                                kFederatedAuthUserInfoRequestIssue) {
    ++federated_auth_user_info_counts_
        [issue->details->federated_auth_user_info_request_details->status];
  }
  RenderFrameHostImpl::ReportInspectorIssue(std::move(issue));
}

bool TestRenderFrameHost::IsTestRenderFrameHost() const {
  return true;
}

void TestRenderFrameHost::DidFailLoadWithError(const GURL& url,
                                               int error_code) {
  RenderFrameHostImpl::DidFailLoadWithError(url, error_code);
}

void TestRenderFrameHost::InitializeRenderFrameIfNeeded() {
  if (!render_view_host()->IsRenderViewLive()) {
    render_view_host()->GetProcess()->Init();
    RenderViewHostTester::For(render_view_host())->CreateTestRenderView();
  }
}

TestRenderFrameHost* TestRenderFrameHost::AppendChild(
    const std::string& frame_name) {
  return AppendChildWithPolicy(frame_name, {});
}

TestRenderFrameHost* TestRenderFrameHost::AppendChildWithPolicy(
    const std::string& frame_name,
    const blink::ParsedPermissionsPolicy& allow) {
  std::string frame_unique_name =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  OnCreateChildFrame(
      GetProcess()->GetNextRoutingID(), CreateStubFrameRemote(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      CreateStubPolicyContainerBindParams(),
      CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, frame_name, frame_unique_name,
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(),
      blink::FramePolicy({network::mojom::WebSandboxFlags::kNone, allow, {}}),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  return static_cast<TestRenderFrameHost*>(
      child_creation_observer_.last_created_frame());
}

TestRenderFrameHost* TestRenderFrameHost::AppendCredentiallessChild(
    const std::string& frame_name) {
  TestRenderFrameHost* rfh = AppendChildWithPolicy(frame_name, {});
  auto attributes = blink::mojom::IframeAttributes::New();
  attributes->credentialless = true;
  rfh->frame_tree_node()->SetAttributes(std::move(attributes));
  return rfh;
}

void TestRenderFrameHost::Detach() {
  if (IsFencedFrameRoot()) {
    // In production code, detaching Fenced Frames is intiated in a renderer
    // process by, e.g. Web API `Element.remove()`. This is resolved as
    // `Node.removeChild()` of the parent node and triggers
    // RenderFrameProxyHost::Detach for the outer delegate node. In unit tests,
    // this method initiates detaching. So, this method mimics
    // RenderFrameProxyHost::Detach.

    ResumeDeletionForTesting();

    frame_tree_node_->render_manager()->RemoveOuterDelegateFrame();
  } else {
    DetachForTesting();
  }
}

void TestRenderFrameHost::SimulateNavigationStart(const GURL& url) {
  SendRendererInitiatedNavigationRequest(url, false);
}

void TestRenderFrameHost::SimulateRedirect(const GURL& new_url) {
  NavigationRequest* request = frame_tree_node_->navigation_request();
  if (!request->loader_for_testing()) {
    base::RunLoop loop;
    request->set_on_start_checks_complete_closure_for_testing(
        loop.QuitClosure());
    loop.Run();
  }
  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);
  url_loader->SimulateServerRedirect(new_url);
}

void TestRenderFrameHost::SimulateBeforeUnloadCompleted(bool proceed) {
  base::TimeTicks now = base::TimeTicks::Now();
  ProcessBeforeUnloadCompleted(
      proceed, /* treat_as_final_completion_callback= */ false, now, now,
      /*for_legacy=*/false);
}

void TestRenderFrameHost::SimulateUnloadACK() {
  OnUnloadACK();
}

void TestRenderFrameHost::SimulateUserActivation() {
  frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
}

const std::vector<std::string>& TestRenderFrameHost::GetConsoleMessages() {
  return console_messages_;
}

void TestRenderFrameHost::ClearConsoleMessages() {
  console_messages_.clear();
}

int TestRenderFrameHost::GetHeavyAdIssueCount(
    RenderFrameHostTester::HeavyAdIssueType type) {
  switch (type) {
    case RenderFrameHostTester::HeavyAdIssueType::kNetworkTotal:
      return heavy_ad_issue_network_count_;
    case RenderFrameHostTester::HeavyAdIssueType::kCpuTotal:
      return heavy_ad_issue_cpu_total_count_;
    case RenderFrameHostTester::HeavyAdIssueType::kCpuPeak:
      return heavy_ad_issue_cpu_peak_count_;
    case RenderFrameHostTester::HeavyAdIssueType::kAll:
      return heavy_ad_issue_network_count_ + heavy_ad_issue_cpu_total_count_ +
             heavy_ad_issue_cpu_peak_count_;
  }
}

int TestRenderFrameHost::GetFederatedAuthRequestIssueCount(
    std::optional<blink::mojom::FederatedAuthRequestResult> status_type) {
  if (!status_type) {
    int total = 0;
    for (const auto& [result, count] : federated_auth_counts_)
      total += count;
    return total;
  }

  auto it = federated_auth_counts_.find(*status_type);
  if (it == federated_auth_counts_.end())
    return 0;
  return it->second;
}

int TestRenderFrameHost::GetFederatedAuthUserInfoRequestIssueCount(
    std::optional<blink::mojom::FederatedAuthUserInfoRequestResult>
        status_type) {
  if (!status_type) {
    int total = 0;
    for (const auto& [result, count] : federated_auth_user_info_counts_) {
      total += count;
    }
    return total;
  }

  auto it = federated_auth_user_info_counts_.find(*status_type);
  if (it == federated_auth_user_info_counts_.end()) {
    return 0;
  }
  return it->second;
}

void TestRenderFrameHost::SimulateManifestURLUpdate(const GURL& manifest_url) {
  GetPage().UpdateManifestUrl(manifest_url);
}

TestRenderFrameHost* TestRenderFrameHost::AppendFencedFrame() {
  fenced_frames_.push_back(std::make_unique<FencedFrame>(
      weak_ptr_factory_.GetSafeRef(), /* was_discarded= */ false));
  FencedFrame* fenced_frame = fenced_frames_.back().get();
  // Create stub RemoteFrameInterfaces.
  auto remote_frame_interfaces =
      blink::mojom::RemoteFrameInterfacesFromRenderer::New();
  remote_frame_interfaces->frame_host_receiver =
      mojo::AssociatedRemote<blink::mojom::RemoteFrameHost>()
          .BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<blink::mojom::RemoteFrame> frame;
  std::ignore = frame.BindNewEndpointAndPassDedicatedReceiver();
  remote_frame_interfaces->frame = frame.Unbind();
  fenced_frame->InitInnerFrameTreeAndReturnProxyToOuterFrameTree(
      std::move(remote_frame_interfaces), blink::RemoteFrameToken(),
      base::UnguessableToken::Create());
  return static_cast<TestRenderFrameHost*>(fenced_frame->GetInnerRoot());
}

void TestRenderFrameHost::SendNavigate(int nav_entry_id,
                                       bool did_create_new_entry,
                                       const GURL& url) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, url,
                             ui::PAGE_TRANSITION_LINK, 0);
}

void TestRenderFrameHost::SendNavigateWithTransition(
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    ui::PageTransition transition) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, url,
                             transition, 0);
}

void TestRenderFrameHost::SendNavigateWithParameters(
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    ui::PageTransition transition,
    int response_code) {
  // This approach to determining whether a navigation is to be treated as
  // same document is not robust, as it will not handle pushState type
  // navigation. Do not use elsewhere!
  GURL::Replacements replacements;
  replacements.ClearRef();
  bool was_within_same_document =
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) &&
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
      (GetLastCommittedURL().is_valid() && !last_commit_was_error_page_ &&
       url.ReplaceComponents(replacements) ==
           GetLastCommittedURL().ReplaceComponents(replacements));

  auto params = BuildDidCommitParams(did_create_new_entry, url, transition,
                                     response_code, was_within_same_document);
  if (!was_within_same_document)
    params->embedding_token = base::UnguessableToken::Create();

  SendNavigateWithParams(std::move(params), was_within_same_document);
}

void TestRenderFrameHost::SendNavigateWithParams(
    mojom::DidCommitProvisionalLoadParamsPtr params,
    bool was_within_same_document) {
  SendNavigateWithParamsAndInterfaceParams(
      std::move(params),
      BuildDidCommitInterfaceParams(was_within_same_document),
      was_within_same_document);
}

void TestRenderFrameHost::SendNavigateWithParamsAndInterfaceParams(
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
    bool was_within_same_document) {
  last_commit_was_error_page_ = params->url_is_unreachable;
  if (was_within_same_document) {
    SendDidCommitSameDocumentNavigation(
        std::move(params), blink::mojom::SameDocumentNavigationType::kFragment,
        /*should_replace_current_entry=*/false);
  } else {
    DidCommitProvisionalLoad(std::move(params), std::move(interface_params));
  }
}

void TestRenderFrameHost::SendDidCommitSameDocumentNavigation(
    mojom::DidCommitProvisionalLoadParamsPtr params,
    blink::mojom::SameDocumentNavigationType same_document_navigation_type,
    bool should_replace_current_entry) {
  auto same_doc_params = mojom::DidCommitSameDocumentNavigationParams::New();
  same_doc_params->same_document_navigation_type =
      same_document_navigation_type;
  same_doc_params->should_replace_current_entry = should_replace_current_entry;
  params->http_status_code = last_http_status_code();
  DidCommitSameDocumentNavigation(std::move(params),
                                  std::move(same_doc_params));
}

void TestRenderFrameHost::SendStartLoadingForAsyncNavigationApiCommit() {
  StartLoadingForAsyncNavigationApiCommit();
}

void TestRenderFrameHost::SendRendererInitiatedNavigationRequest(
    const GURL& url,
    bool has_user_gesture) {
  // Since this is renderer-initiated navigation, the RenderFrame must be
  // initialized. Do it if it hasn't happened yet.
  InitializeRenderFrameIfNeeded();

  blink::mojom::BeginNavigationParamsPtr begin_params =
      blink::mojom::BeginNavigationParams::New(
          std::nullopt /* initiator_frame_token */, std::string() /* headers */,
          net::LOAD_NORMAL, false /* skip_service_worker */,
          blink::mojom::RequestContextType::HYPERLINK,
          blink::mojom::MixedContentContextType::kBlockable,
          false /* is_form_submission */,
          false /* was_initiated_by_link_click */,
          blink::mojom::ForceHistoryPush::kNo, GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */,
          GURL() /* client_side_redirect_url */,
          std::nullopt /* devtools_initiator_info */,
          nullptr /* trust_token_params */, std::nullopt /* impression */,
          base::TimeTicks() /* renderer_before_unload_start */,
          base::TimeTicks() /* renderer_before_unload_end */,
          blink::mojom::NavigationInitiatorActivationAndAdStatus::
              kDidNotStartWithTransientActivation,
          false /* is_container_initiated */,
          net::StorageAccessApiStatus::kNone, false /* has_rel_opener */);
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = url;
  common_params->initiator_origin = GetLastCommittedOrigin();
  common_params->referrer = blink::mojom::Referrer::New(
      GURL(), network::mojom::ReferrerPolicy::kDefault);
  common_params->transition = ui::PAGE_TRANSITION_LINK;
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->has_user_gesture = has_user_gesture;
  common_params->request_destination =
      network::mojom::RequestDestination::kDocument;

  mojo::PendingAssociatedRemote<mojom::NavigationClient>
      navigation_client_remote;
  GetRemoteAssociatedInterfaces()->GetInterface(
      navigation_client_remote.InitWithNewEndpointAndPassReceiver());
  BeginNavigation(std::move(common_params), std::move(begin_params),
                  mojo::NullRemote(), std::move(navigation_client_remote),
                  mojo::NullRemote(), mojo::NullReceiver());
}

void TestRenderFrameHost::SimulateDidChangeOpener(
    const std::optional<blink::LocalFrameToken>& opener_frame_token) {
  DidChangeOpener(opener_frame_token);
}

void TestRenderFrameHost::DidEnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  EnforceInsecureRequestPolicy(policy);
}

void TestRenderFrameHost::PrepareForCommit() {
  PrepareForCommitInternal(network::mojom::URLResponseHead::New(),
                           mojo::ScopedDataPipeConsumerHandle());
}

void TestRenderFrameHost::PrepareForCommitDeprecatedForNavigationSimulator(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle response_body) {
  PrepareForCommitInternal(std::move(response), std::move(response_body));
}

void TestRenderFrameHost::PrepareForCommitInternal(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle response_body) {
  NavigationRequest* request = frame_tree_node_->navigation_request();
  CHECK(request);
  bool have_to_make_network_request =
      IsURLHandledByNetworkStack(request->common_params().url) &&
      !NavigationTypeUtils::IsSameDocument(
          request->common_params().navigation_type);

  // Simulate a beforeUnload completion callback from the renderer if the
  // browser is waiting for it. If it runs it will update the request state.
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    static_cast<TestRenderFrameHost*>(frame_tree_node()->current_frame_host())
        ->SimulateBeforeUnloadCompleted(true);
  }

  if (!have_to_make_network_request)
    return;  // |request| is destructed by now.

  CHECK(request->state() >= NavigationRequest::WILL_START_NAVIGATION &&
        request->state() < NavigationRequest::READY_TO_COMMIT);

  if (!request->loader_for_testing()) {
    base::RunLoop loop;
    request->set_on_start_checks_complete_closure_for_testing(
        loop.QuitClosure());
    loop.Run();
  }

  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);

  // Simulate the network stack commit.
  if (response->load_timing.send_start.is_null()) {
    response->load_timing.send_start = base::TimeTicks::Now();
  }
  if (response->load_timing.receive_headers_start.is_null()) {
    response->load_timing.receive_headers_start = base::TimeTicks::Now();
  }
  if (!response->parsed_headers) {
    response->parsed_headers = network::PopulateParsedHeaders(
        response->headers.get(), request->GetURL());
  }
  // TODO(carlosk): Ideally, it should be possible someday to
  // fully commit the navigation at this call to CallOnResponseStarted.
  url_loader->CallOnResponseStarted(std::move(response),
                                    std::move(response_body), std::nullopt);
}

void TestRenderFrameHost::SimulateCommitProcessed(
    NavigationRequest* navigation_request,
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    bool same_document) {
  CHECK(params);
  if (!same_document) {
    // Note: Although the code does not prohibit the running of multiple
    // callbacks, no more than 1 callback will ever run, because navigation_id
    // is unique across all callback storages.
    {
      auto callback_it = commit_callback_.find(navigation_request);
      if (callback_it != commit_callback_.end()) {
        std::move(callback_it->second)
            .Run(std::move(params),
                 mojom::DidCommitProvisionalLoadInterfaceParams::New(
                     std::move(browser_interface_broker_receiver)));
        return;
      }
    }
    {
      auto callback_it = commit_failed_callback_.find(navigation_request);
      if (callback_it != commit_failed_callback_.end()) {
        std::move(callback_it->second)
            .Run(std::move(params),
                 mojom::DidCommitProvisionalLoadInterfaceParams::New(
                     std::move(browser_interface_broker_receiver)));
        return;
      }
    }
  }

  SendNavigateWithParamsAndInterfaceParams(
      std::move(params),
      mojom::DidCommitProvisionalLoadInterfaceParams::New(
          std::move(browser_interface_broker_receiver)),
      same_document);
}

#if !BUILDFLAG(IS_ANDROID)
void TestRenderFrameHost::CreateHidServiceForTesting(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  RenderFrameHostImpl::GetHidService(std::move(receiver));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void TestRenderFrameHost::CreateWebUsbServiceForTesting(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  RenderFrameHostImpl::CreateWebUsbService(std::move(receiver));
}

void TestRenderFrameHost::ResetLocalFrame() {
  local_frame_.reset();
}

void TestRenderFrameHost::SendCommitNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory,
    mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
        fetch_later_loader_factory,
    const std::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
    blink::mojom::PolicyContainerPtr policy_container,
    const blink::DocumentToken& document_token,
    const base::UnguessableToken& devtools_navigation_token) {
  CHECK(navigation_client);
  commit_callback_[navigation_request] =
      BuildCommitNavigationCallback(navigation_request);
}

void TestRenderFrameHost::SendCommitFailedNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int32_t error_code,
    int32_t extended_error_code,
    const std::optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    const blink::DocumentToken& document_token,
    blink::mojom::PolicyContainerPtr policy_container) {
  CHECK(navigation_client);
  commit_failed_callback_[navigation_request] =
      BuildCommitFailedNavigationCallback(navigation_request);
}

mojom::DidCommitProvisionalLoadParamsPtr
TestRenderFrameHost::BuildDidCommitParams(bool did_create_new_entry,
                                          const GURL& url,
                                          ui::PageTransition transition,
                                          int response_code,
                                          bool is_same_document) {
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->url = url;
  params->referrer = blink::mojom::Referrer::New();
  params->transition = transition;
  params->should_update_history = true;
  params->did_create_new_entry = did_create_new_entry;
  params->contents_mime_type = "text/html";
  params->method = "GET";
  params->http_status_code = response_code;
  params->history_list_was_cleared = simulate_history_list_was_cleared_;
  params->post_id = -1;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params->item_sequence_number =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  params->document_sequence_number = params->item_sequence_number + 1;

  // When the user hits enter in the Omnibox without changing the URL, Blink
  // behaves similarly to a reload and does not change the item and document
  // sequence numbers. Simulate this behavior here too.
  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    NavigationEntryImpl* entry =
        frame_tree_node()->navigator().controller().GetLastCommittedEntry();
    if (entry && entry->GetURL() == url) {
      FrameNavigationEntry* frame_entry =
          entry->GetFrameEntry(frame_tree_node());
      if (frame_entry) {
        params->item_sequence_number = frame_entry->item_sequence_number();
        params->document_sequence_number =
            frame_entry->document_sequence_number();
      }
    }
  }

  // In most cases, the origin will match the URL's origin.  Tests that need to
  // check corner cases (like about:blank) should specify the origin and
  // initiator_base_url params manually.
  url::Origin origin = url::Origin::Create(url);
  params->origin = origin;

  params->page_state = blink::PageState::CreateForTestingWithSequenceNumbers(
      url, params->item_sequence_number, params->document_sequence_number);

  return params;
}

mojom::DidCommitProvisionalLoadInterfaceParamsPtr
TestRenderFrameHost::BuildDidCommitInterfaceParams(bool is_same_document) {
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver;

  if (!is_same_document) {
    browser_interface_broker_receiver =
        mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
            .InitWithNewPipeAndPassReceiver();
  }

  auto interface_params = mojom::DidCommitProvisionalLoadInterfaceParams::New(
      std::move(browser_interface_broker_receiver));
  return interface_params;
}

void TestRenderFrameHost::AbortCommit(NavigationRequest* navigation_request) {
  NavigationRequestCancelled(navigation_request,
                             NavigationDiscardReason::kExplicitCancellation);
}

// static
mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver() {
  return mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
      .InitWithNewPipeAndPassReceiver();
}

// static
mojo::PendingAssociatedRemote<mojom::Frame>
TestRenderFrameHost::CreateStubFrameRemote() {
  // There's no renderer to pass the receiver to in these tests.
  mojo::AssociatedRemote<mojom::Frame> frame_remote;
  mojo::PendingAssociatedReceiver<mojom::Frame> frame_receiver =
      frame_remote.BindNewEndpointAndPassDedicatedReceiver();
  return frame_remote.Unbind();
}

// static
blink::mojom::PolicyContainerBindParamsPtr
TestRenderFrameHost::CreateStubPolicyContainerBindParams() {
  return blink::mojom::PolicyContainerBindParams::New(
      mojo::PendingAssociatedRemote<blink::mojom::PolicyContainerHost>()
          .InitWithNewEndpointAndPassReceiver());
}

// static
mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver() {
  mojo::AssociatedReceiver<blink::mojom::AssociatedInterfaceProvider> receiver(
      nullptr);
  mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
      pending_remote = receiver.BindNewEndpointAndPassDedicatedRemote();
  return receiver.Unbind();
}

void TestRenderFrameHost::SimulateLoadingCompleted(
    TestRenderFrameHost::LoadingScenario loading_scenario) {
  if (!is_loading())
    return;

  if (loading_scenario == LoadingScenario::NewDocumentNavigation) {
    if (is_main_frame())
      MainDocumentElementAvailable(/* uses_temporary_zoom_level */ false);

    DidDispatchDOMContentLoadedEvent();

    if (is_main_frame())
      DocumentOnLoadCompleted();

    DidFinishLoad(GetLastCommittedURL());
  }

  DidStopLoading();
}

}  // namespace content
