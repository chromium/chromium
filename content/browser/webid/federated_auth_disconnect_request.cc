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

using FederatedApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using LoginState = IdentityRequestAccount::LoginState;
using DisconnectStatusForMetrics = FedCmDisconnectStatus;
using blink::mojom::DisconnectStatus;
using blink::mojom::FederatedAuthRequestResult;

// static
std::unique_ptr<FederatedAuthDisconnectRequest>
FederatedAuthDisconnectRequest::Create(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options) {
  std::unique_ptr<FederatedAuthDisconnectRequest> request =
      base::WrapUnique<FederatedAuthDisconnectRequest>(
          new FederatedAuthDisconnectRequest(
              std::move(network_manager), permission_delegate,
              render_frame_host, metrics, std::move(options)));
  return request;
}

FederatedAuthDisconnectRequest::~FederatedAuthDisconnectRequest() {
  Complete(DisconnectStatus::kError, FedCmDisconnectStatus::kUnhandledRequest);
}

FederatedAuthDisconnectRequest::FederatedAuthDisconnectRequest(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    FedCmMetrics* metrics,
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options)
    : network_manager_(std::move(network_manager)),
      permission_delegate_(permission_delegate),
      metrics_(metrics),
      render_frame_host_(render_frame_host),
      options_(std::move(options)),
      origin_(render_frame_host->GetLastCommittedOrigin()),
      start_time_(base::TimeTicks::Now()) {
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
             DisconnectStatusForMetrics::kIdpNotPotentiallyTrustworthy);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate->GetApiPermissionStatus(embedding_origin_);

  std::optional<DisconnectStatusForMetrics> error_disconnect_status;
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
  if (error_disconnect_status) {
    Complete(DisconnectStatus::kError, *error_disconnect_status);
    return;
  }
  // Reject if we know that there are no sharing permissions with the given IdP
  // and the IdP doesn't have third party cookies access on the RP site.
  if (!webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
          *render_frame_host_, options_->config->config_url, embedding_origin_,
          origin_, /*account_id=*/std::nullopt, permission_delegate_,
          api_permission_delegate)) {
    Complete(DisconnectStatus::kError,
             DisconnectStatusForMetrics::kNoAccountToDisconnect);
    return;
  }

  provider_fetcher_ = std::make_unique<FederatedProviderFetcher>(
      *render_frame_host_, network_manager_.get());
  GURL config_url = options_->config->config_url;
  provider_fetcher_->Start(
      {GURL(config_url)}, blink::mojom::RpMode::kPassive, /*icon_ideal_size=*/0,
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
      case FederatedAuthRequestResult::kWellKnownHttpNotFound: {
        status = FedCmDisconnectStatus::kWellKnownHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownNoResponse: {
        status = FedCmDisconnectStatus::kWellKnownNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownInvalidResponse: {
        status = FedCmDisconnectStatus::kWellKnownInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownListEmpty: {
        status = FedCmDisconnectStatus::kWellKnownListEmpty;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownInvalidContentType: {
        status = FedCmDisconnectStatus::kWellKnownInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kConfigHttpNotFound: {
        status = FedCmDisconnectStatus::kConfigHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kConfigNoResponse: {
        status = FedCmDisconnectStatus::kConfigNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kConfigInvalidResponse: {
        status = FedCmDisconnectStatus::kConfigInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kConfigInvalidContentType: {
        status = FedCmDisconnectStatus::kConfigInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownTooBig: {
        status = FedCmDisconnectStatus::kWellKnownTooBig;
        break;
      }
      case FederatedAuthRequestResult::kConfigNotInWellKnown: {
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
    Complete(DisconnectStatus::kError, status);
    return;
  }

  if (!webid::IsEndpointSameOrigin(options_->config->config_url,
                                   fetch_result.endpoints.disconnect)) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Config is missing or has an invalid or cross-origin  "
        "\"disconnect_endpoint\" URL.");
    Complete(DisconnectStatus::kError,
             DisconnectStatusForMetrics::kDisconnectUrlIsCrossOrigin);
    return;
  }
  disconnect_request_sent_ = true;
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
             DisconnectStatusForMetrics::kDisconnectFailedOnServer);
    return;
  }
  permission_delegate_->RevokeSharingPermission(origin_, embedding_origin_,
                                                idp_origin, account_id);
  Complete(DisconnectStatus::kSuccess, DisconnectStatusForMetrics::kSuccess);
}

void FederatedAuthDisconnectRequest::Complete(
    blink::mojom::DisconnectStatus status,
    FedCmDisconnectStatus disconnect_status_for_metrics) {
  if (!callback_) {
    return;
  }
  if (disconnect_status_for_metrics != FedCmDisconnectStatus::kSuccess) {
    AddConsoleErrorMessage(disconnect_status_for_metrics);
  }

  std::optional<base::TimeDelta> duration =
      disconnect_request_sent_
          ? std::optional<base::TimeDelta>{base::TimeTicks::Now() - start_time_}
          : std::nullopt;
  metrics_->RecordDisconnectMetrics(
      disconnect_status_for_metrics, duration, *render_frame_host_, origin_,
      embedding_origin_, options_->config->config_url,
      webid::GetNewSessionID());

  std::move(callback_).Run(status);
}

void FederatedAuthDisconnectRequest::AddConsoleErrorMessage(
    FedCmDisconnectStatus disconnect_status_for_metrics) {
  render_frame_host_->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      webid::GetDisconnectConsoleErrorMessage(disconnect_status_for_metrics));
}

}  // namespace content
