// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/delegated_idp_network_request_manager.h"

namespace content {

DelegatedIdpNetworkRequestManager::DelegatedIdpNetworkRequestManager(
    IdpNetworkRequestManager* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DelegatedIdpNetworkRequestManager::~DelegatedIdpNetworkRequestManager() =
    default;

void DelegatedIdpNetworkRequestManager::FetchWellKnown(
    const GURL& provider,
    FetchWellKnownCallback callback) {
  delegate_->FetchWellKnown(provider, std::move(callback));
}

void DelegatedIdpNetworkRequestManager::FetchConfig(
    const GURL& provider,
    blink::mojom::RpMode rp_mode,
    int idp_brand_icon_ideal_size,
    int idp_brand_icon_minimum_size,
    FetchConfigCallback callback) {
  delegate_->FetchConfig(provider, rp_mode, idp_brand_icon_ideal_size,
                         idp_brand_icon_minimum_size, std::move(callback));
}

void DelegatedIdpNetworkRequestManager::FetchClientMetadata(
    const GURL& endpoint,
    const std::string& client_id,
    int rp_brand_icon_ideal_size,
    int rp_brand_icon_minimum_size,
    FetchClientMetadataCallback callback) {
  delegate_->FetchClientMetadata(endpoint, client_id, rp_brand_icon_ideal_size,
                                 rp_brand_icon_minimum_size,
                                 std::move(callback));
}

void DelegatedIdpNetworkRequestManager::SendAccountsRequest(
    const GURL& accounts_url,
    const std::string& client_id,
    AccountsRequestCallback callback) {
  delegate_->SendAccountsRequest(accounts_url, client_id, std::move(callback));
}

void DelegatedIdpNetworkRequestManager::SendTokenRequest(
    const GURL& token_url,
    const std::string& account,
    const std::string& url_encoded_post_data,
    TokenRequestCallback callback,
    ContinueOnCallback continue_on,
    RecordErrorMetricsCallback record_error_metrics_callback) {
  delegate_->SendTokenRequest(token_url, account, url_encoded_post_data,
                              std::move(callback), std::move(continue_on),
                              std::move(record_error_metrics_callback));
}

void DelegatedIdpNetworkRequestManager::SendSuccessfulTokenRequestMetrics(
    const GURL& metrics_endpoint_url,
    base::TimeDelta api_call_to_show_dialog_time,
    base::TimeDelta show_dialog_to_continue_clicked_time,
    base::TimeDelta account_selected_to_token_response_time,
    base::TimeDelta api_call_to_token_response_time) {
  delegate_->SendSuccessfulTokenRequestMetrics(
      metrics_endpoint_url, api_call_to_show_dialog_time,
      show_dialog_to_continue_clicked_time,
      account_selected_to_token_response_time, api_call_to_token_response_time);
}

void DelegatedIdpNetworkRequestManager::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint_url,
    MetricsEndpointErrorCode error_code) {
  delegate_->SendFailedTokenRequestMetrics(metrics_endpoint_url, error_code);
}

void DelegatedIdpNetworkRequestManager::SendLogout(const GURL& logout_url,
                                                   LogoutCallback callback) {
  delegate_->SendLogout(logout_url, std::move(callback));
}

void DelegatedIdpNetworkRequestManager::SendDisconnectRequest(
    const GURL& disconnect_url,
    const std::string& account_hint,
    const std::string& client_id,
    DisconnectCallback callback) {
  delegate_->SendDisconnectRequest(disconnect_url, account_hint, client_id,
                                   std::move(callback));
}

void DelegatedIdpNetworkRequestManager::DownloadAndDecodeImage(
    const GURL& url,
    ImageCallback callback) {
  delegate_->DownloadAndDecodeImage(url, std::move(callback));
}

}  // namespace content
