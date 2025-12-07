// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/disconnect_request.h"

#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/webid/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content::webid {

using FederatedApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using LoginState = IdentityRequestAccount::LoginState;
using blink::mojom::FederatedAuthRequestResult;

// static
std::unique_ptr<DisconnectRequest> DisconnectRequest::Create(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    std::unique_ptr<Metrics> fedcm_metrics,
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options) {
  std::unique_ptr<DisconnectRequest> request =
      base::WrapUnique<DisconnectRequest>(new DisconnectRequest(
          std::move(network_manager), permission_delegate, render_frame_host,
          std::move(fedcm_metrics), std::move(options)));
  return request;
}

DisconnectRequest::~DisconnectRequest() {
  Complete(blink::mojom::DisconnectStatus::kError,
           DisconnectStatus::kUnhandledRequest);
}

DisconnectRequest::DisconnectRequest(
    std::unique_ptr<IdpNetworkRequestManager> network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RenderFrameHost* render_frame_host,
    std::unique_ptr<Metrics> fedcm_metrics,
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options)
    : network_manager_(std::move(network_manager)),
      permission_delegate_(permission_delegate),
      render_frame_host_(render_frame_host),
      fedcm_metrics_(std::move(fedcm_metrics)),
      options_(std::move(options)),
      origin_(render_frame_host->GetLastCommittedOrigin()),
      start_time_(base::TimeTicks::Now()),
      perfetto_track_(CreatePerfettoTrackForFedCM(this)) {
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  DCHECK(main_frame->IsInPrimaryMainFrame());
  embedding_origin_ = main_frame->GetLastCommittedOrigin();
}

void DisconnectRequest::SetCallbackAndStart(
    blink::mojom::FederatedAuthRequest::DisconnectCallback callback,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate) {
  TRACE_EVENT_BEGIN("content.fedcm", "FedCM disconnect", perfetto_track_);

  callback_ = std::move(callback);

  url::Origin config_origin = url::Origin::Create(options_->config->config_url);
  if (!network::IsOriginPotentiallyTrustworthy(config_origin)) {
    Complete(blink::mojom::DisconnectStatus::kError,
             DisconnectStatus::kIdpNotPotentiallyTrustworthy);
    return;
  }

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate->GetApiPermissionStatus(embedding_origin_);

  std::optional<DisconnectStatus> error_disconnect_status;
  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_disconnect_status = DisconnectStatus::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      error_disconnect_status = DisconnectStatus::kDisabledInSettings;
      break;
    // We do not block disconnect on FedCM cooldown.
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
  }
  if (error_disconnect_status) {
    Complete(blink::mojom::DisconnectStatus::kError, *error_disconnect_status);
    return;
  }
  // Reject if we know that there are no sharing permissions with the given IdP
  // and the IdP doesn't have third party cookies access on the RP site.
  if (!HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
          *render_frame_host_, options_->config->config_url, embedding_origin_,
          origin_, /*account_id=*/std::nullopt, permission_delegate_,
          api_permission_delegate)) {
    Complete(blink::mojom::DisconnectStatus::kError,
             DisconnectStatus::kNoAccountToDisconnect);
    return;
  }

  config_fetcher_ = std::make_unique<ConfigFetcher>(*render_frame_host_,
                                                    network_manager_.get());
  GURL config_url = options_->config->config_url;
  // TODO(crbug.com/390626180): It seems ok to ignore the well-known checks in
  // all cases here. However, keeping this unchanged for now when the IDP
  // registration API is not enabled since we only really need this for that
  // case.
  config_fetcher_->Start(
      {{config_url, IsIdPRegistrationEnabled()}},
      blink::mojom::RpMode::kPassive, /*icon_ideal_size=*/0,
      /*icon_minimum_size=*/0,
      base::BindOnce(&DisconnectRequest::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisconnectRequest::OnAllConfigAndWellKnownFetched(
    std::vector<ConfigFetcher::FetchResult> fetch_results) {
  config_fetcher_.reset();
  DCHECK_EQ(fetch_results.size(), 1u);
  const ConfigFetcher::FetchResult& fetch_result = fetch_results[0];
  if (fetch_result.error) {
    const ConfigFetcher::FetchError& fetch_error = *fetch_result.error;
    if (fetch_error.additional_console_error_message) {
      render_frame_host_->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          *fetch_error.additional_console_error_message);
    }

    DisconnectStatus status;
    switch (fetch_error.result) {
      case FederatedAuthRequestResult::kWellKnownHttpNotFound: {
        status = DisconnectStatus::kWellKnownHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownNoResponse: {
        status = DisconnectStatus::kWellKnownNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownInvalidResponse: {
        status = DisconnectStatus::kWellKnownInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownListEmpty: {
        status = DisconnectStatus::kWellKnownListEmpty;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownInvalidContentType: {
        status = DisconnectStatus::kWellKnownInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kConfigHttpNotFound: {
        status = DisconnectStatus::kConfigHttpNotFound;
        break;
      }
      case FederatedAuthRequestResult::kConfigNoResponse: {
        status = DisconnectStatus::kConfigNoResponse;
        break;
      }
      case FederatedAuthRequestResult::kConfigInvalidResponse: {
        status = DisconnectStatus::kConfigInvalidResponse;
        break;
      }
      case FederatedAuthRequestResult::kConfigInvalidContentType: {
        status = DisconnectStatus::kConfigInvalidContentType;
        break;
      }
      case FederatedAuthRequestResult::kWellKnownTooBig: {
        status = DisconnectStatus::kWellKnownTooBig;
        break;
      }
      case FederatedAuthRequestResult::kConfigNotInWellKnown: {
        status = DisconnectStatus::kConfigNotInWellKnown;
        break;
      }
      default: {
        // The ConfigFetcher does not return any other type of
        // result.
        NOTREACHED();
      }
    }
    Complete(blink::mojom::DisconnectStatus::kError, status);
    return;
  }

  if (!IsEndpointSameOrigin(options_->config->config_url,
                            fetch_result.endpoints.disconnect)) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Config is missing or has an invalid or cross-origin  "
        "\"disconnect_endpoint\" URL.");
    Complete(blink::mojom::DisconnectStatus::kError,
             DisconnectStatus::kDisconnectUrlIsCrossOrigin);
    return;
  }
  disconnect_request_sent_ = true;
  network_manager_->SendDisconnectRequest(
      fetch_result.endpoints.disconnect, options_->account_hint,
      options_->config->client_id,
      base::BindOnce(&DisconnectRequest::OnDisconnectResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisconnectRequest::OnDisconnectResponse(FetchStatus fetch_status,
                                             const std::string& account_id) {
  CHECK(callback_);
  // Matches the GrantSharingPermission() call in
  // RequestService::CompleteTokenRequest(). Note that the IDP origin
  // cannot be an arbitrary origin, but rather needs to be a potentially
  // trustworthy one.
  url::Origin idp_origin = url::Origin::Create(options_->config->config_url);
  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    // Even though the response was unsuccessful, the credentialed fetch was
    // sent to the IDP, so disconnect all permissions associated with the triple
    // (`origin_`, `embedding_origin`, `idp_origin`).
    permission_delegate_->RevokeSharingPermission(
        origin_, embedding_origin_, idp_origin, /*account_id=*/"");
    Complete(blink::mojom::DisconnectStatus::kError,
             DisconnectStatus::kDisconnectFailedOnServer);
    return;
  }
  permission_delegate_->RevokeSharingPermission(origin_, embedding_origin_,
                                                idp_origin, account_id);
  Complete(blink::mojom::DisconnectStatus::kSuccess,
           DisconnectStatus::kSuccess);
}

void DisconnectRequest::Complete(
    blink::mojom::DisconnectStatus status,
    DisconnectStatus disconnect_status_for_metrics) {
  if (!callback_) {
    return;
  }

  TRACE_EVENT_END("content.fedcm", perfetto_track_);
  if (disconnect_status_for_metrics != DisconnectStatus::kSuccess) {
    AddConsoleErrorMessage(disconnect_status_for_metrics);
  }

  std::optional<base::TimeDelta> duration =
      disconnect_request_sent_
          ? std::optional<base::TimeDelta>{base::TimeTicks::Now() - start_time_}
          : std::nullopt;
  fedcm_metrics_->RecordDisconnectMetrics(
      disconnect_status_for_metrics, duration,
      ComputeRequesterFrameType(*render_frame_host_, origin_,
                                embedding_origin_),
      options_->config->config_url);
  fedcm_metrics_.reset();

  std::move(callback_).Run(status);
}

void DisconnectRequest::AddConsoleErrorMessage(
    DisconnectStatus disconnect_status_for_metrics) {
  render_frame_host_->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      GetDisconnectConsoleErrorMessage(disconnect_status_for_metrics));
}

}  // namespace content::webid
