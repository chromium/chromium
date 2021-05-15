// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_client.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/task_type.h"

namespace content {

NavigationClient::NavigationClient(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

NavigationClient::~NavigationClient() {}

void NavigationClient::CommitNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
    absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token,
    blink::mojom::PolicyContainerPtr policy_container,
    CommitNavigationCallback callback) {
  // TODO(ahemery): The reset should be done when the navigation did commit
  // (meaning at a later stage). This is not currently possible because of
  // race conditions leading to the early deletion of NavigationRequest would
  // unexpectedly abort the ongoing navigation. Remove when the races are fixed.
  ResetDisconnectionHandler();
  render_frame_->CommitNavigation(
      std::move(common_params), std::move(commit_params),
      std::move(response_head), std::move(response_body),
      std::move(url_loader_client_endpoints), std::move(subresource_loaders),
      std::move(subresource_overrides),
      std::move(controller_service_worker_info), std::move(container_info),
      std::move(prefetch_loader_factory), devtools_navigation_token,
      std::move(policy_container), std::move(callback));
}

void NavigationClient::CommitFailedNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    int extended_error_code,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
    blink::mojom::PolicyContainerPtr policy_container,
    CommitFailedNavigationCallback callback) {
  ResetDisconnectionHandler();
  render_frame_->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      has_stale_copy_in_cache, error_code, extended_error_code,
      resolve_error_info, error_page_content, std::move(subresource_loaders),
      std::move(policy_container), std::move(callback));
}

void NavigationClient::Bind(
    mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver) {
  navigation_client_receiver_.Bind(
      std::move(receiver), render_frame_->GetTaskRunner(
                               blink::TaskType::kInternalNavigationAssociated));
  SetDisconnectionHandler();
}

void NavigationClient::MarkWasInitiatedInThisFrame() {
  DCHECK(!was_initiated_in_this_frame_);
  was_initiated_in_this_frame_ = true;
}

void NavigationClient::SetDisconnectionHandler() {
  navigation_client_receiver_.set_disconnect_handler(base::BindOnce(
      &NavigationClient::OnDroppedNavigation, base::Unretained(this)));
}

void NavigationClient::ResetDisconnectionHandler() {
  navigation_client_receiver_.set_disconnect_handler(base::DoNothing());
}

void NavigationClient::OnDroppedNavigation() {
  render_frame_->OnDroppedNavigation();
}

}  // namespace content
