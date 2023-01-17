// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
enum class ReferrerPolicy;
}

namespace network {
class SimpleURLLoader;
}

namespace content {

class RenderFrameHostImpl;

// Manages network requests and maintains relevant state for interaction with
// the Identity Provider across a FedCM transaction. Owned by
// FederatedAuthRequestImpl and has a lifetime limited to a single identity
// transaction between an RP and an IDP.
//
// Diagram of the permission-based data flows between the browser and the IDP:
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//      |                                 |
//      |     GET /fedcm.json             |
//      |-------------------------------->|
//      |                                 |
//      |        JSON{idp_url}            |
//      |<--------------------------------|
//      |                                 |
//      | POST /idp_url with OIDC request |
//      |-------------------------------->|
//      |                                 |
//      |       token or signin_url       |
//      |<--------------------------------|
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//
// If the IDP returns an token, the sequence finishes. If it returns a
// signin_url, that URL is loaded as a rendered Document into a new window for
// the user to interact with the IDP.
class CONTENT_EXPORT IdpNetworkRequestManager {
 public:
  enum class ParseStatus {
    kSuccess,
    kHttpNotFoundError,
    kNoResponseError,
    kInvalidResponseError,
    // ParseStatus::kEmptyListError only applies to well known and account list
    // responses. It is used to classify a successful response where the list in
    // the response is empty.
    kEmptyListError,
  };
  struct FetchStatus {
    ParseStatus parse_status;
    // The HTTP response code, if one was received, otherwise the net error. It
    // is possible to distinguish which it is since HTTP response codes are
    // positive and net errors are negative.
    int response_code;
  };

  enum class LogoutResponse {
    kSuccess,
    kError,
  };

  struct CONTENT_EXPORT Endpoints {
    Endpoints();
    ~Endpoints();
    Endpoints(const Endpoints&);

    GURL token;
    GURL accounts;
    GURL client_metadata;
    GURL metrics;
  };

  struct ClientMetadata {
    GURL privacy_policy_url;
    GURL terms_of_service_url;
  };

  // Error codes sent to the metrics endpoint.
  // Enum is part of public FedCM API. Do not renumber error codes.
  // The error codes are not consecutive to make adding error codes easier in
  // the future.
  enum class MetricsEndpointErrorCode {
    kNone = 0,  // Success
    kOther = 1,
    // Errors triggered by how RP calls FedCM API.
    kRpFailure = 100,
    // User Failures.
    kUserFailure = 200,
    // Generic IDP Failures.
    kIdpServerInvalidResponse = 300,
    kIdpServerUnavailable = 301,
    kManifestError = 302,
    // Specific IDP Failures.
    kAccountsEndpointInvalidResponse = 401,
    kTokenEndpointInvalidResponse = 402,
  };

  using AccountList = std::vector<IdentityRequestAccount>;
  using AccountsRequestCallback =
      base::OnceCallback<void(FetchStatus, AccountList)>;
  using DownloadCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              int response_code)>;
  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, const std::set<GURL>&)>;
  using FetchConfigCallback = base::OnceCallback<
      void(FetchStatus, Endpoints, IdentityProviderMetadata)>;
  using FetchClientMetadataCallback =
      base::OnceCallback<void(FetchStatus, ClientMetadata)>;
  using LogoutCallback = base::OnceCallback<void()>;
  using ParseJsonCallback =
      base::OnceCallback<void(FetchStatus,
                              data_decoder::DataDecoder::ValueOrError)>;
  using TokenRequestCallback =
      base::OnceCallback<void(FetchStatus, const std::string&)>;

  static std::unique_ptr<IdpNetworkRequestManager> Create(
      RenderFrameHostImpl* host);

  IdpNetworkRequestManager(
      const url::Origin& relying_party,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      network::mojom::ClientSecurityStatePtr client_security_state);

  virtual ~IdpNetworkRequestManager();

  IdpNetworkRequestManager(const IdpNetworkRequestManager&) = delete;
  IdpNetworkRequestManager& operator=(const IdpNetworkRequestManager&) = delete;

  // Computes the well-known URL from the identity provider URL.
  static absl::optional<GURL> ComputeWellKnownUrl(const GURL& url);

  // Fetch the well-known file. This is the /.well-known/web-identity file on
  // the eTLD+1 calculated from the provider URL, used to check that the
  // provider URL is valid for this eTLD+1.
  virtual void FetchWellKnown(const GURL& provider, FetchWellKnownCallback);

  // Attempt to fetch the IDP's FedCM parameters from the config file.
  virtual void FetchConfig(const GURL& provider,
                           int idp_brand_icon_ideal_size,
                           int idp_brand_icon_minimum_size,
                           FetchConfigCallback);

  virtual void FetchClientMetadata(const GURL& endpoint,
                                   const std::string& client_id,
                                   FetchClientMetadataCallback);

  // Fetch accounts list for this user from the IDP.
  virtual void SendAccountsRequest(const GURL& accounts_url,
                                   const std::string& client_id,
                                   AccountsRequestCallback callback);

  // Request a new token for this user account and RP from the IDP.
  virtual void SendTokenRequest(const GURL& token_url,
                                const std::string& account,
                                const std::string& url_encoded_post_data,
                                TokenRequestCallback callback);

  // Sends metrics to metrics endpoint after a token was successfully generated.
  virtual void SendSuccessfulTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      base::TimeDelta api_call_to_show_dialog_time,
      base::TimeDelta show_dialog_to_continue_clicked_time,
      base::TimeDelta account_selected_to_token_response_time,
      base::TimeDelta api_call_to_token_response_time);

  // Sends error code to metrics endpoint when token generation fails.
  virtual void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      MetricsEndpointErrorCode error_code);

  // Send logout request to a single target.
  virtual void SendLogout(const GURL& logout_url, LogoutCallback);

 private:
  // Starts download request using `url_loader`. Calls `parse_json_callback`
  // when the download result has been parsed.
  void DownloadJsonAndParse(
      std::unique_ptr<network::ResourceRequest> resource_request,
      absl::optional<std::string> url_encoded_post_data,
      ParseJsonCallback parse_json_callback,
      size_t max_download_size);

  // Starts download result using `url_loader`. Calls `download_callback` when
  // the download completes.
  void DownloadUrl(std::unique_ptr<network::ResourceRequest> resource_request,
                   absl::optional<std::string> url_encoded_post_data,
                   DownloadCallback download_callback,
                   size_t max_download_size);

  // Called when download initiated by DownloadUrl() completes.
  void OnDownloadedUrl(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       DownloadCallback callback,
                       std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::ResourceRequest> CreateUncredentialedResourceRequest(
      const GURL& target_url,
      bool send_origin,
      bool follow_redirects = false) const;

  std::unique_ptr<network::ResourceRequest> CreateCredentialedResourceRequest(
      const GURL& target_url,
      bool send_origin) const;

  url::Origin relying_party_origin_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  network::mojom::ClientSecurityStatePtr client_security_state_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
