// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NAVIGATION_CLIENT_H_
#define CONTENT_RENDERER_NAVIGATION_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_client.mojom.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"

namespace content {

class RenderFrameImpl;

class NavigationClient : mojom::NavigationClient {
 public:
  explicit NavigationClient(RenderFrameImpl* render_frame);
  NavigationClient(RenderFrameImpl* render_frame,
                   blink::mojom::BeginNavigationParamsPtr begin_params,
                   blink::mojom::CommonNavigationParamsPtr common_params);
  ~NavigationClient() override;

  // mojom::NavigationClient implementation:
  void CommitNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
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
      const blink::DocumentToken& document_token,
      const base::UnguessableToken& devtools_navigation_token,
      const base::Uuid& base_auction_nonce,
      const std::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
      mojo::PendingRemote<blink::mojom::CodeCacheHost>
          code_cache_host_for_background,
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
      const std::optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
      const blink::DocumentToken& document_token,
      blink::mojom::PolicyContainerPtr policy_container,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      CommitFailedNavigationCallback callback) override;

  void Bind(mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver);

  // See NavigationState::was_initiated_in_this_frame for details.
  bool was_initiated_in_this_frame() const {
    return was_initiated_in_this_frame_;
  }

  // Sets up a NavigationClient for a renderer-initiated navigation initiated
  // in this frame. This includes setting `was_initiated_in_this_frame_` to
  // true and setting up a task to send a navigation-cancellation-window-ended
  // notification to the browser.
  void SetUpRendererInitiatedNavigation(
      mojo::PendingRemote<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener_remote);

  void ResetWithoutCancelling();

  void ResetForNewNavigation(bool is_duplicate_navigation);

  void ResetForAbort();

  bool HasBeginNavigationParams() const { return !!begin_params_; }

  const blink::mojom::BeginNavigationParams& begin_params() const {
    return *begin_params_;
  }
  const blink::mojom::CommonNavigationParams& common_params() const {
    return *common_params_;
  }

 private:
  // OnDroppedNavigation is bound from BeginNavigation till CommitNavigation.
  // During this period, it is called when the interface pipe is closed from the
  // browser side, leading to the ongoing navigation cancellation.
  void OnDroppedNavigation();
  void SetDisconnectionHandler();
  void ResetDisconnectionHandler();

  // The window of time in which the renderer can cancel this navigation had
  // ended, so notify the browser about it.
  void NotifyNavigationCancellationWindowEnded();

  mojo::AssociatedReceiver<mojom::NavigationClient> navigation_client_receiver_{
      this};
  mojo::Remote<mojom::NavigationRendererCancellationListener>
      renderer_cancellation_listener_remote_;
  raw_ptr<RenderFrameImpl, DanglingUntriaged> render_frame_;
  // See NavigationState::was_initiated_in_this_frame for details.
  bool was_initiated_in_this_frame_ = false;

  // If the navigation is initiated by this renderer, this will be set to the
  // params sent on the
  blink::mojom::BeginNavigationParamsPtr begin_params_;
  blink::mojom::CommonNavigationParamsPtr common_params_;

  base::WeakPtrFactory<NavigationClient> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_NAVIGATION_CLIENT_H_
