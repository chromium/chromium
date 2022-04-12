// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
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
//      |      id_token or signin_url     |
//      |<--------------------------------|
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//
// If the IDP returns an id_token, the sequence finishes. If it returns a
// signin_url, that URL is loaded as a rendered Document into a new window
// for the user to interact with the IDP.
class CONTENT_EXPORT IdpNetworkRequestManager {
 public:
  enum class FetchStatus {
    kSuccess,
    kHttpNotFoundError,
    kNoResponseError,
    kInvalidResponseError,
    kInvalidRequestError,
  };

  enum class LogoutResponse {
    kSuccess,
    kError,
  };

  enum class RevokeResponse {
    kSuccess,
    kError,
  };

  struct CONTENT_EXPORT Endpoints {
    Endpoints();
    ~Endpoints();
    Endpoints(const Endpoints&);

    std::string token;
    std::string accounts;
    std::string client_metadata;
    std::string revocation;
  };

  struct ClientMetadata {
    std::string privacy_policy_url;
    std::string terms_of_service_url;
  };

  static constexpr char kManifestFilePath[] = "fedcm.json";

  using AccountList = std::vector<content::IdentityRequestAccount>;
  using FetchManifestListCallback =
      base::OnceCallback<void(FetchStatus, const std::set<std::string>&)>;
  using FetchManifestCallback = base::OnceCallback<
      void(FetchStatus, Endpoints, IdentityProviderMetadata)>;
  using FetchClientMetadataCallback =
      base::OnceCallback<void(FetchStatus, ClientMetadata)>;
  using AccountsRequestCallback =
      base::OnceCallback<void(FetchStatus, AccountList)>;
  using TokenRequestCallback =
      base::OnceCallback<void(FetchStatus, const std::string&)>;
  using RevokeCallback = base::OnceCallback<void(RevokeResponse)>;
  using LogoutCallback = base::OnceCallback<void()>;

  static std::unique_ptr<IdpNetworkRequestManager> Create(
      const GURL& provider,
      RenderFrameHostImpl* host);

  IdpNetworkRequestManager(
      const GURL& provider,
      const url::Origin& relying_party,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      network::mojom::ClientSecurityStatePtr client_security_state);

  virtual ~IdpNetworkRequestManager();

  IdpNetworkRequestManager(const IdpNetworkRequestManager&) = delete;
  IdpNetworkRequestManager& operator=(const IdpNetworkRequestManager&) = delete;

  GURL ManifestUrl() const;

  // Fetch the manifest list. This is the /.well-known/fedcm.json file on
  // the eTLD+1 calculated from the provider URL, used to check that the
  // provider URL is valid for this eTLD+1.
  virtual void FetchManifestList(FetchManifestListCallback);

  // Attempt to fetch the IDP's FedCM parameters from the fedcm.json manifest.
  virtual void FetchManifest(absl::optional<int> idp_brand_icon_ideal_size,
                             absl::optional<int> idp_brand_icon_minimum_size,
                             FetchManifestCallback);

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
                                const std::string& request,
                                TokenRequestCallback callback);

  // Send a revoke token request to the IDP.
  virtual void SendRevokeRequest(const GURL& revoke_url,
                                 const std::string& client_id,
                                 const std::string& hint,
                                 RevokeCallback callback);

  // Send logout request to a single target.
  virtual void SendLogout(const GURL& logout_url, LogoutCallback);

  virtual bool IsMockIdpNetworkRequestManager() const;

 private:
  void OnManifestListLoaded(std::unique_ptr<std::string> response_body);
  void OnManifestListParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnManifestLoaded(absl::optional<int> idp_brand_icon_ideal_size,
                        absl::optional<int> idp_brand_icon_minimum_size,
                        std::unique_ptr<std::string> response_body);
  void OnManifestParsed(absl::optional<int> idp_brand_icon_ideal_size,
                        absl::optional<int> idp_brand_icon_minimum_size,
                        data_decoder::DataDecoder::ValueOrError result);
  void OnClientMetadataLoaded(std::unique_ptr<std::string> response_body);
  void OnClientMetadataParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnAccountsRequestResponse(AccountsRequestCallback callback,
                                 std::string client_id,
                                 std::unique_ptr<std::string> response_body);
  void OnAccountsRequestParsed(AccountsRequestCallback callback,
                               std::string client_id,
                               data_decoder::DataDecoder::ValueOrError result);
  void OnTokenRequestResponse(std::unique_ptr<std::string> response_body);
  void OnTokenRequestParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnRevokeResponse(std::unique_ptr<std::string> response_body);
  void OnLogoutCompleted(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> CreateUncredentialedUrlLoader(
      const GURL& url,
      bool send_referrer,
      bool follow_redirects = false) const;
  std::unique_ptr<network::SimpleURLLoader> CreateCredentialedUrlLoader(
      const GURL& url,
      bool send_referrer,
      absl::optional<std::string> request_body = absl::nullopt) const;

  // URL of the Identity Provider.
  GURL provider_;

  url::Origin relying_party_origin_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  FetchManifestListCallback manifest_list_callback_;
  FetchManifestCallback idp_manifest_callback_;
  FetchClientMetadataCallback client_metadata_callback_;
  TokenRequestCallback token_request_callback_;
  RevokeCallback revoke_callback_;
  LogoutCallback logout_callback_;

  // url_loader_ is used for all loads except for the manifest list, which uses
  // manifest_list_url_loader_. This is so we can fetch the manifest list in
  // parallel with the other requests.
  // TODO(cbiesinger): Also allow fetching the client metadata file in
  // parallel with the account list.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::unique_ptr<network::SimpleURLLoader> manifest_list_url_loader_;
  network::mojom::ClientSecurityStatePtr client_security_state_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
