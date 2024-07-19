// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_DELEGATED_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_TEST_DELEGATED_IDP_NETWORK_REQUEST_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

// Forwards IdpNetworkRequestManager calls to delegate. The purpose of this
// class is to enable querying the delegate after FederatedAuthRequestImpl
// destroys the DelegatedIdpNetworkRequestManager.
class DelegatedIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  explicit DelegatedIdpNetworkRequestManager(
      IdpNetworkRequestManager* delegate);
  ~DelegatedIdpNetworkRequestManager() override;

  DelegatedIdpNetworkRequestManager(const DelegatedIdpNetworkRequestManager&) =
      delete;
  DelegatedIdpNetworkRequestManager& operator=(
      const DelegatedIdpNetworkRequestManager&) = delete;

  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override;
  void FetchConfig(const GURL& provider,
                   blink::mojom::RpMode rp_mode,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override;
  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           int rp_brand_icon_ideal_size,
                           int rp_brand_icon_minimum_size,
                           FetchClientMetadataCallback callback) override;
  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override;
  void SendTokenRequest(
      const GURL& token_url,
      const std::string& account,
      const std::string& url_encoded_post_data,
      TokenRequestCallback callback,
      ContinueOnCallback continue_on_callback,
      RecordErrorMetricsCallback record_error_metrics_callback) override;
  void SendSuccessfulTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      base::TimeDelta api_call_to_show_dialog_time,
      base::TimeDelta show_dialog_to_continue_clicked_time,
      base::TimeDelta account_selected_to_token_response_time,
      base::TimeDelta api_call_to_token_response_time) override;
  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      MetricsEndpointErrorCode error_code) override;
  void SendLogout(const GURL& logout_url, LogoutCallback callback) override;
  void SendDisconnectRequest(const GURL& disconnect_url,
                             const std::string& account_hint,
                             const std::string& client_id,
                             DisconnectCallback callback) override;

  void DownloadAndDecodeImage(const GURL& url, ImageCallback callback) override;

 private:
  raw_ptr<IdpNetworkRequestManager, DanglingUntriaged> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_DELEGATED_IDP_NETWORK_REQUEST_MANAGER_H_
