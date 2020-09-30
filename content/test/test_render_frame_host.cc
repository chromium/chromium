// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/guid.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/view_messages.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/navigation_policy.h"
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
#include "services/network/public/cpp/parsed_headers.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "ui/base/page_transition_types.h"

namespace content {

TestRenderFrameHostCreationObserver::TestRenderFrameHostCreationObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents), last_created_frame_(nullptr) {}

TestRenderFrameHostCreationObserver::~TestRenderFrameHostCreationObserver() =
    default;

void TestRenderFrameHostCreationObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  last_created_frame_ = render_frame_host;
}

TestRenderFrameHost::TestRenderFrameHost(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int32_t routing_id,
    const base::UnguessableToken& frame_token,
    RenderFrameHostImpl::LifecycleState lifecyle_state)
    : RenderFrameHostImpl(site_instance,
                          std::move(render_view_host),
                          delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          frame_token,
                          /*renderer_initiated_creation=*/false,
                          lifecyle_state),
      child_creation_observer_(delegate ? delegate->GetAsWebContents()
                                        : nullptr),
      simulate_history_list_was_cleared_(false),
      last_commit_was_error_page_(false) {}

TestRenderFrameHost::~TestRenderFrameHost() = default;

TestRenderViewHost* TestRenderFrameHost::GetRenderViewHost() {
  return static_cast<TestRenderViewHost*>(
      RenderFrameHostImpl::GetRenderViewHost());
}

MockRenderProcessHost* TestRenderFrameHost::GetProcess() {
  return static_cast<MockRenderProcessHost*>(RenderFrameHostImpl::GetProcess());
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

void TestRenderFrameHost::ReportHeavyAdIssue(
    blink::mojom::HeavyAdResolutionStatus resolution,
    blink::mojom::HeavyAdReason reason) {
  switch (reason) {
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
  RenderFrameHostImpl::ReportHeavyAdIssue(resolution, reason);
}

void TestRenderFrameHost::AddUniqueMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  if (std::find(console_messages_.begin(), console_messages_.end(), message) ==
      console_messages_.end()) {
    console_messages_.push_back(message);
  }
  RenderFrameHostImpl::AddUniqueMessageToConsole(level, message);
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
    RenderViewHostTester::For(render_view_host())
        ->CreateTestRenderView(base::nullopt, MSG_ROUTING_NONE, false);
  }
}

TestRenderFrameHost* TestRenderFrameHost::AppendChild(
    const std::string& frame_name) {
  return AppendChildWithPolicy(frame_name, {});
}

TestRenderFrameHost* TestRenderFrameHost::AppendChildWithPolicy(
    const std::string& frame_name,
    const blink::ParsedFeaturePolicy& allow) {
  std::string frame_unique_name = base::GenerateGUID();
  OnCreateChildFrame(
      GetProcess()->GetNextRoutingID(), CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, frame_name, frame_unique_name,
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(
          {network::mojom::WebSandboxFlags::kNone, allow, {}, true, false}),
      blink::mojom::FrameOwnerProperties(),
      blink::mojom::FrameOwnerElementType::kIframe);
  return static_cast<TestRenderFrameHost*>(
      child_creation_observer_.last_created_frame());
}

void TestRenderFrameHost::Detach() {
  DetachForTesting();
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

void TestRenderFrameHost::SimulateNavigationCommit(const GURL& url) {
  if (frame_tree_node_->navigation_request())
    PrepareForCommit();

  bool is_auto_subframe =
      GetParent() && !frame_tree_node()->has_committed_real_load();

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.url = url;
  params.origin = url::Origin::Create(url);
  if (!GetParent())
    params.transition = ui::PAGE_TRANSITION_LINK;
  else if (is_auto_subframe)
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  else
    params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = true;
  params.did_create_new_entry = !is_auto_subframe;
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = "text/html";
  params.method = "GET";
  params.http_status_code = 200;
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = url;

  url::Replacements<char> replacements;
  replacements.ClearRef();

  // This approach to determining whether a navigation is to be treated as
  // same document is not robust, as it will not handle pushState type
  // navigation. Do not use elsewhere!
  bool was_within_same_document =
      (GetLastCommittedURL().is_valid() && !last_commit_was_error_page_ &&
       url.ReplaceComponents(replacements) ==
           GetLastCommittedURL().ReplaceComponents(replacements));

  params.page_state = PageState::CreateForTesting(url, false, nullptr, nullptr);
  if (!was_within_same_document)
    params.embedding_token = base::UnguessableToken::Create();

  SendNavigateWithParams(&params, was_within_same_document);
}

void TestRenderFrameHost::SimulateBeforeUnloadCompleted(bool proceed) {
  base::TimeTicks now = base::TimeTicks::Now();
  ProcessBeforeUnloadCompleted(
      proceed, false /* treat_as_final_completion_callback */, now, now);
}

void TestRenderFrameHost::SimulateUnloadACK() {
  OnUnloadACK();
}

void TestRenderFrameHost::SimulateFeaturePolicyHeader(
    blink::mojom::FeaturePolicyFeature feature,
    const std::vector<url::Origin>& allowlist) {
  blink::ParsedFeaturePolicy header(1);
  header[0].feature = feature;
  header[0].matches_all_origins = false;
  header[0].matches_opaque_src = false;
  for (const auto& origin : allowlist) {
    header[0].allowed_origins.push_back(origin);
  }
  DidSetFramePolicyHeaders(network::mojom::WebSandboxFlags::kNone, header,
                           {} /* dp_header */);
}

void TestRenderFrameHost::SimulateUserActivation() {
  frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
}

const std::vector<std::string>& TestRenderFrameHost::GetConsoleMessages() {
  return console_messages_;
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

void TestRenderFrameHost::SendNavigate(int nav_entry_id,
                                       bool did_create_new_entry,
                                       const GURL& url) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, url,
                             ui::PAGE_TRANSITION_LINK, 200);
}

void TestRenderFrameHost::SendNavigateWithTransition(
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    ui::PageTransition transition) {
  SendNavigateWithParameters(nav_entry_id, did_create_new_entry, url,
                             transition, 200);
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
  url::Replacements<char> replacements;
  replacements.ClearRef();
  bool was_within_same_document =
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) &&
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
      (GetLastCommittedURL().is_valid() && !last_commit_was_error_page_ &&
       url.ReplaceComponents(replacements) ==
           GetLastCommittedURL().ReplaceComponents(replacements));

  auto params = BuildDidCommitParams(nav_entry_id, did_create_new_entry, url,
                                     transition, response_code);
  if (!was_within_same_document)
    params->embedding_token = base::UnguessableToken::Create();

  SendNavigateWithParams(params.get(), was_within_same_document);
}

void TestRenderFrameHost::SendNavigateWithParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    bool was_within_same_document) {
  SendNavigateWithParamsAndInterfaceParams(
      std::move(params),
      BuildDidCommitInterfaceParams(was_within_same_document),
      was_within_same_document);
}

void TestRenderFrameHost::SendNavigateWithParamsAndInterfaceParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
    bool was_within_same_document) {
  if (was_within_same_document) {
    DidCommitSameDocumentNavigation(
        std::make_unique<FrameHostMsg_DidCommitProvisionalLoad_Params>(
            *params));
  } else {
    DidCommitProvisionalLoad(
        std::make_unique<FrameHostMsg_DidCommitProvisionalLoad_Params>(*params),
        std::move(interface_params));
  }
  last_commit_was_error_page_ = params->url_is_unreachable;
}

void TestRenderFrameHost::SendRendererInitiatedNavigationRequest(
    const GURL& url,
    bool has_user_gesture) {
  // Since this is renderer-initiated navigation, the RenderFrame must be
  // initialized. Do it if it hasn't happened yet.
  InitializeRenderFrameIfNeeded();

  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New(
          MSG_ROUTING_NONE /* initiator_routing_id */,
          std::string() /* headers */, net::LOAD_NORMAL,
          false /* skip_service_worker */,
          blink::mojom::RequestContextType::HYPERLINK,
          network::mojom::RequestDestination::kDocument,
          blink::WebMixedContentContextType::kBlockable,
          false /* is_form_submission */,
          false /* was_initiated_by_link_click */,
          GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */,
          GURL() /* client_side_redirect_url */,
          base::nullopt /* devtools_initiator_info */,
          false /* force_ignore_site_for_cookies */,
          nullptr /* trust_token_params */, base::nullopt /* impression */,
          base::TimeTicks() /* renderer_before_unload_start */,
          base::TimeTicks() /* renderer_before_unload_end */);
  auto common_params = CreateCommonNavigationParams();
  common_params->url = url;
  common_params->initiator_origin = GetLastCommittedOrigin();
  common_params->referrer = blink::mojom::Referrer::New(
      GURL(), network::mojom::ReferrerPolicy::kDefault);
  common_params->transition = ui::PAGE_TRANSITION_LINK;
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->has_user_gesture = has_user_gesture;

  mojo::PendingAssociatedRemote<mojom::NavigationClient>
      navigation_client_remote;
  GetRemoteAssociatedInterfaces()->GetInterface(
      navigation_client_remote.InitWithNewEndpointAndPassReceiver());
  BeginNavigation(std::move(common_params), std::move(begin_params),
                  mojo::NullRemote(), std::move(navigation_client_remote),
                  mojo::NullRemote());
}

void TestRenderFrameHost::SimulateDidChangeOpener(
    const base::Optional<base::UnguessableToken>& opener_frame_token) {
  DidChangeOpener(opener_frame_token);
}

void TestRenderFrameHost::DidEnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  EnforceInsecureRequestPolicy(policy);
}

void TestRenderFrameHost::PrepareForCommit() {
  PrepareForCommitInternal(net::IPEndPoint(),
                           /* was_fetched_via_cache=*/false,
                           /* is_signed_exchange_inner_response=*/false,
                           net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
                           base::nullopt, nullptr);
}

void TestRenderFrameHost::PrepareForCommitDeprecatedForNavigationSimulator(
    const net::IPEndPoint& remote_endpoint,
    bool was_fetched_via_cache,
    bool is_signed_exchange_inner_response,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    base::Optional<net::SSLInfo> ssl_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  PrepareForCommitInternal(remote_endpoint, was_fetched_via_cache,
                           is_signed_exchange_inner_response, connection_info,
                           ssl_info, response_headers);
}

void TestRenderFrameHost::PrepareForCommitInternal(
    const net::IPEndPoint& remote_endpoint,
    bool was_fetched_via_cache,
    bool is_signed_exchange_inner_response,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    base::Optional<net::SSLInfo> ssl_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
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
  auto response = network::mojom::URLResponseHead::New();
  response->remote_endpoint = remote_endpoint;
  response->was_fetched_via_cache = was_fetched_via_cache;
  response->is_signed_exchange_inner_response =
      is_signed_exchange_inner_response;
  response->connection_info = connection_info;
  response->ssl_info = ssl_info;
  response->load_timing.send_start = base::TimeTicks::Now();
  response->load_timing.receive_headers_start = base::TimeTicks::Now();
  response->headers = response_headers;
  response->parsed_headers =
      network::PopulateParsedHeaders(response->headers, request->GetURL());
  // TODO(carlosk): Ideally, it should be possible someday to
  // fully commit the navigation at this call to CallOnResponseStarted.
  url_loader->CallOnResponseStarted(std::move(response));
}

void TestRenderFrameHost::SimulateCommitProcessed(
    NavigationRequest* navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        interface_provider_receiver,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    bool same_document) {
  CHECK(params);

  bool did_commit = false;
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
                     std::move(interface_provider_receiver),
                     std::move(browser_interface_broker_receiver)));
        did_commit = true;
      }
    }
    {
      auto callback_it = commit_failed_callback_.find(navigation_request);
      if (callback_it != commit_failed_callback_.end()) {
        std::move(callback_it->second)
            .Run(std::move(params),
                 mojom::DidCommitProvisionalLoadInterfaceParams::New(
                     std::move(interface_provider_receiver),
                     std::move(browser_interface_broker_receiver)));
        did_commit = true;
      }
    }
  }

  if (!did_commit) {
    SendNavigateWithParamsAndInterfaceParams(
        params.get(),
        mojom::DidCommitProvisionalLoadInterfaceParams::New(
            std::move(interface_provider_receiver),
            std::move(browser_interface_broker_receiver)),
        same_document);
  }
}

WebBluetoothServiceImpl*
TestRenderFrameHost::CreateWebBluetoothServiceForTesting() {
  RenderFrameHostImpl::CreateWebBluetoothService(
      dummy_web_bluetooth_service_remote_.InitWithNewPipeAndPassReceiver());
  return RenderFrameHostImpl::GetWebBluetoothServiceForTesting();
}

void TestRenderFrameHost::SendFramePolicy(
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& fp_header,
    const blink::DocumentPolicyFeatureState& dp_header) {
  DidSetFramePolicyHeaders(sandbox_flags, fp_header, dp_header);
}

void TestRenderFrameHost::SendCommitNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token) {
  CHECK(navigation_client);
  commit_callback_[navigation_request] =
      BuildCommitNavigationCallback(navigation_request);
}

void TestRenderFrameHost::SendCommitFailedNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int32_t error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  CHECK(navigation_client);
  commit_failed_callback_[navigation_request] =
      BuildCommitFailedNavigationCallback(navigation_request);
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
TestRenderFrameHost::BuildDidCommitParams(int nav_entry_id,
                                          bool did_create_new_entry,
                                          const GURL& url,
                                          ui::PageTransition transition,
                                          int response_code) {
  auto params =
      std::make_unique<FrameHostMsg_DidCommitProvisionalLoad_Params>();
  params->nav_entry_id = nav_entry_id;
  params->url = url;
  params->transition = transition;
  params->should_update_history = true;
  params->did_create_new_entry = did_create_new_entry;
  params->should_replace_current_entry = false;
  params->gesture = NavigationGestureUser;
  params->contents_mime_type = "text/html";
  params->method = "GET";
  params->http_status_code = response_code;
  params->history_list_was_cleared = simulate_history_list_was_cleared_;
  params->original_request_url = url;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params->item_sequence_number = base::Time::Now().ToDoubleT() * 1000000;
  params->document_sequence_number = params->item_sequence_number + 1;

  // When the user hits enter in the Omnibox without changing the URL, Blink
  // behaves similarly to a reload and does not change the item and document
  // sequence numbers. Simulate this behavior here too.
  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    NavigationEntryImpl* entry =
        static_cast<NavigationEntryImpl*>(frame_tree_node()
                                              ->navigator()
                                              .GetController()
                                              ->GetLastCommittedEntry());
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
  // check corner cases (like about:blank) should specify the origin param
  // manually.
  url::Origin origin = url::Origin::Create(url);
  params->origin = origin;

  params->page_state = PageState::CreateForTestingWithSequenceNumbers(
      url, params->item_sequence_number, params->document_sequence_number);

  return params;
}

mojom::DidCommitProvisionalLoadInterfaceParamsPtr
TestRenderFrameHost::BuildDidCommitInterfaceParams(bool is_same_document) {
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider;
  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
      interface_provider_receiver;

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver;

  if (!is_same_document) {
    interface_provider_receiver =
        interface_provider.InitWithNewPipeAndPassReceiver();
    browser_interface_broker_receiver =
        mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
            .InitWithNewPipeAndPassReceiver();
  }

  auto interface_params = mojom::DidCommitProvisionalLoadInterfaceParams::New(
      std::move(interface_provider_receiver),
      std::move(browser_interface_broker_receiver));
  return interface_params;
}

void TestRenderFrameHost::AbortCommit(NavigationRequest* navigation_request) {
  OnCrossDocumentCommitProcessed(navigation_request,
                                 blink::mojom::CommitResult::Aborted);
}

// static
mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
TestRenderFrameHost::CreateStubInterfaceProviderReceiver() {
  mojo::PendingRemote<::service_manager::mojom::InterfaceProvider>
      dead_interface_provider_proxy;
  return dead_interface_provider_proxy.InitWithNewPipeAndPassReceiver();
}

// static
mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver() {
  return mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
      .InitWithNewPipeAndPassReceiver();
}

void TestRenderFrameHost::SimulateLoadingCompleted(
    TestRenderFrameHost::LoadingScenario loading_scenario) {
  if (!is_loading())
    return;

  if (loading_scenario == LoadingScenario::NewDocumentNavigation) {
    if (frame_tree_node_->IsMainFrame())
      DocumentAvailableInMainFrame(/* uses_temporary_zoom_level */ false);

    DidFinishDocumentLoad();

    DocumentOnLoadCompleted();

    DidFinishLoad(GetLastCommittedURL());
  }

  DidStopLoading();
}

}  // namespace content
