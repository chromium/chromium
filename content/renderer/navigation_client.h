// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NAVIGATION_CLIENT_H_
#define CONTENT_RENDERER_NAVIGATION_CLIENT_H_

#include "content/common/navigation_client.mojom.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {

class RenderFrameImpl;

class NavigationClient : mojom::NavigationClient {
 public:
  explicit NavigationClient(RenderFrameImpl* render_frame);
  ~NavigationClient() override;

  // mojom::NavigationClient implementation:
  void CommitNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
      absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
      mojom::CookieManagerInfoPtr cookie_manager_info,
      mojom::StorageInfoPtr storage_info,
      CommitNavigationCallback callback) override;
  void CommitFailedNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      int extended_error_code,
      const net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
      blink::mojom::PolicyContainerPtr policy_container,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      CommitFailedNavigationCallback callback) override;

  void Bind(mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver);

  // See NavigationState::was_initiated_in_this_frame for details.
  void MarkWasInitiatedInThisFrame();
  bool was_initiated_in_this_frame() const {
    return was_initiated_in_this_frame_;
  }

 private:
  // OnDroppedNavigation is bound from BeginNavigation till CommitNavigation.
  // During this period, it is called when the interface pipe is closed from the
  // browser side, leading to the ongoing navigation cancelation.
  void OnDroppedNavigation();
  void SetDisconnectionHandler();
  void ResetDisconnectionHandler();

  mojo::AssociatedReceiver<mojom::NavigationClient> navigation_client_receiver_{
      this};
  RenderFrameImpl* render_frame_;
  bool was_initiated_in_this_frame_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_NAVIGATION_CLIENT_H_
