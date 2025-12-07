// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_EARLY_HINTS_MANAGER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_EARLY_HINTS_MANAGER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/request_priority.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom

struct ResourceRequest;
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class BrowserContext;
class StoragePartition;

// Contains parameters to create NavigationEarlyHintsManager.
struct CONTENT_EXPORT NavigationEarlyHintsManagerParams {
  NavigationEarlyHintsManagerParams(
      const url::Origin& origin,
      net::IsolationInfo isolation_info,
      mojo::Remote<network::mojom::URLLoaderFactory> loader_factory);
  ~NavigationEarlyHintsManagerParams();

  NavigationEarlyHintsManagerParams(NavigationEarlyHintsManagerParams&&);
  NavigationEarlyHintsManagerParams& operator=(
      NavigationEarlyHintsManagerParams&&);

  NavigationEarlyHintsManagerParams(const NavigationEarlyHintsManagerParams&) =
      delete;
  NavigationEarlyHintsManagerParams& operator=(
      const NavigationEarlyHintsManagerParams&) = delete;

  url::Origin origin;
  net::IsolationInfo isolation_info;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
};

// Handles 103 Early Hints responses for navigation. Responsible for resource
// hints in Early Hints responses. Created when the first 103 response is
// received and owned by NavigationURLLoaderImpl until the final response to the
// navigation request is received. NavigationURLLoaderImpl transfers the
// ownership of this instance to RenderFrameHostImpl via NavigationRequest when
// the navigation is committed so that this can outlive NavigationURLLoaderImpl
// until inflight preloads finish.
class CONTENT_EXPORT NavigationEarlyHintsManager {
 public:
  // Contains results of a preload request.
  struct CONTENT_EXPORT PreloadedResource {
    PreloadedResource();
    ~PreloadedResource();
    PreloadedResource(const PreloadedResource&);
    PreloadedResource& operator=(const PreloadedResource&);

    // Completion error code. Set only when network request is completed.
    std::optional<int> error_code;
    // Optional CORS error details.
    std::optional<network::CorsErrorStatus> cors_error_status;
    // True when the preload was canceled. When true, the response was already
    // in the disk cache.
    bool was_canceled = false;
  };
  using PreloadedResources = base::flat_map<GURL, PreloadedResource>;

  NavigationEarlyHintsManager(BrowserContext& browser_context,
                              StoragePartition& storage_partition,
                              FrameTreeNodeId frame_tree_node_id,
                              NavigationEarlyHintsManagerParams params);

  ~NavigationEarlyHintsManager();

  NavigationEarlyHintsManager(const NavigationEarlyHintsManager&) = delete;
  NavigationEarlyHintsManager& operator=(const NavigationEarlyHintsManager&) =
      delete;
  NavigationEarlyHintsManager(NavigationEarlyHintsManager&&) = delete;
  NavigationEarlyHintsManager& operator=(NavigationEarlyHintsManager&&) =
      delete;

  // Handles an Early Hints response. Can be called multiple times during a
  // navigation. When `early_hints` contains a preload Link header, starts
  // preloading it if preloading hasn't started for the same URL.
  void HandleEarlyHints(network::mojom::EarlyHintsPtr early_hints,
                        const network::ResourceRequest& request_for_navigation);

  // True when at least one preload or preconnect Link header was received via
  // Early Hints responses for main frame navigation.
  bool WasResourceHintsReceived() const;

  std::vector<GURL> TakePreloadedResourceURLs();

  // True when there are at least one inflight preloads.
  bool HasInflightPreloads() const;

  std::optional<base::TimeTicks> first_early_hints_receive_time() const {
    return first_early_hints_receive_time_;
  }

  void WaitForPreloadsFinishedForTesting(
      base::OnceCallback<void(PreloadedResources)> callback);

  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);

 private:
  FRIEND_TEST_ALL_PREFIXES(NavigationEarlyHintsManagerTest, PreloadPriority);
  class PreloadURLLoaderClient;

  struct PreconnectEntry;

  network::mojom::NetworkContext* GetNetworkContext();

  void MaybePreconnect(const network::mojom::LinkHeaderPtr& link);

  void MaybePreloadHintedResource(
      const network::mojom::LinkHeaderPtr& link,
      const network::ResourceRequest& request_for_navigation,
      const std::vector<network::mojom::ContentSecurityPolicyPtr>&
          content_security_policies,
      net::ReferrerPolicy referrer_policy);

  // Determines whether resource hints like preload and preconnect should be
  // handled or not.
  bool ShouldHandleResourceHints(const network::mojom::LinkHeaderPtr& link);

  void OnPreloadComplete(const GURL& url, const PreloadedResource& result);

  static net::RequestPriority CalculateRequestPriority(
      const network::mojom::LinkHeaderPtr& link);

  const raw_ref<BrowserContext> browser_context_;
  const raw_ref<StoragePartition> storage_partition_;
  const FrameTreeNodeId frame_tree_node_id_;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory_;
  // This needs to be declared last because it holds a pointer on
  // `loader_factory`, and thus needs to be destroyed before factory gets
  // destroyed.
  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory_;
  const url::Origin origin_;
  const net::IsolationInfo isolation_info_;

  base::flat_set<PreconnectEntry> preconnect_entries_;

  struct InflightPreload {
    InflightPreload(std::unique_ptr<blink::ThrottlingURLLoader> loader,
                    std::unique_ptr<PreloadURLLoaderClient> client);
    ~InflightPreload();
    InflightPreload(const InflightPreload&) = delete;
    InflightPreload& operator=(const InflightPreload&) = delete;
    InflightPreload(InflightPreload&&) = delete;
    InflightPreload& operator=(InflightPreload&&) = delete;

    // `loader` holds a raw_ptr on `client`, so it needs to be declared last to
    // avoid holding a dangling reference to `client` at destruction.
    std::unique_ptr<PreloadURLLoaderClient> client;
    std::unique_ptr<blink::ThrottlingURLLoader> loader;
  };
  // Using flat_map because the number of preloads are expected to be small.
  // Early Hints preloads should be requested for critical subresources such as
  // style sheets and fonts.
  base::flat_map<GURL, std::unique_ptr<InflightPreload>> inflight_preloads_;

  PreloadedResources preloaded_resources_;

  std::vector<GURL> preloaded_urls_;

  // Set to true when HandleEarlyHints() is called for the first time. Used to
  // ignore following responses.
  std::optional<base::TimeTicks> first_early_hints_receive_time_;
  // Set to true when preload or preconnect Link headers are received. Used for
  // metrics recording.
  bool was_resource_hints_received_ = false;

  base::OnceCallback<void(PreloadedResources)>
      preloads_completion_callback_for_testing_;

  raw_ptr<network::mojom::NetworkContext, DanglingUntriaged>
      network_context_for_testing_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_EARLY_HINTS_MANAGER_H_
