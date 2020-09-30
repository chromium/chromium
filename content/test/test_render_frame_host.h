// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_client.mojom-forward.h"
#include "content/common/navigation_params.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace net {
class IPEndPoint;
}

namespace content {

class TestRenderFrameHostCreationObserver : public WebContentsObserver {
 public:
  explicit TestRenderFrameHostCreationObserver(WebContents* web_contents);
  ~TestRenderFrameHostCreationObserver() override;

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  RenderFrameHost* last_created_frame() const { return last_created_frame_; }

 private:
  RenderFrameHost* last_created_frame_;
};

class TestRenderFrameHost : public RenderFrameHostImpl,
                            public RenderFrameHostTester {
 public:
  TestRenderFrameHost(SiteInstance* site_instance,
                      scoped_refptr<RenderViewHostImpl> render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int32_t routing_id,
                      const base::UnguessableToken& frame_token,
                      LifecycleState lifecyle_state);
  ~TestRenderFrameHost() override;

  // RenderFrameHostImpl overrides (same values, but in Test*/Mock* types)
  TestRenderViewHost* GetRenderViewHost() override;
  MockRenderProcessHost* GetProcess() override;
  TestRenderWidgetHost* GetRenderWidgetHost() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  void ReportHeavyAdIssue(blink::mojom::HeavyAdResolutionStatus resolution,
                          blink::mojom::HeavyAdReason reason) override;
  void AddUniqueMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                                 const std::string& message) override;
  bool IsTestRenderFrameHost() const override;

  // Public overrides to expose RenderFrameHostImpl's mojo methods to tests.
  void DidFailLoadWithError(const GURL& url, int error_code) override;

  // RenderFrameHostTester implementation.
  void InitializeRenderFrameIfNeeded() override;
  TestRenderFrameHost* AppendChild(const std::string& frame_name) override;
  TestRenderFrameHost* AppendChildWithPolicy(
      const std::string& frame_name,
      const blink::ParsedFeaturePolicy& allow) override;
  void Detach() override;
  void SendNavigateWithTransition(int nav_entry_id,
                                  bool did_create_new_entry,
                                  const GURL& url,
                                  ui::PageTransition transition);
  void SimulateBeforeUnloadCompleted(bool proceed) override;
  void SimulateUnloadACK() override;
  void SimulateFeaturePolicyHeader(
      blink::mojom::FeaturePolicyFeature feature,
      const std::vector<url::Origin>& allowlist) override;
  void SimulateUserActivation() override;
  const std::vector<std::string>& GetConsoleMessages() override;
  int GetHeavyAdIssueCount(HeavyAdIssueType type) override;

  void SendNavigate(int nav_entry_id,
                    bool did_create_new_entry,
                    const GURL& url);
  void SendNavigateWithParams(
      FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      bool was_within_same_document);
  void SendNavigateWithParamsAndInterfaceParams(
      FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params,
      bool was_within_same_document);

  // With the current navigation logic this method is a no-op.
  // Simulates a renderer-initiated navigation to |url| starting in the
  // RenderFrameHost.
  // DEPRECATED: use NavigationSimulator instead.
  void SimulateNavigationStart(const GURL& url);

  // Simulates a redirect to |new_url| for the navigation in the
  // RenderFrameHost.
  // DEPRECATED: use NavigationSimulator instead.
  void SimulateRedirect(const GURL& new_url);

  // Simulates a navigation to |url| committing in the RenderFrameHost.
  // DEPRECATED: use NavigationSimulator instead.
  void SimulateNavigationCommit(const GURL& url);

  // This method simulates receiving a BeginNavigation IPC.
  // DEPRECATED: use NavigationSimulator instead.
  void SendRendererInitiatedNavigationRequest(const GURL& url,
                                              bool has_user_gesture);

  void SimulateDidChangeOpener(
      const base::Optional<base::UnguessableToken>& opener_frame_token);

  void DidEnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy);

  // If set, navigations will appear to have cleared the history list in the
  // RenderFrame
  // (FrameHostMsg_DidCommitProvisionalLoad_Params::history_list_was_cleared).
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
      const net::IPEndPoint& remote_endpoint,
      bool was_fetched_via_cache,
      bool is_signed_exchange_inner_response,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      base::Optional<net::SSLInfo> ssl_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Used to simulate the commit of a navigation having been processed in the
  // renderer. If parameters required to commit are not provided, they will be
  // set to default null values.
  void SimulateCommitProcessed(
      NavigationRequest* navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      bool same_document);

  // Send a message with the sandbox flags and feature policy
  void SendFramePolicy(network::mojom::WebSandboxFlags sandbox_flags,
                       const blink::ParsedFeaturePolicy& fp_header,
                       const blink::DocumentPolicyFeatureState& dp_header);

  // Creates a WebBluetooth Service with a dummy InterfaceRequest.
  WebBluetoothServiceImpl* CreateWebBluetoothServiceForTesting();

  bool last_commit_was_error_page() const {
    return last_commit_was_error_page_;
  }

  // Returns a PendingReceiver<InterfaceProvider> that is safe to bind to an
  // implementation, but will never receive any interface receivers.
  static mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
  CreateStubInterfaceProviderReceiver();

  // Returns a PendingReceiver<BrowserInterfaceBroker> that is safe to bind to
  // an implementation, but will never receive any interface requests.
  static mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
  CreateStubBrowserInterfaceBrokerReceiver();

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

  // Expose CreateNewFullscreenWidget for tests.
  using RenderFrameHostImpl::CreateNewFullscreenWidget;

 protected:
  void SendCommitNavigation(
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
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token) override;
  void SendCommitFailedNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int32_t error_code,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories) override;

 private:
  void SendNavigateWithParameters(int nav_entry_id,
                                  bool did_create_new_entry,
                                  const GURL& url,
                                  ui::PageTransition transition,
                                  int response_code);

  void PrepareForCommitInternal(
      const net::IPEndPoint& remote_endpoint,
      bool was_fetched_via_cache,
      bool is_signed_exchange_inner_response,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      base::Optional<net::SSLInfo> ssl_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Computes the page ID for a pending navigation in this RenderFrameHost;
  int32_t ComputeNextPageID();

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  BuildDidCommitParams(int nav_entry_id,
                       bool did_create_new_entry,
                       const GURL& url,
                       ui::PageTransition transition,
                       int response_code);

  mojom::DidCommitProvisionalLoadInterfaceParamsPtr
  BuildDidCommitInterfaceParams(bool is_same_document);

  // Keeps a running vector of messages sent to AddMessageToConsole.
  std::vector<std::string> console_messages_;

  // Keep a count of the heavy ad issues sent to ReportHeavyAdIssue.
  int heavy_ad_issue_network_count_ = 0;
  int heavy_ad_issue_cpu_total_count_ = 0;
  int heavy_ad_issue_cpu_peak_count_ = 0;

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

  mojo::PendingRemote<blink::mojom::WebBluetoothService>
      dummy_web_bluetooth_service_remote_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameHost);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
