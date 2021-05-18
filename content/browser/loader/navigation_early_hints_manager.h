// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_EARLY_HINTS_MANAGER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_EARLY_HINTS_MANAGER_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "url/gurl.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace network {
struct ResourceRequest;
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class BrowserContext;

// Handles 103 Early Hints responses for navigation. Responsible for preloads
// requested by Early Hints responses. Created when the first 103 response is
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
    absl::optional<int> error_code;
    // True when the preload was canceled. When true, the response was already
    // in the disk cache.
    bool was_canceled = false;
  };
  using PreloadedResources = base::flat_map<GURL, PreloadedResource>;

  NavigationEarlyHintsManager(
      BrowserContext& browser_context,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      int frame_tree_node_id);

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
                        const network::ResourceRequest& navigation_request);

  // True when at least one preload Link header was received via Early Hints
  // responses for main frame navigation.
  bool WasPreloadLinkHeaderReceived() const;

  // True when there are at least one inflight preloads.
  bool HasInflightPreloads() const;

  void WaitForPreloadsFinishedForTesting(
      base::OnceCallback<void(PreloadedResources)> callback);

 private:
  class PreloadURLLoaderClient;

  void MaybePreloadHintedResource(
      const network::mojom::LinkHeaderPtr& link,
      const network::ResourceRequest& navigation_request);

  void OnPreloadComplete(const GURL& url, const PreloadedResource& result);

  BrowserContext& browser_context_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  const int frame_tree_node_id_;

  struct InflightPreload {
    InflightPreload(std::unique_ptr<blink::ThrottlingURLLoader> loader,
                    std::unique_ptr<PreloadURLLoaderClient> client);
    ~InflightPreload();
    InflightPreload(const InflightPreload&) = delete;
    InflightPreload& operator=(const InflightPreload&) = delete;
    InflightPreload(InflightPreload&&) = delete;
    InflightPreload& operator=(InflightPreload&&) = delete;

    std::unique_ptr<blink::ThrottlingURLLoader> loader;
    std::unique_ptr<PreloadURLLoaderClient> client;
  };
  // Using flat_map because the number of preloads are expected to be small.
  // Early Hints preloads should be requested for critical subresources such as
  // style sheets and fonts.
  base::flat_map<GURL, std::unique_ptr<InflightPreload>> inflight_preloads_;

  PreloadedResources preloaded_resources_;

  bool was_preload_link_header_received_ = false;

  base::OnceCallback<void(PreloadedResources)>
      preloads_completion_callback_for_testing_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_EARLY_HINTS_MANAGER_H_
