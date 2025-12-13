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
#include "base/values.h"
#include "content/browser/webid/network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
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

namespace content {

using IdentityProviderDataPtr = scoped_refptr<IdentityProviderData>;
using IdentityRequestAccountPtr = scoped_refptr<IdentityRequestAccount>;
class FederatedIdentityPermissionContextDelegate;
class RenderFrameHostImpl;

namespace webid {

class IdentityProviderInfo;
enum class MetricsEndpointErrorCode;

// Manages network requests and maintains relevant state for interaction with
// the Identity Provider across a FedCM transaction. Owned by
// RequestService and has a lifetime limited to a single identity
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
class CONTENT_EXPORT IdpNetworkRequestManager : public NetworkRequestManager {
 public:
  enum class LogoutResponse {
    kSuccess,
    kError,
  };

  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  // LINT.IfChange(AccountsResponseInvalidReason)

  enum class AccountsResponseInvalidReason {
    kResponseIsNotJsonOrDict = 0,
    kNoAccountsKey = 1,
    kAccountListIsEmpty = 2,
    kAccountIsNotDict = 3,
    kAccountMissesRequiredField = 4,
    kAccountsShareSameId = 5,

    kMaxValue = kAccountsShareSameId
  };

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmAccountsResponseInvalidReason)

  struct CONTENT_EXPORT Endpoints {
    Endpoints();
    ~Endpoints();
    Endpoints(const Endpoints&);

    GURL token;
    GURL accounts;
    GURL client_metadata;
    GURL metrics;
    GURL disconnect;
    GURL issuance;
  };

  struct CONTENT_EXPORT WellKnown {
    WellKnown();
    ~WellKnown();
    WellKnown(const WellKnown&);
    std::set<GURL> provider_urls;
    GURL accounts;
    GURL login_url;
  };

  struct CONTENT_EXPORT ClientMetadata {
    ClientMetadata();
    ~ClientMetadata();
    ClientMetadata(const ClientMetadata&);

    GURL privacy_policy_url;
    GURL terms_of_service_url;
    GURL brand_icon_url;
    bool client_is_third_party_to_top_frame_origin{false};
  };

  struct CONTENT_EXPORT TokenResult {
    TokenResult();
    ~TokenResult();
    TokenResult(const TokenResult&) = delete;
    TokenResult& operator=(const TokenResult&) = delete;
    TokenResult(TokenResult&&);
    TokenResult& operator=(TokenResult&&) = default;

    std::optional<base::Value> token;
    std::optional<IdentityCredentialTokenError> error;
  };

  enum class DisconnectResponse {
    kSuccess,
    kError,
  };

  // This enum describes the type of error dialog shown.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(FedCmErrorDialogType)

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

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmErrorDialogType)

  // This enum describes the type of token response received.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(FedCmTokenResponseType)

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

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmTokenResponseType)

  // This enum describes the type of error URL compared to the IDP's config URL.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(FedCmErrorUrlType)

  enum class FedCmErrorUrlType {
    kSameOrigin = 0,
    kCrossOriginSameSite = 1,
    kCrossSite = 2,
    kMaxValue = kCrossSite
  };

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmErrorUrlType)

  using AccountsRequestCallback =
      base::OnceCallback<void(FetchStatus,
                              std::vector<IdentityRequestAccountPtr>)>;
  using FetchAccountPicturesAndBrandIconsCallback =
      base::OnceCallback<void(std::vector<IdentityRequestAccountPtr>,
                              std::unique_ptr<IdentityProviderInfo>,
                              const gfx::Image&)>;
  using FetchIdpBrandIconCallback =
      base::OnceCallback<void(std::unique_ptr<IdentityProviderInfo>)>;
  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, const WellKnown&)>;
  using FetchConfigCallback = base::OnceCallback<
      void(FetchStatus, Endpoints, IdentityProviderMetadata)>;
  using FetchClientMetadataCallback =
      base::OnceCallback<void(FetchStatus, ClientMetadata)>;
  using LogoutCallback = base::OnceCallback<void()>;
  using DisconnectCallback =
      base::OnceCallback<void(FetchStatus, const std::string&)>;
  using TokenRequestCallback =
      base::OnceCallback<void(FetchStatus, TokenResult&&)>;
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
      const url::Origin& rp_embedding_origin,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      network::mojom::ClientSecurityStatePtr client_security_state,
      content::FrameTreeNodeId frame_tree_node_id);

  ~IdpNetworkRequestManager() override;

  IdpNetworkRequestManager(const IdpNetworkRequestManager&) = delete;
  IdpNetworkRequestManager& operator=(const IdpNetworkRequestManager&) = delete;

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

  // Fetch accounts list for this user from the IDP. idp_origin is required
  // because `accounts_url` may be empty when lightweight fedcm is enabled or
  // when the IDP is registered. When lightweight fedcm is enabled, no actual
  // network request will be sent if there are unexpired stored accounts for
  // idp_origin. If there are no unexpired stored accounts and accounts_url is
  // empty, the callback will be invoked with an empty accounts list. Returns
  // whether a network request is sent to fetch accounts.
  virtual bool SendAccountsRequest(const url::Origin& idp_origin,
                                   const GURL& accounts_url,
                                   const std::string& client_id,
                                   AccountsRequestCallback callback);

  // Request a new token for this user account and RP from the IDP.
  virtual void SendTokenRequest(
      const GURL& token_url,
      const std::string& account,
      const std::string& url_encoded_post_data,
      bool idp_blindness,
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
      bool did_show_ui,
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

  void FetchAccountPicturesAndBrandIcons(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      std::unique_ptr<IdentityProviderInfo> idp_info,
      const GURL& rp_brand_icon_url,
      FetchAccountPicturesAndBrandIconsCallback callback);
  void FetchIdpBrandIcon(std::unique_ptr<IdentityProviderInfo> idp_info,
                         FetchIdpBrandIconCallback callback);

  // Download and decode an image. The request is made uncredentialed, using
  // `idp_origin` as the top-level-frame origin for the network isolation key,
  // and using the LOAD_ONLY_FROM_CACHE load flag; effectively, this will never
  // actually create network traffic and only retrieve the image from cache.
  virtual void DownloadAndDecodeCachedImage(const url::Origin& idp_origin,
                                            const GURL& url,
                                            ImageCallback callback);

  // Fetch account picture URLs that have been provided by accounts push;
  // this allows for retrieval from cache later when a credential request is
  // made later. The requests are made without credentials using `idp_origin`
  // as the top-level-frame origin.
  virtual void CacheAccountPictures(const url::Origin& idp_origin,
                                    const std::vector<GURL>& picture_urls);

 private:
  // NetworkRequestManager:
  net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() override;

  void FetchImage(const GURL& url, base::OnceClosure callback);
  void FetchCachedAccountImage(const url::Origin& idp_origin,
                               const GURL& url,
                               base::OnceClosure callback);
  void OnImageReceived(base::OnceClosure callback,
                       GURL url,
                       const gfx::Image& image);
  void OnAllAccountPicturesAndBrandIconUrlReceived(
      FetchAccountPicturesAndBrandIconsCallback callback,
      std::unique_ptr<IdentityProviderInfo> idp_info,
      std::vector<IdentityRequestAccountPtr>&& accounts,
      const GURL& rp_brand_icon_url);
  void OnIdpBrandIconReceived(std::unique_ptr<IdentityProviderInfo> idp_info,
                              FetchIdpBrandIconCallback callback);

  bool IsCrossSiteIframe() const;

  void OnDownloadedImage(ImageCallback callback,
                         std::optional<std::string> response_body,
                         int response_code,
                         const std::string& mime_type,
                         bool cors_error);

  void OnDecodedImage(ImageCallback callback, const SkBitmap& decoded_bitmap);

  std::unique_ptr<network::ResourceRequest> CreateCachedAccountPictureRequest(
      const url::Origin& idp_origin,
      const GURL& target_url,
      bool cache_only) const;

  url::Origin rp_embedding_origin_;

  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;

  // The downloaded image data.
  std::map<GURL, gfx::Image> downloaded_images_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
