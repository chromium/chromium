// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/navigation_client.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/mock_agent_scheduling_group_host.h"
#include "content/test/test_page.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "ui/base/page_transition_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {

class TestRenderFrameHostCreationObserver : public WebContentsObserver {
 public:
  explicit TestRenderFrameHostCreationObserver(WebContents* web_contents);
  ~TestRenderFrameHostCreationObserver() override;

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

  RenderFrameHost* last_created_frame() const { return last_created_frame_; }

 private:
  raw_ptr<RenderFrameHost> last_created_frame_ = nullptr;
};

class TestRenderFrameHost : public RenderFrameHostImpl,
                            public RenderFrameHostTester {
 public:
  TestRenderFrameHost(
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
      LifecycleStateImpl lifecycle_state,
      scoped_refptr<BrowsingContextState> browsing_context_state);

  TestRenderFrameHost(const TestRenderFrameHost&) = delete;
  TestRenderFrameHost& operator=(const TestRenderFrameHost&) = delete;

  ~TestRenderFrameHost() override;

  // Flushes mojo messages on `local_frame_`.
  void FlushLocalFrameMessages();

  // RenderFrameHostImpl overrides (same values, but in Test*/Mock* types)
  TestRenderViewHost* GetRenderViewHost() const override;
  TestPage& GetPage() override;
  MockRenderProcessHost* GetProcess() const override;
  MockAgentSchedulingGroupHost& GetAgentSchedulingGroup() override;
  TestRenderWidgetHost* GetRenderWidgetHost() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  void ReportInspectorIssue(blink::mojom::InspectorIssueInfoPtr issue) override;
  bool IsTestRenderFrameHost() const override;

  // Public overrides to expose RenderFrameHostImpl's mojo methods to tests.
  void DidFailLoadWithError(const GURL& url, int error_code) override;

  // RenderFrameHostTester implementation.
  void InitializeRenderFrameIfNeeded() override;
  TestRenderFrameHost* AppendChild(const std::string& frame_name) override;
  TestRenderFrameHost* AppendChildWithPolicy(
      const std::string& frame_name,
      const blink::ParsedPermissionsPolicy& allow) override;
  TestRenderFrameHost* AppendCredentiallessChild(
      const std::string& frame_name) override;
  void Detach() override;
  void SendNavigateWithTransition(int nav_entry_id,
                                  bool did_create_new_entry,
                                  const GURL& url,
                                  ui::PageTransition transition);
  void SimulateBeforeUnloadCompleted(bool proceed) override;
  void SimulateUnloadACK() override;
  void SimulateUserActivation() override;
  const std::vector<std::string>& GetConsoleMessages() override;
  void ClearConsoleMessages() override;
  int GetHeavyAdIssueCount(HeavyAdIssueType type) override;
  void SimulateManifestURLUpdate(const GURL& manifest_url) override;
  TestRenderFrameHost* AppendFencedFrame() override;
  void CreateWebUsbServiceForTesting(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) override;
  void ResetLocalFrame() override;

#if !BUILDFLAG(IS_ANDROID)
  void CreateHidServiceForTesting(
      mojo::PendingReceiver<blink::mojom::HidService> receiver) override;
#endif  // !BUILDFLAG(IS_ANDROID)

  void SendNavigate(int nav_entry_id,
                    bool did_create_new_entry,
                    const GURL& url);
  void SendNavigateWithParams(mojom::DidCommitProvisionalLoadParamsPtr params,
                              bool was_within_same_document);
  void SendNavigateWithParamsAndInterfaceParams(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
      bool was_within_same_document);
  void SendDidCommitSameDocumentNavigation(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      blink::mojom::SameDocumentNavigationType same_document_navigation_type,
      bool should_replace_current_entry);
  void SendStartLoadingForAsyncNavigationApiCommit();

  // With the current navigation logic this method is a no-op.
  // Simulates a renderer-initiated navigation to |url| starting in the
  // RenderFrameHost.
  // DEPRECATED: use NavigationSimulator instead.
  void SimulateNavigationStart(const GURL& url);

  // Simulates a redirect to |new_url| for the navigation in the
  // RenderFrameHost.
  // DEPRECATED: use NavigationSimulator instead.
  void SimulateRedirect(const GURL& new_url);

  // This method simulates receiving a BeginNavigation IPC.
  // DEPRECATED: use NavigationSimulator instead.
  void SendRendererInitiatedNavigationRequest(const GURL& url,
                                              bool has_user_gesture);

  void SimulateDidChangeOpener(
      const std::optional<blink::LocalFrameToken>& opener_frame_token);

  void DidEnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy);

  // Returns the number of FedCM issues of FederatedAuthRequestResult type
  // `status_type` sent to DevTools. If `status_type` is std::nullopt, returns
  // the total number of FedCM issues of any type sent to DevTools.
  int GetFederatedAuthRequestIssueCount(
      std::optional<blink::mojom::FederatedAuthRequestResult> status_type);

  // Returns the number of FedCM issues of FederatedAuthUserInfoRequestResult
  // type `status_type` sent to DevTools. If `status_type` is std::nullopt,
  // returns the total number of FedCM issues of any type sent to DevTools.
  int GetFederatedAuthUserInfoRequestIssueCount(
      std::optional<blink::mojom::FederatedAuthUserInfoRequestResult>
          status_type);

  // If set, navigations will appear to have cleared the history list in the
  // RenderFrame (DidCommitProvisionalLoadParams::history_list_was_cleared).
  // False by default.
  void set_simulate_history_list_was_cleared(bool cleared) {
    simulate_history_list_was_cleared_ = cleared;
  }

  // Advances the RenderFrameHost (and through it the RenderFrameHostManager) to
  // a state where a new navigation can be committed by a renderer. This
  // simulates a BeforeUnload completion callback from the renderer, and the
  // interaction with the IO thread up until the response is ready to commit.
  void PrepareForCommit();

  // Like PrepareForCommit, but with the socket address when needed.
  // TODO(clamy): Have NavigationSimulator make the relevant calls directly and
  // remove this function.
  void PrepareForCommitDeprecatedForNavigationSimulator(
      network::mojom::URLResponseHeadPtr response,
      mojo::ScopedDataPipeConsumerHandle response_body);

  // Used to simulate the commit of a navigation having been processed in the
  // renderer. If parameters required to commit are not provided, they will be
  // set to default null values.
  void SimulateCommitProcessed(
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      bool same_document);

  // Returns a pending Frame remote that represents a connection to a non-
  // existent renderer, where all messages will go into the void.
  static mojo::PendingAssociatedRemote<mojom::Frame> CreateStubFrameRemote();

  // Returns a PendingReceiver<BrowserInterfaceBroker> that is safe to bind to
  // an implementation, but will never receive any interface requests.
  static mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  CreateStubBrowserInterfaceBrokerReceiver();

  // Returns an `AssociatedInterfaceProvider` that will never receive any
  // interface requests.
  static mojo::PendingAssociatedReceiver<
      blink::mojom::AssociatedInterfaceProvider>
  CreateStubAssociatedInterfaceProviderReceiver();

  // Returns a blink::mojom::PolicyContainerBindParams containing a
  // PendingAssociatedReceiver<PolicyContainerHost> and a
  // PendingReceiver<PolicyContainerHostKeepAliveHandle> that are safe to bind
  // to an implementation, but will never receive any interface requests. To be
  // passed to RenderFrameHostImpl::CreateChildFrame.
  static blink::mojom::PolicyContainerBindParamsPtr
  CreateStubPolicyContainerBindParams();

  // This simulates aborting a cross document navigation.
  // Will abort the navigation with the given |navigation_id|.
  void AbortCommit(NavigationRequest* navigation_request);

  // Returns the navigations that are trying to commit.
  const std::map<NavigationRequest*, std::unique_ptr<NavigationRequest>>&
  navigation_requests() {
    return navigation_requests_;
  }

  enum class LoadingScenario {
    NewDocumentNavigation,
    kSameDocumentNavigation,

    // TODO(altimin): Improve handling for the scenarios where navigation or
    // page load have failed.
    kOther
  };

  // Simulates RenderFrameHost finishing loading and dispatching all relevant
  // callbacks.
  void SimulateLoadingCompleted(LoadingScenario loading_scenario);

  // Expose this for testing.
  using RenderFrameHostImpl::SetPolicyContainerHost;

 protected:
  void SendCommitNavigation(
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
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
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
      const base::UnguessableToken& devtools_navigation_token) override;
  void SendCommitFailedNavigation(
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
      blink::mojom::PolicyContainerPtr policy_container) override;

 private:
  void SendNavigateWithParameters(int nav_entry_id,
                                  bool did_create_new_entry,
                                  const GURL& url,
                                  ui::PageTransition transition,
                                  int response_code);

  void PrepareForCommitInternal(
      network::mojom::URLResponseHeadPtr response,
      mojo::ScopedDataPipeConsumerHandle response_body);

  // Computes the page ID for a pending navigation in this RenderFrameHost;
  int32_t ComputeNextPageID();

  mojom::DidCommitProvisionalLoadParamsPtr BuildDidCommitParams(
      bool did_create_new_entry,
      const GURL& url,
      ui::PageTransition transition,
      int response_code,
      bool is_same_document);

  mojom::DidCommitProvisionalLoadInterfaceParamsPtr
  BuildDidCommitInterfaceParams(bool is_same_document);

  // Keeps a running vector of messages sent to AddMessageToConsole.
  std::vector<std::string> console_messages_;

  // Keep a count of the heavy ad issues sent to ReportInspectorIssue.
  int heavy_ad_issue_network_count_ = 0;
  int heavy_ad_issue_cpu_total_count_ = 0;
  int heavy_ad_issue_cpu_peak_count_ = 0;

  // Keeps a count of federated authentication request issues sent to
  // ReportInspectorIssue.
  std::unordered_map<blink::mojom::FederatedAuthRequestResult, int>
      federated_auth_counts_;

  // Keeps a count of getUserInfo() issues sent to ReportInspectorIssue.
  std::unordered_map<blink::mojom::FederatedAuthUserInfoRequestResult, int>
      federated_auth_user_info_counts_;

  TestRenderFrameHostCreationObserver child_creation_observer_;

  // See set_simulate_history_list_was_cleared() above.
  bool simulate_history_list_was_cleared_;

  // The last commit was for an error page.
  bool last_commit_was_error_page_;

  std::map<NavigationRequest*,
           mojom::NavigationClient::CommitNavigationCallback>
      commit_callback_;
  std::map<NavigationRequest*,
           mojom::NavigationClient::CommitFailedNavigationCallback>
      commit_failed_callback_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
