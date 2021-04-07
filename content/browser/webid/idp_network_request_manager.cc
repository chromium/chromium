// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include "base/base64url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace content {

namespace {
// TODO(kenrb): These need to be defined in the explainer or draft spec and
// referenced here.

// Well-known configuration keys.
constexpr char kIdpEndpointKey[] = "idp_endpoint";
constexpr char kTokenEndpointKey[] = "idtoken_endpoint";
constexpr char kAccountsEndpointKey[] = "accounts_endpoint";

// Sign-in request response keys.
// TODO(majidvp): For consistency rename to signin_endpoint and move into
// `.well-known`.
constexpr char kSigninUrlKey[] = "signin_url";
constexpr char kIdTokenKey[] = "id_token";
constexpr char kAccountsKey[] = "accounts";

constexpr char kAcceptMimeType[] = "application/json";

// `Sec-` prefix makes this a forbidden header and cannot be added by
// JavaScript.
// See https://fetch.spec.whatwg.org/#forbidden-header-name
constexpr char kSecWebIdHeader[] = "Sec-WebID";

// 1 MiB is an arbitrary upper bound that should account for any reasonable
// response size that is a part of this protocol.
constexpr int maxResponseSizeInKiB = 1024;

net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("webid", R"(
        semantics {
          sender: "WebID Backend"
          description:
            "The WebID API allows websites to initiate user account login "
            "with identity providers which provide federated sign-in "
            "capabilities using OpenID Connect. The API provides a "
            "browser-mediated alternative to previously existing federated "
            "sign-in implementations."
          trigger:
            "A website executes the navigator.id.get() JavaScript method to "
            "initiate federated user sign-in to a designated provider."
          data:
            "An identity request contains a scope of claims specifying what "
            "user information is being requested from the identity provider, "
            "a label identifying the calling website application, and some "
            "OpenID Connect protocol functional fields."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "Not user controlled. But the verification is a trusted "
                   "API that doesn't use user data."
          policy_exception_justification:
            "Not implemented, considered not useful as no content is being "
            "uploaded; this request merely downloads the resources on the web."
        })");
}

scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory(
    content::RenderFrameHost* host) {
  return host->GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
}

std::unique_ptr<network::ResourceRequest> CreateCredentialedResourceRequest(
    GURL target_url,
    url::Origin initiator) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  auto target_origin = url::Origin::Create(target_url);
  auto site_for_cookies = net::SiteForCookies::FromOrigin(target_origin);
  resource_request->request_initiator = initiator;
  resource_request->url = target_url;
  resource_request->site_for_cookies = site_for_cookies;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kAcceptMimeType);
  // This header is present exclusively for CSRF resistance.
  // TODO(kenrb): To avoid the value being depended on  set it to a random
  // value. We can later change it if something more useful e.g., a version is
  // needed. https://crbug.com/1196371
  resource_request->headers.SetHeader(kSecWebIdHeader, "1.0");
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, target_origin, target_origin,
      site_for_cookies);

  return resource_request;
}

base::Optional<content::IdentityRequestAccount> ParseAccount(
    const base::Value& account) {
  auto* sub = account.FindStringKey("sub");
  auto* email = account.FindStringKey("email");
  auto* name = account.FindStringKey("name");
  auto* given_name = account.FindStringKey("given_name");
  auto* picture = account.FindStringKey("picture");

  // required fields
  if (!(sub && email && name))
    return base::nullopt;

  return content::IdentityRequestAccount(*sub, *email, *name,
                                         given_name ? *given_name : "",
                                         picture ? *picture : "");
}

}  // namespace

// static
constexpr char IdpNetworkRequestManager::kWellKnownFilePath[];

// static
std::unique_ptr<IdpNetworkRequestManager> IdpNetworkRequestManager::Create(
    const GURL& provider,
    RenderFrameHost* host) {
  // WebID is restricted to secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(url::Origin::Create(provider)))
    return nullptr;

  return std::make_unique<IdpNetworkRequestManager>(provider, host);
}

IdpNetworkRequestManager::IdpNetworkRequestManager(const GURL& provider,
                                                   RenderFrameHost* host)
    : provider_(provider), render_frame_host_(host) {}

IdpNetworkRequestManager::~IdpNetworkRequestManager() = default;

void IdpNetworkRequestManager::FetchIdpWellKnown(
    FetchWellKnownCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!idp_well_known_callback_);

  idp_well_known_callback_ = std::move(callback);

  const url::Origin& idp_origin = url::Origin::Create(provider_);
  GURL target_url =
      idp_origin.GetURL().Resolve(IdpNetworkRequestManager::kWellKnownFilePath);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      CreateTrafficAnnotation();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = target_url;
  // TODO(kenrb): credentials_mode should be kOmit, but for prototyping
  // purposes it is useful to be able to run test IdPs on services that always
  // require cookies. This needs to be changed back when a better solution is
  // found or those test IdPs are no longer required.
  // See https://crbug.com/1159177.
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kAcceptMimeType);
  // TODO(kenrb): Not following redirects is important for security because
  // this bypasses CORB. Ensure there is a test added.
  // https://crbug.com/1155312.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->request_initiator =
      render_frame_host_->GetLastCommittedOrigin();
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 idp_origin, idp_origin, net::SiteForCookies());

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  // Use the browser process URL loader factory because it has cross-origin
  // read blocking disabled.
  auto loader_factory = GetUrlLoaderFactory(render_frame_host_);

  url_loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnWellKnownLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendSigninRequest(
    const GURL& signin_url,
    const std::string& request,
    SigninRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!signin_request_callback_);

  signin_request_callback_ = std::move(callback);

  // TODO(kenrb): A straight URL encoding isn't right. Add proper parsing.
  // https://crbug.com/1141125.
  std::string encoded_request;
  base::Base64UrlEncode(base::StringPiece(request),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_request);

  // TODO: Should this be a POST, rather than a GET using query parameters?
  // https://crbug.com/1141125.
  GURL target_url = GURL(signin_url.spec() + "?" + encoded_request);
  auto resource_request = CreateCredentialedResourceRequest(
      target_url, render_frame_host_->GetLastCommittedOrigin());
  auto traffic_annotation = CreateTrafficAnnotation();
  // TODO(kenrb): Make this not send cookies. https://crbug.com/1141125.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  auto loader_factory = GetUrlLoaderFactory(render_frame_host_);

  url_loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnSigninRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendAccountsRequest(
    const GURL& accounts_url,
    AccountsRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!accounts_request_callback_);
  accounts_request_callback_ = std::move(callback);

  auto resource_request = CreateCredentialedResourceRequest(
      accounts_url, render_frame_host_->GetLastCommittedOrigin());
  // Use ReferrerPolicy::NO_REFERRER for this request so that relying party
  // identity is not exposed to the Identity provider via referror.
  resource_request->referrer_policy = net::ReferrerPolicy::NO_REFERRER;
  auto traffic_annotation = CreateTrafficAnnotation();
  // TODO(kenrb): Make this not send cookies. https://crbug.com/1141125.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  auto loader_factory = GetUrlLoaderFactory(render_frame_host_);

  url_loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnAccountsRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendTokenRequest(const GURL& token_url,
                                                const std::string& account,
                                                const std::string& request,
                                                TokenRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!token_request_callback_);

  token_request_callback_ = std::move(callback);

  // TODO(majidvp): Append account and request to the post body.
  auto resource_request = CreateCredentialedResourceRequest(
      token_url, render_frame_host_->GetLastCommittedOrigin());
  auto traffic_annotation = CreateTrafficAnnotation();
  // TODO(kenrb): Make this not send cookies. https://crbug.com/1141125.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  auto loader_factory = GetUrlLoaderFactory(render_frame_host_);

  url_loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnTokenRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

// static
bool IdpNetworkRequestManager::ParseAccounts(
    const base::Value* accounts,
    IdpNetworkRequestManager::AccountList& account_list) {
  if (!accounts->is_list())
    return false;

  for (auto& account : accounts->GetList()) {
    if (!account.is_dict())
      return false;

    auto parsed_account = ParseAccount(account);
    if (parsed_account)
      account_list.push_back(parsed_account.value());
  }
  return true;
}

void IdpNetworkRequestManager::OnWellKnownLoaded(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  auto* response_info = url_loader_->ResponseInfo();
  if (response_info && response_info->headers)
    response_code = response_info->headers->response_code();

  url_loader_.reset();

  if (response_code == net::HTTP_NOT_FOUND) {
    std::move(idp_well_known_callback_)
        .Run(FetchStatus::kWebIdNotSupported, Endpoints());
    return;
  }

  if (!response_body) {
    std::move(idp_well_known_callback_)
        .Run(FetchStatus::kFetchError, Endpoints());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnWellKnownParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnWellKnownParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  auto Fail = [&]() {
    std::move(idp_well_known_callback_)
        .Run(FetchStatus::kInvalidResponseError, Endpoints());
  };

  if (!result.value) {
    Fail();
    return;
  }

  auto& response = *result.value;
  if (!response.is_dict()) {
    Fail();
    return;
  }

  auto ExtractEndpoint = [&](const char* key) {
    const base::Value* endpoint = response.FindKey(key);
    if (!endpoint || !endpoint->is_string()) {
      return std::string();
    }
    return endpoint->GetString();
  };

  auto idp_endpoint = ExtractEndpoint(kIdpEndpointKey);
  auto token_endpoint = ExtractEndpoint(kTokenEndpointKey);
  auto accounts_endpoint = ExtractEndpoint(kAccountsEndpointKey);

  std::move(idp_well_known_callback_)
      .Run(FetchStatus::kSuccess,
           {idp_endpoint, token_endpoint, accounts_endpoint});
}

void IdpNetworkRequestManager::OnSigninRequestResponse(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  url_loader_.reset();

  if (!response_body) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kSigninError, std::string());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnSigninRequestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnSigninRequestParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kInvalidResponseError, std::string());
    return;
  }

  auto& response = *result.value;

  if (!response.is_dict()) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kInvalidResponseError, std::string());
    return;
  }

  // TODO(kenrb): This possibly should be part of the well-known file, unless
  // IDPs ever have a reason to serve different URLs for sign-in pages.
  // https://crbug.com/1141125.
  const base::Value* signin_url = response.FindKey(kSigninUrlKey);
  const base::Value* id_token = response.FindKey(kIdTokenKey);

  // Only one of the fields should be present.
  bool signin_url_present = signin_url && signin_url->is_string();
  bool token_present = id_token && id_token->is_string();
  bool all_present = signin_url_present && token_present;
  if (!(signin_url_present || token_present) || all_present) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kInvalidResponseError, std::string());
    return;
  }

  if (signin_url) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kLoadIdp, signin_url->GetString());
    return;
  }

  std::move(signin_request_callback_)
      .Run(SigninResponse::kTokenGranted, id_token->GetString());
}

void IdpNetworkRequestManager::OnAccountsRequestResponse(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  url_loader_.reset();

  if (!response_body) {
    std::move(accounts_request_callback_)
        .Run(AccountsResponse::kNetError, AccountList());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnAccountsRequestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnAccountsRequestParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  auto Fail = [&]() {
    std::move(accounts_request_callback_)
        .Run(AccountsResponse::kInvalidResponseError, AccountList());
  };

  if (!result.value) {
    Fail();
    return;
  }

  auto& response = *result.value;
  if (!response.is_dict()) {
    Fail();
    return;
  }
  AccountList account_list;
  const base::Value* accounts = response.FindKey(kAccountsKey);
  bool accounts_present = accounts && ParseAccounts(accounts, account_list);

  if (!accounts_present) {
    Fail();
    return;
  }
  std::move(accounts_request_callback_)
      .Run(AccountsResponse::kSuccess, account_list);
}

void IdpNetworkRequestManager::OnTokenRequestResponse(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  url_loader_.reset();

  if (!response_body) {
    std::move(token_request_callback_)
        .Run(TokenResponse::kNetError, std::string());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnTokenRequestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnTokenRequestParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  auto Fail = [&]() {
    std::move(token_request_callback_)
        .Run(TokenResponse::kInvalidResponseError, std::string());
  };

  if (!result.value) {
    Fail();
    return;
  }

  auto& response = *result.value;
  if (!response.is_dict()) {
    Fail();
    return;
  }
  const base::Value* id_token = response.FindKey(kIdTokenKey);
  bool token_present = id_token && id_token->is_string();

  if (!token_present) {
    Fail();
    return;
  }
  std::move(token_request_callback_)
      .Run(TokenResponse::kSuccess, id_token->GetString());
}

}  // namespace content
