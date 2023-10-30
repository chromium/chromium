// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_revoke_request.h"

#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {
namespace {

base::TimeDelta GetRandomRejectionTime() {
  // TODO(crbug.com/1473134): add some reasonable delay in cases where it is
  // needed.
  return base::TimeDelta();
}

}  // namespace

using FederatedApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using LoginState = IdentityRequestAccount::LoginState;
using RevokeStatusForMetrics = content::FedCmRevokeStatus;
using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RevokeStatus;

// static
std::unique_ptr<FederatedAuthRevokeRequest> FederatedAuthRevokeRequest::Create(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityCredentialRevokeOptionsPtr options,
    bool should_complete_request_immediately) {
  std::unique_ptr<FederatedAuthRevokeRequest> request =
      base::WrapUnique<FederatedAuthRevokeRequest>(
          new FederatedAuthRevokeRequest(std::move(network_manager),
                                         permission_delegate, render_frame_host,
                                         metrics, std::move(options),
                                         should_complete_request_immediately));
  return request;
}

FederatedAuthRevokeRequest::~FederatedAuthRevokeRequest() {
  Complete(RevokeStatus::kError, FedCmRevokeStatus::kUnhandledRequest,
           /*should_delay_callback=*/false);
}

FederatedAuthRevokeRequest::FederatedAuthRevokeRequest(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityCredentialRevokeOptionsPtr options,
    bool should_complete_request_immediately)
    : network_manager_(std::move(network_manager)),
      permission_delegate_(permission_delegate),
      metrics_(metrics),
      render_frame_host_(render_frame_host),
      options_(std::move(options)),
      should_complete_request_immediately_(should_complete_request_immediately),
      origin_(render_frame_host->GetLastCommittedOrigin()) {
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  DCHECK(main_frame->IsInPrimaryMainFrame());
  embedding_origin_ = main_frame->GetLastCommittedOrigin();
}

void FederatedAuthRevokeRequest::SetCallbackAndStart(
    blink::mojom::FederatedAuthRequest::RevokeCallback callback,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  callback_ = std::move(callback);

  url::Origin config_origin = url::Origin::Create(options_->config->config_url);
  if (!network::IsOriginPotentiallyTrustworthy(config_origin)) {
    Complete(RevokeStatus::kError,
             RevokeStatusForMetrics::kIdpNotPotentiallyTrustworthy,
             /*should_delay_callback=*/false);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate->GetApiPermissionStatus(embedding_origin_);

  absl::optional<RevokeStatusForMetrics> error_revoke_status;
  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_revoke_status = RevokeStatusForMetrics::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED:
      // Use a generic error because by the time we ship we should not have this
      // type of status.
      error_revoke_status = RevokeStatusForMetrics::kDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      // TODO(crbug.com/1495108): determine if blocking is the right behavior.
      error_revoke_status = RevokeStatusForMetrics::kDisabledInSettings;
      break;
    // We do not block revocation on FedCM cooldown.
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
      break;
  }
  if (error_revoke_status) {
    Complete(RevokeStatus::kError, *error_revoke_status,
             /*should_delay_callback=*/true);
    return;
  }
  // Reject if we know that there are no sharing permissions with the given IdP.
  if (!permission_delegate_->HasSharingPermission(
          origin_, embedding_origin_, config_origin, absl::nullopt)) {
    Complete(RevokeStatus::kError, RevokeStatusForMetrics::kNoAccountToRevoke,
             /*should_delay_callback=*/true);
    return;
  }

  provider_fetcher_ = std::make_unique<FederatedProviderFetcher>(
      *render_frame_host_, network_manager_.get());
  GURL config_url = options_->config->config_url;
  provider_fetcher_->Start(
      {GURL(config_url)}, /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindOnce(
          &FederatedAuthRevokeRequest::OnAllConfigAndWellKnownFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRevokeRequest::OnAllConfigAndWellKnownFetched(
    std::vector<FederatedProviderFetcher::FetchResult> fetch_results) {
  provider_fetcher_.reset();
  DCHECK(fetch_results.size() == 1u);
  const FederatedProviderFetcher::FetchResult& fetch_result = fetch_results[0];
  if (fetch_result.error) {
    const FederatedProviderFetcher::FetchError& fetch_error =
        *fetch_result.error;
    if (fetch_error.additional_console_error_message) {
      render_frame_host_->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          *fetch_error.additional_console_error_message);
    }

    // TODO (crbug.com/1473134): add devtools issues and console errors.

    FedCmRevokeStatus status;
    switch (fetch_error.result) {
      case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound: {
        status = FedCmRevokeStatus::kWellKnownHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse: {
        status = FedCmRevokeStatus::kWellKnownNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse: {
        status = FedCmRevokeStatus::kWellKnownInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty: {
        status = FedCmRevokeStatus::kWellKnownListEmpty;
        break;
      }
      case FederatedAuthRequestResult::
          kErrorFetchingWellKnownInvalidContentType: {
        status = FedCmRevokeStatus::kWellKnownInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound: {
        status = FedCmRevokeStatus::kConfigHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse: {
        status = FedCmRevokeStatus::kConfigNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse: {
        status = FedCmRevokeStatus::kConfigInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType: {
        status = FedCmRevokeStatus::kConfigInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
        status = FedCmRevokeStatus::kWellKnownTooBig;
        break;
      }
      case FederatedAuthRequestResult::kErrorConfigNotInWellKnown: {
        status = FedCmRevokeStatus::kConfigNotInWellKnown;
        break;
      }
      default: {
        status = FedCmRevokeStatus::kUnhandledRequest;
        // The FederatedProviderFetcher does not return any other type of
        // result.
        CHECK(false);
        break;
      }
    }
    Complete(RevokeStatus::kError, status,
             /*should_delay_callback=*/false);
    return;
  }

  if (!webid::IsEndpointSameOrigin(options_->config->config_url,
                                   fetch_result.endpoints.revoke)) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Config is missing or has an invalid or cross-origin  "
        "\"revocation_endpoint\" URL.");
    Complete(RevokeStatus::kError,
             RevokeStatusForMetrics::kRevokeUrlIsCrossOrigin,
             /*should_delay_callback=*/false);
    return;
  }
  std::string account_hint = options_->account_hint;

  network_manager_->SendRevokeRequest(
      fetch_result.endpoints.revoke, account_hint, embedding_origin_, origin_,
      base::BindOnce(&FederatedAuthRevokeRequest::OnRevokeResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRevokeRequest::OnRevokeResponse(
    IdpNetworkRequestManager::RevokeResponse response) {
  // TODO(crbug.com/1473134): implement this method.
  CHECK(callback_);
  Complete(RevokeStatus::kSuccess, RevokeStatusForMetrics::kSuccess,
           /*should_delay_callback=*/false);
}

void FederatedAuthRevokeRequest::Complete(
    blink::mojom::RevokeStatus status,
    absl::optional<content::FedCmRevokeStatus> revoke_status_for_metrics,
    bool should_delay_callback) {
  if (!callback_) {
    return;
  }

  if (revoke_status_for_metrics) {
    metrics_->RecordRevokeStatus(*revoke_status_for_metrics);
  }

  if (!should_delay_callback || should_complete_request_immediately_) {
    std::move(callback_).Run(status);
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FederatedAuthRevokeRequest::Complete,
                     weak_ptr_factory_.GetWeakPtr(), status, absl::nullopt,
                     /*should_delay_callback=*/false),
      GetRandomRejectionTime());
}

}  // namespace content
