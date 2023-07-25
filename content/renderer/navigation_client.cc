// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_client.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/common/frame.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/platform/task_type.h"

namespace content {

NavigationClient::NavigationClient(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

NavigationClient::~NavigationClient() {}

void NavigationClient::CommitNavigation(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
    absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory,
    const blink::DocumentToken& document_token,
    const base::UnguessableToken& devtools_navigation_token,
    const absl::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
    blink::mojom::PolicyContainerPtr policy_container,
    mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host,
    mojo::PendingRemote<blink::mojom::ResourceCache> resource_cache,
    mojom::CookieManagerInfoPtr cookie_manager_info,
    mojom::StorageInfoPtr storage_info,
    CommitNavigationCallback callback) {
  DCHECK(blink::IsRequestDestinationFrame(common_params->request_destination));

  // TODO(https://crbug.com/1467502): The reset should be done when the
  // navigation did commit (meaning at a later stage). This is not currently
  // possible because of race conditions leading to the early deletion of
  // NavigationRequest would unexpectedly abort the ongoing navigation. Remove
  // when the races are fixed.
  ResetDisconnectionHandler();
  render_frame_->CommitNavigation(
      std::move(common_params), std::move(commit_params),
      std::move(response_head), std::move(response_body),
      std::move(url_loader_client_endpoints), std::move(subresource_loaders),
      std::move(subresource_overrides),
      std::move(controller_service_worker_info), std::move(container_info),
      std::move(subresource_proxying_loader_factory),
      std::move(keep_alive_loader_factory), document_token,
      devtools_navigation_token, permissions_policy,
      std::move(policy_container), std::move(code_cache_host),
      std::move(resource_cache), std::move(cookie_manager_info),
      std::move(storage_info), std::move(callback));
}

void NavigationClient::CommitFailedNavigation(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    int extended_error_code,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
    const blink::DocumentToken& document_token,
    blink::mojom::PolicyContainerPtr policy_container,
    mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
    CommitFailedNavigationCallback callback) {
  ResetDisconnectionHandler();
  render_frame_->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      has_stale_copy_in_cache, error_code, extended_error_code,
      resolve_error_info, error_page_content, std::move(subresource_loaders),
      document_token, std::move(policy_container),
      std::move(alternative_error_page_info), std::move(callback));
}

void NavigationClient::Bind(
    mojo::PendingAssociatedReceiver<mojom::NavigationClient> receiver) {
  navigation_client_receiver_.Bind(
      std::move(receiver), render_frame_->GetTaskRunner(
                               blink::TaskType::kInternalNavigationAssociated));
  SetDisconnectionHandler();
}

void NavigationClient::SetUpRendererInitiatedNavigation(
    mojo::PendingRemote<mojom::NavigationRendererCancellationListener>
        renderer_cancellation_listener_remote) {
  DCHECK(!was_initiated_in_this_frame_);
  was_initiated_in_this_frame_ = true;
  renderer_cancellation_listener_remote_.Bind(
      std::move(renderer_cancellation_listener_remote),
      render_frame_->GetTaskRunner(
          blink::TaskType::kInternalNavigationCancellation));

  // Renderer-initiated navigations can be canceled from the JS task it was
  // initiated from. If we post a task here, the task will run after the JS task
  // that started the navigation had finished running. So, we can post a task to
  // notify the browser that navigation cancellation is no longer possible from
  // here.
  render_frame_->GetTaskRunner(blink::TaskType::kInternalNavigationCancellation)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &NavigationClient::NotifyNavigationCancellationWindowEnded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NavigationClient::ResetWithoutCancelling() {
  navigation_client_receiver_.ResetWithReason(
      mojom::NavigationClient::kResetForSwap, "");
}

void NavigationClient::NotifyNavigationCancellationWindowEnded() {
  DCHECK(was_initiated_in_this_frame_);
  renderer_cancellation_listener_remote_->RendererCancellationWindowEnded();
  renderer_cancellation_listener_remote_.reset();
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
