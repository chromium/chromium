// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_

#include <memory>
#include <optional>
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
#include "url/gurl.h"
#include "url/origin.h"

namespace gfx {
class Image;
}

namespace net {
enum class ReferrerPolicy;
}

namespace network {
class SimpleURLLoader;
}

namespace content {

using IdentityProviderDataPtr = scoped_refptr<IdentityProviderData>;
using IdentityRequestAccountPtr = scoped_refptr<IdentityRequestAccount>;
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
//      |       token or login_url       |
//      |<--------------------------------|
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//
// If the IDP returns an token, the sequence finishes. If it returns a
// login_url, that URL is loaded as a rendered Document into a new window for
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
    kInvalidContentTypeError,
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

  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  enum class AccountsResponseInvalidReason {
    kResponseIsNotJsonOrDict,
    kNoAccountsKey,
    kAccountListIsEmpty,
    kAccountIsNotDict,
    kAccountMissesRequiredField,
    kAccountsShareSameId,

    kMaxValue = kAccountsShareSameId
  };

  struct CONTENT_EXPORT Endpoints {
    Endpoints();
    ~Endpoints();
    Endpoints(const Endpoints&);

    GURL token;
    GURL accounts;
    GURL client_metadata;
    GURL metrics;
    GURL disconnect;
  };

  struct CONTENT_EXPORT WellKnown {
    WellKnown();
    ~WellKnown();
    WellKnown(const WellKnown&);
    std::set<GURL> provider_urls;
    GURL accounts;
    GURL login_url;
  };

  struct ClientMetadata {
    GURL privacy_policy_url;
    GURL terms_of_service_url;
    GURL brand_icon_url;
  };

  struct CONTENT_EXPORT TokenResult {
    TokenResult();
    ~TokenResult();
    TokenResult(const TokenResult&);

    std::string token;
    std::optional<IdentityCredentialTokenError> error;
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

  enum class DisconnectResponse {
    kSuccess,
    kError,
  };

  // This enum describes the type of error dialog shown.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FedCmErrorDialogType {
    kGenericEmptyWithoutUrl = 0,
    kGenericEmptyWithUrl = 1,
    kGenericNonEmptyWithoutUrl = 2,
    kGenericNonEmptyWithUrl = 3,
    kInvalidRequestWithoutUrl = 4,
    kInvalidRequestWithUrl = 5,
    kUnauthorizedClientWithoutUrl = 6,
    kUnauthorizedClientWithUrl = 7,
    kAccessDeniedWithoutUrl = 8,
    kAccessDeniedWithUrl = 9,
    kTemporarilyUnavailableWithoutUrl = 10,
    kTemporarilyUnavailableWithUrl = 11,
    kServerErrorWithoutUrl = 12,
    kServerErrorWithUrl = 13,

    kMaxValue = kServerErrorWithUrl
  };

  // This enum describes the type of token response received.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FedCmTokenResponseType {
    kTokenReceivedAndErrorNotReceivedAndContinueOnNotReceived = 0,
    kTokenReceivedAndErrorReceivedAndContinueOnNotReceived = 1,
    kTokenNotReceivedAndErrorNotReceivedAndContinueOnNotReceived = 2,
    kTokenNotReceivedAndErrorReceivedAndContinueOnNotReceived = 3,
    kTokenReceivedAndErrorNotReceivedAndContinueOnReceived = 4,
    kTokenReceivedAndErrorReceivedAndContinueOnReceived = 5,
    kTokenNotReceivedAndErrorNotReceivedAndContinueOnReceived = 6,
    kTokenNotReceivedAndErrorReceivedAndContinueOnReceived = 7,

    kMaxValue = kTokenNotReceivedAndErrorReceivedAndContinueOnReceived
  };

  // This enum describes the type of error URL compared to the IDP's config URL.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FedCmErrorUrlType {
    kSameOrigin = 0,
    kCrossOriginSameSite = 1,
    kCrossSite = 2,

    kMaxValue = kCrossSite
  };

  using AccountsRequestCallback =
      base::OnceCallback<void(FetchStatus,
                              std::vector<IdentityRequestAccountPtr>)>;
  using DownloadCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              int response_code,
                              const std::string& mime_type)>;
  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, const WellKnown&)>;
  using FetchConfigCallback = base::OnceCallback<
      void(FetchStatus, Endpoints, IdentityProviderMetadata)>;
  using FetchClientMetadataCallback =
      base::OnceCallback<void(FetchStatus, ClientMetadata)>;
  using LogoutCallback = base::OnceCallback<void()>;
  using ParseJsonCallback =
      base::OnceCallback<void(FetchStatus,
                              data_decoder::DataDecoder::ValueOrError)>;
  using DisconnectCallback =
      base::OnceCallback<void(FetchStatus, const std::string&)>;
  using TokenRequestCallback =
      base::OnceCallback<void(FetchStatus, TokenResult)>;
  using ContinueOnCallback = base::OnceCallback<void(FetchStatus, const GURL&)>;
  using RecordErrorMetricsCallback =
      base::OnceCallback<void(FedCmTokenResponseType,
                              std::optional<FedCmErrorDialogType>,
                              std::optional<FedCmErrorUrlType>)>;
  using ImageCallback = base::OnceCallback<void(const gfx::Image&)>;

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
  static std::optional<GURL> ComputeWellKnownUrl(const GURL& url);

  // Fetch the well-known file. This is the /.well-known/web-identity file on
  // the eTLD+1 calculated from the provider URL, used to check that the
  // provider URL is valid for this eTLD+1.
  virtual void FetchWellKnown(const GURL& provider, FetchWellKnownCallback);

  // Attempt to fetch the IDP's FedCM parameters from the config file.
  virtual void FetchConfig(const GURL& provider,
                           blink::mojom::RpMode rp_mode,
                           int idp_brand_icon_ideal_size,
                           int idp_brand_icon_minimum_size,
                           FetchConfigCallback);

  virtual void FetchClientMetadata(const GURL& endpoint,
                                   const std::string& client_id,
                                   int rp_brand_icon_ideal_size,
                                   int rp_brand_icon_minimum_size,
                                   FetchClientMetadataCallback);

  // Fetch accounts list for this user from the IDP.
  virtual void SendAccountsRequest(const GURL& accounts_url,
                                   const std::string& client_id,
                                   AccountsRequestCallback callback);

  // Request a new token for this user account and RP from the IDP.
  virtual void SendTokenRequest(
      const GURL& token_url,
      const std::string& account,
      const std::string& url_encoded_post_data,
      TokenRequestCallback callback,
      ContinueOnCallback continue_on,
      RecordErrorMetricsCallback record_error_metrics_callback);

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

  // Send a disconnect request to the IDP.
  virtual void SendDisconnectRequest(const GURL& disconnect_url,
                                     const std::string& account_hint,
                                     const std::string& client_id,
                                     DisconnectCallback callback);

  // Download and decode an image. The request is made uncredentialed.
  virtual void DownloadAndDecodeImage(const GURL& url, ImageCallback callback);

 private:
  // Starts download request using `url_loader`. Calls `parse_json_callback`
  // when the download result has been parsed.
  void DownloadJsonAndParse(
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::optional<std::string> url_encoded_post_data,
      ParseJsonCallback parse_json_callback,
      size_t max_download_size,
      bool allow_http_error_results = false);

  // Starts download result using `url_loader`. Calls `download_callback` when
  // the download completes.
  void DownloadUrl(std::unique_ptr<network::ResourceRequest> resource_request,
                   std::optional<std::string> url_encoded_post_data,
                   DownloadCallback download_callback,
                   size_t max_download_size,
                   bool allow_http_error_results = false);

  // Called when download initiated by DownloadUrl() completes.
  void OnDownloadedUrl(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       DownloadCallback callback,
                       std::unique_ptr<std::string> response_body);

  void OnDownloadedImage(ImageCallback callback,
                         std::unique_ptr<std::string> response_body,
                         int response_code,
                         const std::string& mime_type);

  void OnDecodedImage(ImageCallback callback, const SkBitmap& decoded_bitmap);

  std::unique_ptr<network::ResourceRequest> CreateUncredentialedResourceRequest(
      const GURL& target_url,
      bool send_origin,
      bool follow_redirects = false) const;

  enum class CredentialedResourceRequestType {
    kNoOrigin,
    kOriginWithoutCORS,
    kOriginWithCORS
  };

  std::unique_ptr<network::ResourceRequest> CreateCredentialedResourceRequest(
      const GURL& target_url,
      CredentialedResourceRequestType type) const;

  url::Origin relying_party_origin_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  network::mojom::ClientSecurityStatePtr client_security_state_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
