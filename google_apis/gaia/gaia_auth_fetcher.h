// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
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

namespace signin {
// Mode determining whether Gaia can change the order of the accounts in
// cookies.
enum class MultiloginMode {
  MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER = 0,
  MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER
};
}  // namespace signin

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class GaiaAuthFetcher {
 public:
  struct MultiloginTokenIDPair {
    std::string token_;
    std::string gaia_id_;

    MultiloginTokenIDPair(const std::string& gaia_id,
                          const std::string& token) {
      gaia_id_ = gaia_id;
      token_ = token;
    }
  };

  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // Magic string indicating that though the user does not have Less Secure
  // Apps enabled, the user provided the right password.
  static const char kWebLoginRequired[];

  // This will later be hidden behind an auth service which caches tokens.
  GaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~GaiaAuthFetcher();

  // Start a request to revoke |auth_token|.
  //
  // OnOAuth2RevokeTokenCompleted will be called on the consumer on the original
  // thread.
  void StartRevokeOAuth2Token(const std::string& auth_token);

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchange(const std::string& auth_code);

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  // Resulting refresh token is annotated on the server with |device_id|. Format
  // of device_id on the server is at most 64 unicode characters.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      const std::string& auth_code,
      const std::string& device_id);

  // Start a request to get user info for the account identified by |lsid|.
  //
  // Either OnGetUserInfoSuccess or OnGetUserInfoFailure will be
  // called on the consumer on the original thread.
  void StartGetUserInfo(const std::string& lsid);

  // Start a MergeSession request to pre-login the user with the given
  // credentials.
  //
  // Start a MergeSession request to fill the browsing cookie jar with
  // credentials represented by the account whose uber-auth token is
  // |uber_token|.  This method will modify the cookies of the current profile.
  //
  // The |external_cc_result| string can specify the result of connetion checks
  // for various google properties, and MergeSession will set cookies on those
  // properties too if appropriate.  See StartGetCheckConnectionInfo() for
  // details.  The string is a comma separated list of token/result pairs, where
  // token and result are separated by a colon.  This string may be empty, in
  // which case no specific handling is performed.
  //
  // Either OnMergeSessionSuccess or OnMergeSessionFailure will be
  // called on the consumer on the original thread.
  void StartMergeSession(const std::string& uber_token,
                         const std::string& external_cc_result);

  // Start a request to exchange an OAuthLogin-scoped oauth2 access token for an
  // uber-auth token.  The returned token can be used with the method
  // StartMergeSession().
  // If |is_bound_to_channel_id| is true, then the generated UberToken will
  // be bound to the channel ID of the network context of |getter_|.
  //
  // Either OnUberAuthTokenSuccess or OnUberAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartTokenFetchForUberAuthExchange(const std::string& access_token,
                                          bool is_bound_to_channel_id);

  // Start a request to exchange an OAuthLogin-scoped oauth2 access token for a
  // ClientLogin-style service tokens.  The response to this request is the
  // same as the response to a ClientLogin request, except that captcha
  // challenges are never issued.
  //
  // Either OnClientLoginSuccess or OnClientLoginFailure will be
  // called on the consumer on the original thread. If |service| is empty,
  // the call will attempt to fetch uber auth token.
  void StartOAuthLogin(const std::string& access_token,
                       const std::string& service);

  // Starts a request to get the cookie for list of accounts.
  void StartOAuthMultilogin(const std::vector<MultiloginTokenIDPair>& accounts);

  // Starts a request to list the accounts in the GAIA cookie.
  void StartListAccounts();

  // Starts a request to log out the accounts in the GAIA cookie.
  void StartLogOut();

  // Starts a request to get the list of URLs to check for connection info.
  // Returns token/URL pairs to check, and the resulting status can be given to
  // /MergeSession requests.
  void StartGetCheckConnectionInfo();

  // StartClientLogin been called && results not back yet?
  bool HasPendingFetch();

  // Stop any URL fetches in progress.
  virtual void CancelRequest();

  // From the data loaded from a SimpleURLLoader, generates an appropriate
  // error. From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateOAuthLoginError(const std::string& data,
                                                        net::Error net_error);

 protected:
  // Creates and starts |url_loader_|, used to make all Gaia request.  |body| is
  // used as the body of the POST request sent to GAIA.  Any strings listed in
  // |headers| are added as extra HTTP headers in the request.
  //
  // |load_flags| are passed to directly to network::SimpleURLLoader::Create()
  // when creating the SimpleURLLoader.
  //
  // HasPendingFetch() should return false before calling this method, and will
  // return true afterwards.
  virtual void CreateAndStartGaiaFetcher(
      const std::string& body,
      const std::string& headers,
      const GURL& gaia_gurl,
      int load_flags,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Called by OnURLLoadComplete, exposed for ease of testing.
  virtual void OnURLLoadCompleteInternal(
      net::Error net_error,
      int response_code,
      const network::HttpRawRequestResponseInfo::HeadersVector& headers,
      std::string response_body);

  // Dispatch the results of a request.
  void DispatchFetchedRequest(const GURL& url,
                              const std::string& data,
                              const net::ResponseCookies& cookies,
                              net::Error net_error,
                              int response_code);

  void SetPendingFetch(bool pending_fetch);

  // Needed to use XmlHTTPRequest for Multilogin requeston iOS even after
  // iOS11 because WKWebView cannot read response body if content-disposition
  // header is set.
  // TODO(https://crbug.com/889471) Remove this once requests are done using
  // NSUrlSession in iOS.
  bool IsMultiloginUrl(const GURL& url);

 private:
  // The format of the POST body for IssueAuthToken.
  static const char kIssueAuthTokenFormat[];
  // The format of the POST body to get OAuth2 token pair from auth code.
  static const char kOAuth2CodeToTokenPairBodyFormat[];
  // Additional param for the POST body to get OAuth2 token pair from auth code.
  static const char kOAuth2CodeToTokenPairDeviceIdParam[];
  // The format of the POST body to revoke an OAuth2 token.
  static const char kOAuth2RevokeTokenBodyFormat[];
  // The format of the POST body for GetUserInfo.
  static const char kGetUserInfoFormat[];
  // The format of the POST body for MergeSession.
  static const char kMergeSessionFormat[];
  // The format of the URL for UberAuthToken.
  static const char kUberAuthTokenURLFormat[];
  // The format of the body for OAuthLogin.
  static const char kOAuthLoginFormat[];

  // Constants for parsing ClientLogin errors.
  static const char kAccountDeletedError[];
  static const char kAccountDeletedErrorCode[];
  static const char kAccountDisabledError[];
  static const char kAccountDisabledErrorCode[];
  static const char kBadAuthenticationError[];
  static const char kBadAuthenticationErrorCode[];
  static const char kCaptchaError[];
  static const char kCaptchaErrorCode[];
  static const char kServiceUnavailableError[];
  static const char kServiceUnavailableErrorCode[];
  static const char kErrorParam[];
  static const char kErrorUrlParam[];
  static const char kCaptchaUrlParam[];
  static const char kCaptchaTokenParam[];

  // Constants for parsing ClientOAuth errors.
  static const char kNeedsAdditional[];
  static const char kCaptcha[];
  static const char kTwoFactor[];

  // Constants for request/response for OAuth2 requests.
  static const char kAuthHeaderFormat[];
  static const char kOAuthHeaderFormat[];
  static const char kOAuth2BearerHeaderFormat[];
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

  void OnGetUserInfoFetched(const std::string& data,
                            net::Error net_error,
                            int response_code);

  void OnMergeSessionFetched(const std::string& data,
                             net::Error net_error,
                             int response_code);

  void OnUberAuthTokenFetch(const std::string& data,
                            net::Error net_error,
                            int response_code);

  void OnOAuthMultiloginFetched(const std::string& data,
                                net::Error net_error,
                                int response_code);

  void OnOAuthLoginFetched(const std::string& data,
                           net::Error net_error,
                           int response_code);

  void OnGetCheckConnectionInfoFetched(const std::string& data,
                                       net::Error net_error,
                                       int response_code);

  // Tokenize the results of a ClientLogin fetch.
  static void ParseClientLoginResponse(const std::string& data,
                                       std::string* sid,
                                       std::string* lsid,
                                       std::string* token);

  static void ParseClientLoginFailure(const std::string& data,
                                      std::string* error,
                                      std::string* error_url,
                                      std::string* captcha_url,
                                      std::string* captcha_token);

  // Is this a special case Gaia error for TwoFactor auth?
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Is this a special case Gaia error for Less Secure Apps?
  static bool IsWebLoginRequiredSuccess(const std::string& alleged_error);

  // Supply the sid / lsid returned from ClientLogin in order to
  // request a long lived auth token for a service.
  static std::string MakeIssueAuthTokenBody(const std::string& sid,
                                            const std::string& lsid,
                                            const char* const service);
  // Given auth code and device ID (optional), create body to get OAuth2 token
  // pair.
  static std::string MakeGetTokenPairBody(const std::string& auth_code,
                                          const std::string& device_id);
  // Given an OAuth2 token, create body to revoke the token.
  std::string MakeRevokeTokenBody(const std::string& auth_token);
  // Supply the lsid returned from ClientLogin in order to fetch
  // user information.
  static std::string MakeGetUserInfoBody(const std::string& lsid);

  // Supply the authentication token returned from StartIssueAuthToken.
  static std::string MakeMergeSessionQuery(
      const std::string& auth_token,
      const std::string& external_cc_result,
      const std::string& continue_url,
      const std::string& source);

  static std::string MakeGetAuthCodeHeader(const std::string& auth_token);

  static std::string MakeOAuthLoginBody(const std::string& service,
                                        const std::string& source);

  // From a SimpleURLLoader result, generates an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateAuthError(const std::string& data,
                                                  net::Error net_error);

  // These fields are common to GaiaAuthFetcher, same every request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GaiaAuthConsumer* const consumer_;
  std::string source_;
  const GURL oauth2_token_gurl_;
  const GURL oauth2_revoke_gurl_;
  const GURL get_user_info_gurl_;
  const GURL merge_session_gurl_;
  const GURL uberauth_token_gurl_;
  const GURL oauth_login_gurl_;
  const GURL oauth_multilogin_gurl_;
  const GURL list_accounts_gurl_;
  const GURL logout_gurl_;
  const GURL get_check_connection_info_url_;

  // While a fetch is going on:
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  GURL original_url_;
  std::string request_body_;

  std::string requested_service_;  // Currently tracked for IssueAuthToken only.
  bool fetch_pending_ = false;
  bool fetch_token_from_auth_code_ = false;

  // For investigation of https://crbug.com/876306.
  base::TimeDelta list_accounts_system_uptime_;
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // Process creation time is not available on iOS and Android.
  base::TimeDelta list_accounts_process_uptime_;
#endif

  friend class GaiaAuthFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CaptchaParse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, AccountDeletedError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, AccountDisabledError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, BadAuthenticationError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, IncomprehensibleError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ServiceUnavailableError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckTwoFactorResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, LoginNetFailure);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ParseOAuth2TokenPairResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthWithQuote);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeQuote);

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcher);
};

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
