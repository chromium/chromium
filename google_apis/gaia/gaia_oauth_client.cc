// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_oauth_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/backoff_entry.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_throttler_entry.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {
const char kAccessTokenValue[] = "access_token";
const char kRefreshTokenValue[] = "refresh_token";
const char kExpiresInValue[] = "expires_in";
}

namespace gaia {

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core> {
 public:
  Core(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : backoff_entry_(&backoff_policy_),
        num_retries_(0),
        max_retries_(0),
        url_loader_factory_(url_loader_factory),
        delegate_(nullptr),
        request_type_(NO_PENDING_REQUEST) {
    backoff_policy_.num_errors_to_ignore =
        net::URLRequestThrottlerEntry::kDefaultNumErrorsToIgnore;
    backoff_policy_.initial_delay_ms =
        net::URLRequestThrottlerEntry::kDefaultInitialDelayMs;
    backoff_policy_.multiply_factor =
        net::URLRequestThrottlerEntry::kDefaultMultiplyFactor;
    backoff_policy_.jitter_factor =
        net::URLRequestThrottlerEntry::kDefaultJitterFactor;
    backoff_policy_.maximum_backoff_ms =
        net::URLRequestThrottlerEntry::kDefaultMaximumBackoffMs;
    backoff_policy_.entry_lifetime_ms =
        net::URLRequestThrottlerEntry::kDefaultEntryLifetimeMs;
    backoff_policy_.always_use_initial_delay = false;
  }

  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             GaiaOAuthClient::Delegate* delegate);
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    const std::vector<std::string>& scopes,
                    int max_retries,
                    GaiaOAuthClient::Delegate* delegate);
  void GetUserEmail(const std::string& oauth_access_token,
                    int max_retries,
                    Delegate* delegate);
  void GetUserId(const std::string& oauth_access_token,
                 int max_retries,
                 Delegate* delegate);
  void GetUserInfo(const std::string& oauth_access_token,
                   int max_retries,
                   Delegate* delegate);
  void GetTokenInfo(const std::string& qualifier,
                    const std::string& query,
                    int max_retries,
                    Delegate* delegate);

  // Called as a SimpleURLLoader callback
  void OnURLLoadComplete(std::unique_ptr<std::string> body);

 private:
  friend class base::RefCountedThreadSafe<Core>;

  enum RequestType {
    NO_PENDING_REQUEST,
    TOKENS_FROM_AUTH_CODE,
    REFRESH_TOKEN,
    TOKEN_INFO,
    USER_EMAIL,
    USER_ID,
    USER_INFO,
  };

  ~Core() {}

  void GetUserInfoImpl(RequestType type,
                       const std::string& oauth_access_token,
                       int max_retries,
                       Delegate* delegate);

  // Stores request settings into |this| and calls SendRequest().
  void MakeRequest(
      RequestType type,
      const GURL& url,
      std::string post_body /* may be empty if not needed*/,
      std::string authorization_header /* empty if not needed */,
      int max_retries,
      GaiaOAuthClient::Delegate* delegate,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Sends out a request based on things MakeRequest() stored...
  // once |backoff_entry_| says it's OK. Can be called many times to retry,
  // assuming |request_| is destroyed first.
  void SendRequest();

  // Actually sends the request.
  void SendRequestImpl();

  void HandleResponse(std::unique_ptr<std::string> body,
                      bool* should_retry_request);

  net::BackoffEntry::Policy backoff_policy_;
  net::BackoffEntry backoff_entry_;

  int num_retries_;
  int max_retries_;
  GURL url_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::string post_body_;
  std::string authorization_header_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GaiaOAuthClient::Delegate* delegate_;
  std::unique_ptr<network::SimpleURLLoader> request_;
  RequestType request_type_;

  base::WeakPtrFactory<Core> weak_ptr_factory_{this};
};

void GaiaOAuthClient::Core::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  std::string post_body =
      "code=" + net::EscapeUrlEncodedData(auth_code, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&redirect_uri=" +
      net::EscapeUrlEncodedData(oauth_client_info.redirect_uri, true) +
      "&grant_type=authorization_code";
  net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("gaia_oauth_client_get_tokens", R"(
        semantics {
          sender: "OAuth 2.0 calls"
          description:
            "This request exchanges an authorization code for an OAuth 2.0 "
            "refresh token and an OAuth 2.0 access token."
          trigger:
            "This request is triggered when a Chrome service requires an "
            "access token and a refresh token (e.g. Cloud Print, Chrome Remote "
            "Desktop etc.) See https://developers.google.com/identity/protocols"
            "/OAuth2 for more information about the Google implementation of "
            "the OAuth 2.0 protocol."
          data:
            "The Google console client ID and client secret of the caller, the "
            "OAuth authorization code and the redirect URI."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })"));
  MakeRequest(TOKENS_FROM_AUTH_CODE,
              GURL(GaiaUrls::GetInstance()->oauth2_token_url()), post_body,
              /* authorization_header = */ std::string(), max_retries, delegate,
              traffic_annotation);
}

void GaiaOAuthClient::Core::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    const std::vector<std::string>& scopes,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  std::string post_body =
      "refresh_token=" + net::EscapeUrlEncodedData(refresh_token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&grant_type=refresh_token";

  if (!scopes.empty()) {
    std::string scopes_string = base::JoinString(scopes, " ");
    post_body += "&scope=" + net::EscapeUrlEncodedData(scopes_string, true);
  }

  net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("gaia_oauth_client_refresh_token", R"(
        semantics {
          sender: "OAuth 2.0 calls"
          description:
            "This request fetches a fresh access token that can be used to "
            "authenticate an API call to a Google web endpoint."
          trigger:
            "This is called whenever the caller needs a fresh OAuth 2.0 access "
            "token."
          data:
            "The OAuth 2.0 refresh token, the Google console client ID and "
            "client secret of the caller, and optionally the scopes of the API "
            "for which the access token should be authorized."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })"));
  MakeRequest(REFRESH_TOKEN, GURL(GaiaUrls::GetInstance()->oauth2_token_url()),
              post_body,
              /* authorization_header = */ std::string(), max_retries, delegate,
              traffic_annotation);
}

void GaiaOAuthClient::Core::GetUserEmail(const std::string& oauth_access_token,
                                         int max_retries,
                                         Delegate* delegate) {
  GetUserInfoImpl(USER_EMAIL, oauth_access_token, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserId(const std::string& oauth_access_token,
                                      int max_retries,
                                      Delegate* delegate) {
  GetUserInfoImpl(USER_ID, oauth_access_token, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserInfo(const std::string& oauth_access_token,
                                      int max_retries,
                                      Delegate* delegate) {
  GetUserInfoImpl(USER_INFO, oauth_access_token, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserInfoImpl(
    RequestType type,
    const std::string& oauth_access_token,
    int max_retries,
    Delegate* delegate) {
  net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("gaia_oauth_client_get_user_info", R"(
        semantics {
          sender: "OAuth 2.0 calls"
          description:
            "This request is used to fetch profile information about the user, "
            "like the email, the ID of the account, the full name, and the "
            "profile picture."
          trigger:
            "The main trigger for this request is in the AccountTrackerService "
            "that fetches the user info soon after the user signs in."
          data:
            "The OAuth 2.0 access token of the account."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })"));
  std::string auth = "OAuth " + oauth_access_token;
  MakeRequest(type, GaiaUrls::GetInstance()->oauth_user_info_url(),
              /* post_body = */ std::string(), auth, max_retries, delegate,
              traffic_annotation);
}

void GaiaOAuthClient::Core::GetTokenInfo(const std::string& qualifier,
                                         const std::string& query,
                                         int max_retries,
                                         Delegate* delegate) {
  std::string post_body =
      qualifier + "=" + net::EscapeUrlEncodedData(query, true);
  net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("gaia_oauth_client_get_token_info",
                                          R"(
        semantics {
          sender: "OAuth 2.0 calls"
          description:
            "This request fetches information about an OAuth 2.0 access token. "
            "The response is a dictionary of response values. The provided "
            "access token may have any scope, and basic results will be "
            "returned: issued_to, audience, scope, expires_in, access_type. In "
            "addition, if the https://www.googleapis.com/auth/userinfo.email "
            "scope is present, the email and verified_email fields will be "
            "returned. If the https://www.googleapis.com/auth/userinfo.profile "
            "scope is present, the user_id field will be returned."
          trigger:
            "This is triggered after a Google account is added to the browser. "
            "It it also triggered after each successful fetch of an OAuth 2.0 "
            "access token."
          data: "The OAuth 2.0 access token."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })"));
  MakeRequest(TOKEN_INFO,
              GURL(GaiaUrls::GetInstance()->oauth2_token_info_url()), post_body,
              /* authorization_header = */ std::string(), max_retries, delegate,
              traffic_annotation);
}

void GaiaOAuthClient::Core::MakeRequest(
    RequestType type,
    const GURL& url,
    std::string post_body,
    std::string authorization_header,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_EQ(request_type_, NO_PENDING_REQUEST);
  request_type_ = type;
  delegate_ = delegate;
  num_retries_ = 0;
  max_retries_ = max_retries;
  url_ = url;
  traffic_annotation_ = traffic_annotation;
  post_body_ = std::move(post_body);
  authorization_header_ = std::move(authorization_header);
  SendRequest();
}

void GaiaOAuthClient::Core::SendRequest() {
  if (backoff_entry_.ShouldRejectRequest()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GaiaOAuthClient::Core::SendRequestImpl,
                       weak_ptr_factory_.GetWeakPtr()),
        backoff_entry_.GetTimeUntilRelease());
  } else {
    SendRequestImpl();
  }
}

void GaiaOAuthClient::Core::SendRequestImpl() {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = post_body_.empty() ? "GET" : "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (!authorization_header_.empty())
    resource_request->headers.SetHeader("Authorization", authorization_header_);

  request_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation_));

  if (!post_body_.empty()) {
    request_->AttachStringForUpload(post_body_,
                                    "application/x-www-form-urlencoded");
  }

  // Retry is implemented internally.
  request_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);

  request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      // Unretained(this) is safe since |this| owns |request_|, and its deletion
      // will cancel the callback.
      base::BindOnce(&GaiaOAuthClient::Core::OnURLLoadComplete,
                     base::Unretained(this)));
}

void GaiaOAuthClient::Core::OnURLLoadComplete(
    std::unique_ptr<std::string> body) {
  bool should_retry = false;
  base::WeakPtr<GaiaOAuthClient::Core> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  // HandleResponse() may delete |this| if it assigns |should_retry| == false.
  HandleResponse(std::move(body), &should_retry);
  if (should_retry) {
    num_retries_++;
    backoff_entry_.InformOfRequest(false);
    SendRequest();
  } else {
    if (weak_this)
      backoff_entry_.InformOfRequest(true);
  }
}

void GaiaOAuthClient::Core::HandleResponse(std::unique_ptr<std::string> body,
                                           bool* should_retry_request) {
  *should_retry_request = false;
  // Move ownership of the request fetcher into a local scoped_ptr which
  // will be nuked when we're done handling the request.
  std::unique_ptr<network::SimpleURLLoader> source = std::move(request_);

  int response_code = -1;
  if (source->ResponseInfo() && source->ResponseInfo()->headers)
    response_code = source->ResponseInfo()->headers->response_code();

  // HTTP_BAD_REQUEST means the arguments are invalid.  HTTP_UNAUTHORIZED means
  // the access or refresh token is invalid. No point retrying. We are
  // done here.
  if (response_code == net::HTTP_BAD_REQUEST ||
      response_code == net::HTTP_UNAUTHORIZED) {
    delegate_->OnOAuthError();
    return;
  }

  std::unique_ptr<base::DictionaryValue> response_dict;
  if (response_code == net::HTTP_OK && body) {
    std::string data = std::move(*body);
    std::unique_ptr<base::Value> message_value =
        base::JSONReader::ReadDeprecated(data);
    if (message_value.get() && message_value->is_dict()) {
      response_dict.reset(
          static_cast<base::DictionaryValue*>(message_value.release()));
    }
  }

  if (!response_dict.get()) {
    // If we don't have an access token yet and the the error was not
    // RC_BAD_REQUEST, we may need to retry.
    if ((max_retries_ != -1) && (num_retries_ >= max_retries_)) {
      // Retry limit reached. Give up.
      request_type_ = NO_PENDING_REQUEST;
      delegate_->OnNetworkError(response_code);
    } else {
      *should_retry_request = true;
    }
    return;
  }

  RequestType type = request_type_;
  request_type_ = NO_PENDING_REQUEST;

  switch (type) {
    case USER_EMAIL: {
      std::string email;
      response_dict->GetString("email", &email);
      delegate_->OnGetUserEmailResponse(email);
      break;
    }

    case USER_ID: {
      std::string id;
      response_dict->GetString("id", &id);
      delegate_->OnGetUserIdResponse(id);
      break;
    }

    case USER_INFO: {
      delegate_->OnGetUserInfoResponse(std::move(response_dict));
      break;
    }

    case TOKEN_INFO: {
      delegate_->OnGetTokenInfoResponse(std::move(response_dict));
      break;
    }

    case TOKENS_FROM_AUTH_CODE:
    case REFRESH_TOKEN: {
      std::string access_token;
      std::string refresh_token;
      int expires_in_seconds = 0;
      response_dict->GetString(kAccessTokenValue, &access_token);
      response_dict->GetString(kRefreshTokenValue, &refresh_token);
      response_dict->GetInteger(kExpiresInValue, &expires_in_seconds);

      if (access_token.empty()) {
        delegate_->OnOAuthError();
        return;
      }

      if (type == REFRESH_TOKEN) {
        delegate_->OnRefreshTokenResponse(access_token, expires_in_seconds);
      } else {
        delegate_->OnGetTokensResponse(refresh_token,
                                       access_token,
                                       expires_in_seconds);
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

GaiaOAuthClient::GaiaOAuthClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  core_ = new Core(std::move(url_loader_factory));
}

GaiaOAuthClient::~GaiaOAuthClient() {
}

void GaiaOAuthClient::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    Delegate* delegate) {
  return core_->GetTokensFromAuthCode(oauth_client_info,
                                      auth_code,
                                      max_retries,
                                      delegate);
}

void GaiaOAuthClient::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    const std::vector<std::string>& scopes,
    int max_retries,
    Delegate* delegate) {
  return core_->RefreshToken(oauth_client_info,
                             refresh_token,
                             scopes,
                             max_retries,
                             delegate);
}

void GaiaOAuthClient::GetUserEmail(const std::string& access_token,
                                  int max_retries,
                                  Delegate* delegate) {
  return core_->GetUserEmail(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetUserId(const std::string& access_token,
                                int max_retries,
                                Delegate* delegate) {
  return core_->GetUserId(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetUserInfo(const std::string& access_token,
                                  int max_retries,
                                  Delegate* delegate) {
  return core_->GetUserInfo(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetTokenInfo(const std::string& access_token,
                                   int max_retries,
                                   Delegate* delegate) {
  return core_->GetTokenInfo("access_token", access_token, max_retries,
                             delegate);
}

void GaiaOAuthClient::GetTokenHandleInfo(const std::string& token_handle,
                                         int max_retries,
                                         Delegate* delegate) {
  return core_->GetTokenInfo("token_handle", token_handle, max_retries,
                             delegate);
}

}  // namespace gaia
