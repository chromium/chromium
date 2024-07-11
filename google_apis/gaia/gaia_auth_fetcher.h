// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

// Authenticate a user against the Google Accounts APIs with various
// capabilities and return results to a GaiaAuthConsumer.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time on any thread. To parallelize
// requests, create multiple GaiaAuthFetchers.

class GaiaAuthFetcherTest;

namespace gaia {

// Mode determining whether Gaia can change the order of the accounts in
// cookies.
enum class MultiloginMode {
  MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER = 0,
  MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER
};

// Specifies the "source" parameter for Gaia calls.
class COMPONENT_EXPORT(GOOGLE_APIS) GaiaSource {
 public:
  enum Type {
    kChrome,
    kChromeOS,
    kAccountReconcilorDice,
    kAccountReconcilorMirror,
    kPrimaryAccountManager
  };

  // Implicit conversion is necessary to avoid boilerplate code.
  GaiaSource(Type type);
  GaiaSource(Type source, const std::string& suffix);
  void SetGaiaSourceSuffix(const std::string& suffix);
  std::string ToString();

 private:
  Type type_;
  std::string suffix_;
};

}  // namespace gaia

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class COMPONENT_EXPORT(GOOGLE_APIS) GaiaAuthFetcher {
 public:
  struct COMPONENT_EXPORT(GOOGLE_APIS) MultiloginTokenIDPair {
    std::string token_;
    std::string gaia_id_;

    MultiloginTokenIDPair(const std::string& gaia_id,
                          const std::string& token) {
      gaia_id_ = gaia_id;
      token_ = token;
    }
  };

  // This will later be hidden behind an auth service which caches tokens.
  GaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  GaiaAuthFetcher(const GaiaAuthFetcher&) = delete;
  GaiaAuthFetcher& operator=(const GaiaAuthFetcher&) = delete;

  virtual ~GaiaAuthFetcher();

  // Start a request to revoke |auth_token|.
  //
  // OnOAuth2RevokeTokenCompleted will be called on the consumer on the original
  // thread.
  void StartRevokeOAuth2Token(const std::string& auth_token);

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  // If `binding_registration_token` is not empty, also registers binding key
  // information to create a bound refresh token. This doesn't guarantee that
  // the server actually binds the refresh token to a key.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchange(
      const std::string& auth_code,
      const std::string& user_agent_full_version_list = std::string(),
      const std::string& binding_registration_token = std::string());

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  // Resulting refresh token is annotated on the server with `device_id`. Format
  // of device_id on the server is at most 64 unicode characters.
  // If `binding_registration_token` is not empty, also registers binding key
  // information to create a bound refresh token. This doesn't guarantee that
  // the server actually binds the refresh token to a key.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      const std::string& auth_code,
      const std::string& device_id,
      const std::string& user_agent_full_version_list = std::string(),
      const std::string& binding_registration_token = std::string());

  // Starts a request to get the cookie for list of accounts.
  void StartOAuthMultilogin(gaia::MultiloginMode mode,
                            const std::vector<MultiloginTokenIDPair>& accounts,
                            const std::string& external_cc_result);

  // Starts a request to list the accounts in the GAIA cookie.
  void StartListAccounts();

  // Starts a request to log out the accounts in the GAIA cookie.
  // Note: this only clears the Gaia cookies. Other cookies such as the SAML
  // provider cookies are not cleared. To cleanly remove an account from the
  // web, the Gaia logout page should be loaded as a navigation.
  void StartLogOut();

  // Given a child account's OAuth2 refresh token, the parent account's
  // obfuscated GAIA ID, and their |credential| create the reauth proof token
  // for the parent.
  //
  // |max_retries| specifies the maximum number of times we should retry on a
  // network error.  This could help to fetch the token in the case of a flaky
  // network connection. This does not apply in the case of an ReAuth error
  // (i.e. there was something wrong with the ReAuth input arguments).  Setting
  // |max_retries| to -1 implies infinite retries.
  //
  // Virtual so it can be overridden by fake implementations.
  virtual void StartCreateReAuthProofTokenForParent(
      const std::string& child_oauth_access_token,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& parent_credential);

  // Starts a request to get the list of URLs to check for connection info.
  // Returns token/URL pairs to check, and the resulting status can be given to
  // OAuth multilogin requests.
  void StartGetCheckConnectionInfo();

  // `CreateAndStartGaiaFetcher()` been called && results not back yet?
  bool HasPendingFetch();

 protected:
  // Creates and starts |url_loader_|, used to make all Gaia request.  |body| is
  // used as the body of the POST request sent to GAIA. |body_content_type| is
  // the body content type to set, but only used if |body| is set.  Any strings
  // listed in |headers| are added as extra HTTP headers in the request.
  //
  // |credentials_mode| are passed to directly to
  // network::SimpleURLLoader::Create() when creating the SimpleURLLoader.
  //
  // HasPendingFetch() should return false before calling this method, and will
  // return true afterwards.
  virtual void CreateAndStartGaiaFetcher(
      const std::string& body,
      const std::string& body_content_type,
      const std::string& headers,
      const GURL& gaia_gurl,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Called by OnURLLoadComplete, exposed for ease of testing.
  void OnURLLoadCompleteInternal(net::Error net_error,
                                 int response_code,
                                 std::string response_body);

  // Dispatch the results of a request.
  void DispatchFetchedRequest(const GURL& url,
                              const std::string& data,
                              net::Error net_error,
                              int response_code);

  void SetPendingFetch(bool pending_fetch);

  // Needed to use XmlHTTPRequest for Multilogin requeston iOS even after
  // iOS11 because WKWebView cannot read response body if content-disposition
  // header is set.
  // TODO(crbug.com/40595504) Remove this once requests are done using
  // NSUrlSession in iOS.
  bool IsMultiloginUrl(const GURL& url);

  bool IsReAuthApiUrl(const GURL& url);

  bool IsListAccountsUrl(const GURL& url);

 private:
  // The format of the POST body to get OAuth2 token pair from auth code.
  static const char kOAuth2CodeToTokenPairBodyFormat[];
  // Additional params for the POST body to get OAuth2 token pair from auth
  // code.
  static const char kOAuth2CodeToTokenPairDeviceIdParam[];
  static const char kOAuth2CodeToTokenPairBindingRegistrationTokenParam[];
  // The format of the POST body to revoke an OAuth2 token.
  static const char kOAuth2RevokeTokenBodyFormat[];

  // Constants for parsing error responses.
  static const char kErrorParam[];
  static const char kErrorUrlParam[];

  // Constants for request/response for OAuth2 requests.
  static const char kOAuthHeaderFormat[];
  static const char kOAuthMultiBearerHeaderFormat[];

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  void OnOAuth2TokenPairFetched(const std::string& data,
                                net::Error net_error,
                                int response_code);

  void OnOAuth2RevokeTokenFetched(const std::string& data,
                                  net::Error net_error,
                                  int response_code);

  void OnListAccountsFetched(const std::string& data,
                             net::Error net_error,
                             int response_code);

  void OnLogOutFetched(const std::string& data,
                       net::Error net_error,
                       int response_code);

  void OnOAuthMultiloginFetched(const std::string& data,
                                net::Error net_error,
                                int response_code);

  void OnGetCheckConnectionInfoFetched(const std::string& data,
                                       net::Error net_error,
                                       int response_code);

  void OnReAuthApiInfoFetched(const std::string& data,
                              net::Error net_error,
                              int response_code);

  static void ParseFailureResponse(const std::string& data,
                                   std::string* error,
                                   std::string* error_url);

  // Given auth code, device ID (optional), and registration token for token
  // binding (optional) create body to get OAuth2 token pair.
  static std::string MakeGetTokenPairBody(
      const std::string& auth_code,
      const std::string& device_id,
      const std::string& binding_registration_token);
  // Given an OAuth2 token, create body to revoke the token.
  std::string MakeRevokeTokenBody(const std::string& auth_token);

  // From a SimpleURLLoader result, generates an appropriate error.
  static GoogleServiceAuthError GenerateAuthError(const std::string& data,
                                                  net::Error net_error);

  // These fields are common to GaiaAuthFetcher, same every request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<GaiaAuthConsumer> consumer_;
  std::string source_;
  const GURL oauth2_token_gurl_;
  const GURL oauth2_revoke_gurl_;
  const GURL oauth_multilogin_gurl_;
  const GURL list_accounts_gurl_;
  const GURL logout_gurl_;
  const GURL get_check_connection_info_url_;
  const GURL reauth_api_url_;

  // While a fetch is going on:
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  GURL original_url_;
  std::string request_body_;

  bool fetch_pending_ = false;

  friend class GaiaAuthFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CaptchaParse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, BadAuthenticationError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, BadAuthenticationShortError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, IncomprehensibleError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ServiceUnavailableError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ServiceUnavailableShortError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckTwoFactorResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, LoginNetFailure);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ParseOAuth2TokenPairResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthWithQuote);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeQuote);
};

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
