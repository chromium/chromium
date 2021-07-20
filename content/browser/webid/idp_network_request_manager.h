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
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
}

namespace content {

class RenderFrameHost;

// Manages network requests and maintains relevant state for interaction with
// the Identity Provider across a WebID transaction. Owned by
// FederatedAuthRequestImpl and has a lifetime limited to a single identity
// transaction between an RP and an IDP.
//
// Diagram of the permission-based data flows between the browser and the IDP:
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//      |                                 |
//      |     GET /.well-known/webid      |
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
    kWebIdNotSupported,
    kFetchError,
    kInvalidResponseError,
  };

  enum class SigninResponse {
    kLoadIdp,
    kTokenGranted,
    kSigninError,
    kInvalidResponseError,
  };

  enum class AccountsResponse {
    kSuccess,
    kNetError,
    kInvalidResponseError,
  };

  enum class TokenResponse {
    kSuccess,
    kNetError,
    kInvalidRequestError,
    kInvalidResponseError,
  };

  enum class LogoutResponse {
    kSuccess,
    kError,
  };

  struct Endpoints {
    std::string idp;
    std::string token;
    std::string accounts;
  };

  static constexpr char kWellKnownFilePath[] = ".well-known/webid";

  using AccountList = std::vector<content::IdentityRequestAccount>;
  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, Endpoints)>;
  using SigninRequestCallback =
      base::OnceCallback<void(SigninResponse, const std::string&)>;
  using AccountsRequestCallback =
      base::OnceCallback<void(AccountsResponse, AccountList)>;
  using TokenRequestCallback =
      base::OnceCallback<void(TokenResponse, const std::string&)>;
  using LogoutCallback = base::OnceCallback<void(LogoutResponse)>;

  static std::unique_ptr<IdpNetworkRequestManager> Create(
      const GURL& provider,
      RenderFrameHost* host);

  IdpNetworkRequestManager(
      const GURL& provider,
      const url::Origin& relying_party,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

  virtual ~IdpNetworkRequestManager();

  IdpNetworkRequestManager(const IdpNetworkRequestManager&) = delete;
  IdpNetworkRequestManager& operator=(const IdpNetworkRequestManager&) = delete;

  // Attempt to fetch the IDP's WebID parameters from the its .well-known file.
  virtual void FetchIdpWellKnown(FetchWellKnownCallback);

  // Transmit the OAuth request to the IDP.
  virtual void SendSigninRequest(const GURL& signin_url,
                                 const std::string& request,
                                 SigninRequestCallback);

  // Fetch accounts list for this user from the IDP.
  virtual void SendAccountsRequest(const GURL& accounts_url,
                                   AccountsRequestCallback);

  // Request a new token for this user account and RP from the IDP.
  virtual void SendTokenRequest(const GURL& token_url,
                                const std::string& account,
                                const std::string& request,
                                TokenRequestCallback callback);

  // Send logout request to a single target.
  virtual void SendLogout(const GURL& logout_url, LogoutCallback);

 private:
  void OnWellKnownLoaded(std::unique_ptr<std::string> response_body);
  void OnWellKnownParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnSigninRequestResponse(std::unique_ptr<std::string> response_body);
  void OnSigninRequestParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnAccountsRequestResponse(std::unique_ptr<std::string> response_body);
  void OnAccountsRequestParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnTokenRequestResponse(std::unique_ptr<std::string> response_body);
  void OnTokenRequestParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnLogoutCompleted(std::unique_ptr<std::string> response_body);

  // URL of the Identity Provider.
  GURL provider_;

  url::Origin relying_party_origin_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  FetchWellKnownCallback idp_well_known_callback_;
  SigninRequestCallback signin_request_callback_;
  AccountsRequestCallback accounts_request_callback_;
  TokenRequestCallback token_request_callback_;
  LogoutCallback logout_callback_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
