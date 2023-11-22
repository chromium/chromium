// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_disconnect_request.h"

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
using DisconnectStatusForMetrics = content::FedCmDisconnectStatus;
using blink::mojom::DisconnectStatus;
using blink::mojom::FederatedAuthRequestResult;

// static
std::unique_ptr<FederatedAuthDisconnectRequest>
FederatedAuthDisconnectRequest::Create(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
    bool should_complete_request_immediately) {
  std::unique_ptr<FederatedAuthDisconnectRequest> request =
      base::WrapUnique<FederatedAuthDisconnectRequest>(
          new FederatedAuthDisconnectRequest(
              std::move(network_manager), permission_delegate,
              render_frame_host, metrics, std::move(options),
              should_complete_request_immediately));
  return request;
}

FederatedAuthDisconnectRequest::~FederatedAuthDisconnectRequest() {
  Complete(DisconnectStatus::kError, FedCmDisconnectStatus::kUnhandledRequest,
           /*should_delay_callback=*/false);
}

FederatedAuthDisconnectRequest::FederatedAuthDisconnectRequest(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
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

void FederatedAuthDisconnectRequest::SetCallbackAndStart(
    blink::mojom::FederatedAuthRequest::DisconnectCallback callback,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  callback_ = std::move(callback);

  url::Origin config_origin = url::Origin::Create(options_->config->config_url);
  if (!network::IsOriginPotentiallyTrustworthy(config_origin)) {
    Complete(DisconnectStatus::kError,
             DisconnectStatusForMetrics::kIdpNotPotentiallyTrustworthy,
             /*should_delay_callback=*/false);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate->GetApiPermissionStatus(embedding_origin_);

  absl::optional<DisconnectStatusForMetrics> error_disconnect_status;
  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_disconnect_status = DisconnectStatusForMetrics::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      error_disconnect_status = DisconnectStatusForMetrics::kDisabledInSettings;
      break;
    // We do not block disconnect on FedCM cooldown.
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
      break;
  }
  if (error_disconnect_status) {
    Complete(DisconnectStatus::kError, *error_disconnect_status,
             /*should_delay_callback=*/true);
    return;
  }
  // Reject if we know that there are no sharing permissions with the given IdP
  // and the IdP doesn't have third party cookies access on the RP site.
  if (!webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
          *render_frame_host_, options_->config->config_url, embedding_origin_,
          origin_, /*account_id=*/absl::nullopt, permission_delegate_,
          api_permission_delegate)) {
    Complete(DisconnectStatus::kError,
             DisconnectStatusForMetrics::kNoAccountToDisconnect,
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
          &FederatedAuthDisconnectRequest::OnAllConfigAndWellKnownFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthDisconnectRequest::OnAllConfigAndWellKnownFetched(
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

    FedCmDisconnectStatus status;
    switch (fetch_error.result) {
      case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound: {
        status = FedCmDisconnectStatus::kWellKnownHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse: {
        status = FedCmDisconnectStatus::kWellKnownNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse: {
        status = FedCmDisconnectStatus::kWellKnownInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty: {
        status = FedCmDisconnectStatus::kWellKnownListEmpty;
        break;
      }
      case FederatedAuthRequestResult::
          kErrorFetchingWellKnownInvalidContentType: {
        status = FedCmDisconnectStatus::kWellKnownInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound: {
        status = FedCmDisconnectStatus::kConfigHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse: {
        status = FedCmDisconnectStatus::kConfigNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse: {
        status = FedCmDisconnectStatus::kConfigInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType: {
        status = FedCmDisconnectStatus::kConfigInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
        status = FedCmDisconnectStatus::kWellKnownTooBig;
        break;
      }
      case FederatedAuthRequestResult::kErrorConfigNotInWellKnown: {
        status = FedCmDisconnectStatus::kConfigNotInWellKnown;
        break;
      }
      default: {
        status = FedCmDisconnectStatus::kUnhandledRequest;
        // The FederatedProviderFetcher does not return any other type of
        // result.
        CHECK(false);
        break;
      }
    }
    Complete(DisconnectStatus::kError, status,
             /*should_delay_callback=*/false);
    return;
  }

  if (!webid::IsEndpointSameOrigin(options_->config->config_url,
                                   fetch_result.endpoints.disconnect)) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Config is missing or has an invalid or cross-origin  "
        "\"disconnect_endpoint\" URL.");
    Complete(DisconnectStatus::kError,
             DisconnectStatusForMetrics::kDisconnectUrlIsCrossOrigin,
             /*should_delay_callback=*/false);
    return;
  }
  network_manager_->SendDisconnectRequest(
      fetch_result.endpoints.disconnect, options_->account_hint,
      options_->config->client_id,
      base::BindOnce(&FederatedAuthDisconnectRequest::OnDisconnectResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthDisconnectRequest::OnDisconnectResponse(
    IdpNetworkRequestManager::FetchStatus fetch_status,
    const std::string& account_id) {
  CHECK(callback_);
  // Matches the GrantSharingPermission() call in
  // FederatedAuthRequestImpl::CompleteTokenRequest(). Note that the IDP origin
  // cannot be an arbitrary origin, but rather needs to be a potentially
  // trustworthy one.
  url::Origin idp_origin = url::Origin::Create(options_->config->config_url);
  if (fetch_status.parse_status !=
      IdpNetworkRequestManager::ParseStatus::kSuccess) {
    // Even though the response was unsuccessful, the credentialed fetch was
    // sent to the IDP, so disconnect all permissions associated with the triple
    // (`origin_`, `embedding_origin`, `idp_origin`).
    permission_delegate_->RevokeSharingPermission(
        origin_, embedding_origin_, idp_origin, /*account_id=*/"");
    Complete(DisconnectStatus::kError,
             DisconnectStatusForMetrics::kDisconnectFailedOnServer,
             /*should_delay_callback=*/false);
    return;
  }
  permission_delegate_->RevokeSharingPermission(origin_, embedding_origin_,
                                                idp_origin, account_id);
  Complete(DisconnectStatus::kSuccess, DisconnectStatusForMetrics::kSuccess,
           /*should_delay_callback=*/false);
}

void FederatedAuthDisconnectRequest::Complete(
    blink::mojom::DisconnectStatus status,
    absl::optional<content::FedCmDisconnectStatus>
        disconnect_status_for_metrics,
    bool should_delay_callback) {
  if (!callback_) {
    return;
  }

  if (disconnect_status_for_metrics) {
    metrics_->RecordDisconnectStatus(*disconnect_status_for_metrics);
  }

  if (!should_delay_callback || should_complete_request_immediately_) {
    std::move(callback_).Run(status);
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FederatedAuthDisconnectRequest::Complete,
                     weak_ptr_factory_.GetWeakPtr(), status, absl::nullopt,
                     /*should_delay_callback=*/false),
      GetRandomRejectionTime());
}

}  // namespace content
