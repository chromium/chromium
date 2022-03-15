// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/color_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/escape.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "url/origin.h"

namespace content {

namespace {
using LoginState = IdentityRequestAccount::LoginState;

// TODO(kenrb): These need to be defined in the explainer or draft spec and
// referenced here.

// fedcm.json configuration keys.
constexpr char kTokenEndpointKey[] = "id_token_endpoint";
constexpr char kAccountsEndpointKey[] = "accounts_endpoint";
constexpr char kClientMetadataEndpointKey[] = "client_metadata_endpoint";
constexpr char kRevocationEndpoint[] = "revocation_endpoint";

// Keys in fedcm.json 'branding' dictionary.
constexpr char kIdpBrandingBackgroundColor[] = "background_color";
constexpr char kIdpBrandingForegroundColor[] = "color";
constexpr char kIdpBrandingIcons[] = "icons";

// Client metadata keys.
constexpr char kPrivacyPolicyKey[] = "privacy_policy_url";
constexpr char kTermsOfServiceKey[] = "terms_of_service_url";

// Accounts endpoint response keys.
constexpr char kAccountsKey[] = "accounts";
constexpr char kIdpBrandingKey[] = "branding";

// Keys in 'account' dictionary in accounts endpoint.
constexpr char kAccountIdKey[] = "id";
constexpr char kAccountEmailKey[] = "email";
constexpr char kAccountNameKey[] = "name";
constexpr char kAccountGivenNameKey[] = "given_name";
constexpr char kAccountPictureKey[] = "picture";
constexpr char kAccountApprovedClientsKey[] = "approved_clients";

// Keys in 'branding' 'icons' dictionary in accounts endpoint.
constexpr char kIdpBrandingIconUrl[] = "url";
constexpr char kIdpBrandingIconSize[] = "size";

// Sign-in request response keys.
// TODO(majidvp): For consistency rename to signin_endpoint and move into the
// fedcm manifest.
constexpr char kSigninUrlKey[] = "signin_url";
constexpr char kIdTokenKey[] = "id_token";

// Token request body keys
constexpr char kTokenAccountKey[] = "account_id";
constexpr char kTokenRequestKey[] = "request";

// Revoke request body keys.
constexpr char kClientIdKey[] = "client_id";
constexpr char kRevokeAccountKey[] = "account_id";
constexpr char kRevokeRequestKey[] = "request";

constexpr char kRequestBodyContentType[] = "application/x-www-form-urlencoded";

// 1 MiB is an arbitrary upper bound that should account for any reasonable
// response size that is a part of this protocol.
constexpr int maxResponseSizeInKiB = 1024;

// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("fedcm", R"(
        semantics {
          sender: "FedCM Backend"
          description:
            "The FedCM API allows websites to initiate user account login "
            "with identity providers which provide federated sign-in "
            "capabilities using OpenID Connect. The API provides a "
            "browser-mediated alternative to previously existing federated "
            "sign-in implementations."
          trigger:
            "A website executes the navigator.credentials.get() JavaScript "
            "method to initiate federated user sign-in to a designated "
            "provider."
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

void AddCsrfHeader(network::ResourceRequest* request) {
  request->headers.SetHeader(kSecFedCmCsrfHeader, kSecFedCmCsrfHeaderValue);
}

std::unique_ptr<network::ResourceRequest> CreateCredentialedResourceRequest(
    GURL target_url,
    bool send_referrer,
    url::Origin initiator,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  auto target_origin = url::Origin::Create(target_url);
  auto site_for_cookies = net::SiteForCookies::FromOrigin(target_origin);
  AddCsrfHeader(resource_request.get());
  resource_request->request_initiator = initiator;
  resource_request->url = target_url;
  resource_request->site_for_cookies = site_for_cookies;
  if (send_referrer) {
    resource_request->referrer = initiator.GetURL();
    // Since referrer_policy only affects redirects and we disable redirects
    // below, we don't need to set referrer_policy here.
  }
  // TODO(cbiesinger): Not following redirects is important for security because
  // this bypasses CORB. Ensure there is a test added.
  // https://crbug.com/1155312.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kRequestBodyContentType);

  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, target_origin, target_origin,
      site_for_cookies);
  DCHECK(client_security_state);
  resource_request->trusted_params->client_security_state =
      std::move(client_security_state);

  return resource_request;
}

absl::optional<content::IdentityRequestAccount> ParseAccount(
    const base::Value& account,
    const std::string& client_id) {
  auto* id = account.FindStringKey(kAccountIdKey);
  auto* email = account.FindStringKey(kAccountEmailKey);
  auto* name = account.FindStringKey(kAccountNameKey);
  auto* given_name = account.FindStringKey(kAccountGivenNameKey);
  auto* picture = account.FindStringKey(kAccountPictureKey);
  auto* approved_clients = account.FindListKey(kAccountApprovedClientsKey);

  // required fields
  if (!(id && email && name))
    return absl::nullopt;

  RecordApprovedClientsExistence(approved_clients != nullptr);

  absl::optional<LoginState> approved_value;
  if (approved_clients) {
    for (const base::Value& entry : approved_clients->GetListDeprecated()) {
      if (entry.is_string() && entry.GetString() == client_id) {
        approved_value = LoginState::kSignIn;
        break;
      }
    }
    if (!approved_value) {
      // We did get an approved_clients list, but the client ID was not found.
      // This means we are certain that the client is not approved; set to
      // kSignUp instead of leaving as nullopt.
      approved_value = LoginState::kSignUp;
    }
    RecordApprovedClientsSize(approved_clients->GetList().size());
  }

  return content::IdentityRequestAccount(
      *id, *email, *name, given_name ? *given_name : "",
      picture ? GURL(*picture) : GURL(), approved_value);
}

// Parses accounts from given Value. Returns true if parse is successful and
// adds parsed accounts to the |account_list|.
bool ParseAccounts(const base::Value* accounts,
                   IdpNetworkRequestManager::AccountList& account_list,
                   const std::string& client_id) {
  DCHECK(account_list.empty());
  if (!accounts->is_list())
    return false;

  for (auto& account : accounts->GetListDeprecated()) {
    if (!account.is_dict())
      return false;

    auto parsed_account = ParseAccount(account, client_id);
    if (parsed_account)
      account_list.push_back(parsed_account.value());
  }
  return !account_list.empty();
}

absl::optional<SkColor> ParseCssColor(const std::string* value) {
  if (value == nullptr)
    return absl::nullopt;

  SkColor color;
  if (!content::ParseCssColorString(*value, &color))
    return absl::nullopt;

  return SkColorSetA(color, 0xff);
}

// Parse IdentityProviderMetadata from given value. Overwrites |idp_metadata|
// with the parsed value.
void ParseIdentityProviderMetadata(const base::Value& idp_metadata_value,
                                   absl::optional<int> brand_icon_ideal_size,
                                   absl::optional<int> brand_icon_minimum_size,
                                   IdentityProviderMetadata& idp_metadata) {
  if (!idp_metadata_value.is_dict())
    return;

  idp_metadata.brand_background_color = ParseCssColor(
      idp_metadata_value.FindStringKey(kIdpBrandingBackgroundColor));
  if (idp_metadata.brand_background_color) {
    idp_metadata.brand_text_color = ParseCssColor(
        idp_metadata_value.FindStringKey(kIdpBrandingForegroundColor));
    if (idp_metadata.brand_text_color) {
      float text_contrast_ratio = color_utils::GetContrastRatio(
          *idp_metadata.brand_background_color, *idp_metadata.brand_text_color);
      if (text_contrast_ratio < color_utils::kMinimumReadableContrastRatio)
        idp_metadata.brand_text_color = absl::nullopt;
    }
  }

  const base::Value* icons_value =
      idp_metadata_value.FindKey(kIdpBrandingIcons);
  if (icons_value != nullptr && icons_value->is_list()) {
    std::vector<blink::Manifest::ImageResource> icons;
    for (const base::Value& icon_value : icons_value->GetListDeprecated()) {
      if (!icon_value.is_dict())
        continue;

      const std::string* icon_src =
          icon_value.FindStringKey(kIdpBrandingIconUrl);
      if (icon_src == nullptr)
        continue;

      blink::Manifest::ImageResource icon;
      icon.src = GURL(*icon_src);
      if (!icon.src.is_valid())
        continue;

      icon.purpose = {blink::mojom::ManifestImageResource_Purpose::MASKABLE};

      absl::optional<int> icon_size =
          icon_value.FindIntKey(kIdpBrandingIconSize);
      int icon_size_int = icon_size ? icon_size.value() : 0;
      icon.sizes.emplace_back(icon_size_int, icon_size_int);

      icons.push_back(icon);
    }

    if (brand_icon_minimum_size && brand_icon_ideal_size) {
      idp_metadata.brand_icon_url =
          blink::ManifestIconSelector::FindBestMatchingSquareIcon(
              icons,
              brand_icon_ideal_size.value() / kMaskableWebIconSafeZoneRatio,
              brand_icon_minimum_size.value() / kMaskableWebIconSafeZoneRatio,
              blink::mojom::ManifestImageResource_Purpose::MASKABLE);
    }
  }
}

using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
FetchStatus GetResponseError(network::SimpleURLLoader* url_loader,
                             std::string* response_body) {
  int response_code = -1;
  auto* response_info = url_loader->ResponseInfo();
  if (response_info && response_info->headers)
    response_code = response_info->headers->response_code();

  if (response_code == net::HTTP_NOT_FOUND)
    return FetchStatus::kHttpNotFoundError;

  if (!response_body)
    return FetchStatus::kNoResponseError;

  return FetchStatus::kSuccess;
}

FetchStatus GetParsingError(
    const data_decoder::DataDecoder::ValueOrError& result) {
  if (!result.value)
    return FetchStatus::kInvalidResponseError;

  auto& response = *result.value;
  if (!response.is_dict())
    return FetchStatus::kInvalidResponseError;

  return FetchStatus::kSuccess;
}

}  // namespace

IdpNetworkRequestManager::Endpoints::Endpoints() = default;
IdpNetworkRequestManager::Endpoints::~Endpoints() = default;
IdpNetworkRequestManager::Endpoints::Endpoints(const Endpoints& other) =
    default;

// static
constexpr char IdpNetworkRequestManager::kManifestFilePath[];

// static
std::unique_ptr<IdpNetworkRequestManager> IdpNetworkRequestManager::Create(
    const GURL& provider,
    RenderFrameHostImpl* host) {
  // FedCM is restricted to secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(url::Origin::Create(provider)))
    return nullptr;

  // Use the browser process URL loader factory because it has cross-origin
  // read blocking disabled. This is safe because even though these are
  // renderer-initiated fetches, the browser parses the responses and does not
  // leak the values to the renderer. The renderer should only learn information
  // when the user selects an account to sign in.
  return std::make_unique<IdpNetworkRequestManager>(
      provider, host->GetLastCommittedOrigin(),
      host->GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess(),
      host->BuildClientSecurityState());
}

IdpNetworkRequestManager::IdpNetworkRequestManager(
    const GURL& provider,
    const url::Origin& relying_party_origin,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    network::mojom::ClientSecurityStatePtr client_security_state)
    : provider_(provider),
      relying_party_origin_(relying_party_origin),
      loader_factory_(loader_factory),
      client_security_state_(std::move(client_security_state)) {
  DCHECK(client_security_state_);
  // If COEP:credentialless was used, this would break FedCM credentialled
  // requests. We clear the Cross-Origin-Embedder-Policy because FedCM responses
  // are not really embedded in the page. They do not enter the renderer
  // process. This is safe because FedCM does not leak any data to the
  // requesting page except for the final issued token, and we only get that
  // token if the server is a new FedCM server, on which we can rely to validate
  // requestors if they want to.
  client_security_state_->cross_origin_embedder_policy =
      network::CrossOriginEmbedderPolicy();
}

IdpNetworkRequestManager::~IdpNetworkRequestManager() = default;

void IdpNetworkRequestManager::FetchManifest(
    absl::optional<int> idp_brand_icon_ideal_size,
    absl::optional<int> idp_brand_icon_minimum_size,
    FetchManifestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!idp_manifest_callback_);

  idp_manifest_callback_ = std::move(callback);

  // Accepts both "https://idp.example/foo/" and "https://idp.example/foo" as
  // valid provider url to locate the manifest. Historically, URLs with a
  // trailing slash indicate a directory while those without a trailing slash
  // denote a file. However, to give developers more flexibility, we append a
  // trailing slash if one is not present.
  GURL target_url = provider_;
  if (target_url.path().empty() || target_url.path().back() != '/') {
    std::string new_path = target_url.path() + '/';
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);
    target_url = target_url.ReplaceComponents(replacements);
  }

  target_url = target_url.Resolve(IdpNetworkRequestManager::kManifestFilePath);

  url_loader_ =
      CreateUncredentialedUrlLoader(target_url, /* send_referrer= */ false);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnManifestLoaded,
                     weak_ptr_factory_.GetWeakPtr(), idp_brand_icon_ideal_size,
                     idp_brand_icon_minimum_size),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendSigninRequest(
    const GURL& signin_url,
    const std::string& request,
    SigninRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!signin_request_callback_);

  signin_request_callback_ = std::move(callback);

  std::string escaped_request = net::EscapeUrlEncodedData(request, true);

  GURL target_url = GURL(signin_url.spec() + "?" + escaped_request);
  url_loader_ =
      CreateCredentialedUrlLoader(target_url, /* send_referrer= */ true);
  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnSigninRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendAccountsRequest(
    const GURL& accounts_url,
    const std::string& client_id,
    AccountsRequestCallback callback) {
  DCHECK(!url_loader_);

  url_loader_ =
      CreateCredentialedUrlLoader(accounts_url, /* send_referrer= */ false);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnAccountsRequestResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     client_id),
      maxResponseSizeInKiB * 1024);
}

// TODO(majidvp): Should accept request in base::Value form instead of string.
std::string CreateTokenRequestBody(const std::string& account,
                                   const std::string& request) {
  // Given account and id_request creates the following JSON
  // ```json
  // {
  //   "account_id": "1234",
  //   "request": "nonce=abc987987cba&client_id=89898"
  //   }
  // }```
  base::Value request_data(base::Value::Type::DICTIONARY);
  request_data.SetStringKey(kTokenAccountKey, account);
  if (!request.empty())
    request_data.SetStringKey(kTokenRequestKey, request);

  std::string request_body;
  if (!base::JSONWriter::Write(request_data, &request_body)) {
    LOG(ERROR) << "Not able to serialize token request body.";
    return std::string();
  }
  return request_body;
}

void IdpNetworkRequestManager::SendTokenRequest(const GURL& token_url,
                                                const std::string& account,
                                                const std::string& request,
                                                TokenRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!token_request_callback_);

  token_request_callback_ = std::move(callback);
  if (request.empty()) {
    std::move(token_request_callback_)
        .Run(FetchStatus::kInvalidRequestError, std::string());
    return;
  }

  url_loader_ = CreateCredentialedUrlLoader(token_url,
                                            /* send_referrer= */ true, request);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnTokenRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

std::string CreateRevokeRequestBody(const std::string& client_id,
                                    const std::string& account) {
  // Given account and id_request creates the following JSON
  // ```json
  // {
  //   "account_id": "123",
  //   "request": {
  //     "client_id": "client1234"
  //   }
  // }```
  base::Value request_dict(base::Value::Type::DICTIONARY);
  request_dict.SetStringKey(kClientIdKey, client_id);

  base::Value request_data(base::Value::Type::DICTIONARY);
  request_data.SetStringKey(kRevokeAccountKey, account);
  request_data.SetKey(kRevokeRequestKey, std::move(request_dict));

  std::string request_body;
  if (!base::JSONWriter::Write(request_data, &request_body)) {
    LOG(ERROR) << "Not able to serialize token request body.";
    return std::string();
  }
  return request_body;
}

void IdpNetworkRequestManager::SendRevokeRequest(const GURL& revoke_url,
                                                 const std::string& client_id,
                                                 const std::string& hint,
                                                 RevokeCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!token_request_callback_);

  revoke_callback_ = std::move(callback);

  std::string revoke_request_body;
  if (!client_id.empty())
    revoke_request_body += "client_id=" + client_id;

  if (hint.empty()) {
    std::move(revoke_callback_).Run(RevokeResponse::kError);
    return;
  }
  if (!revoke_request_body.empty())
    revoke_request_body += "&";
  revoke_request_body += "hint=" + hint;

  url_loader_ = CreateCredentialedUrlLoader(
      revoke_url, /* send_referrer= */ true, revoke_request_body);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnRevokeResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendLogout(const GURL& logout_url,
                                          LogoutCallback callback) {
  // TODO(kenrb): Add browser test verifying that the response to this can
  // clear cookies. https://crbug.com/1155312.
  DCHECK(!url_loader_);
  DCHECK(!logout_callback_);

  logout_callback_ = std::move(callback);

  auto resource_request = CreateCredentialedResourceRequest(
      logout_url, /* send_referrer= */ false, relying_party_origin_,
      client_security_state_.Clone());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept, "*/*");

  auto traffic_annotation = CreateTrafficAnnotation();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnLogoutCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::OnManifestLoaded(
    absl::optional<int> idp_brand_icon_ideal_size,
    absl::optional<int> idp_brand_icon_minimum_size,
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(idp_manifest_callback_)
        .Run(response_error, Endpoints(), IdentityProviderMetadata());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnManifestParsed,
                     weak_ptr_factory_.GetWeakPtr(), idp_brand_icon_ideal_size,
                     idp_brand_icon_minimum_size));
}

void IdpNetworkRequestManager::OnManifestParsed(
    absl::optional<int> idp_brand_icon_ideal_size,
    absl::optional<int> idp_brand_icon_minimum_size,
    data_decoder::DataDecoder::ValueOrError result) {
  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    std::move(idp_manifest_callback_)
        .Run(FetchStatus::kInvalidResponseError, Endpoints(),
             IdentityProviderMetadata());
    return;
  }

  auto& response = *result.value;
  auto ExtractEndpoint = [&](const char* key) {
    const base::Value* endpoint = response.FindKey(key);
    if (!endpoint || !endpoint->is_string()) {
      return std::string();
    }
    return endpoint->GetString();
  };

  Endpoints endpoints;
  endpoints.token = ExtractEndpoint(kTokenEndpointKey);
  endpoints.accounts = ExtractEndpoint(kAccountsEndpointKey);
  endpoints.client_metadata = ExtractEndpoint(kClientMetadataEndpointKey);
  endpoints.revocation = ExtractEndpoint(kRevocationEndpoint);

  const base::Value* idp_metadata_value = response.FindKey(kIdpBrandingKey);
  IdentityProviderMetadata idp_metadata;
  if (idp_metadata_value) {
    ParseIdentityProviderMetadata(*idp_metadata_value,
                                  idp_brand_icon_ideal_size,
                                  idp_brand_icon_minimum_size, idp_metadata);
  }

  std::move(idp_manifest_callback_)
      .Run(FetchStatus::kSuccess, endpoints, std::move(idp_metadata));
}

void IdpNetworkRequestManager::OnSigninRequestResponse(
    std::unique_ptr<std::string> response_body) {
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

  // TODO(kenrb): This possibly should be part of the fedcm manifest, unless
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
    AccountsRequestCallback callback,
    std::string client_id,
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(callback).Run(response_error, AccountList());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnAccountsRequestParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     client_id));
}

void IdpNetworkRequestManager::OnAccountsRequestParsed(
    AccountsRequestCallback callback,
    std::string client_id,
    data_decoder::DataDecoder::ValueOrError result) {
  auto Fail = [&]() {
    std::move(callback).Run(FetchStatus::kInvalidResponseError, AccountList());
  };

  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    Fail();
    return;
  }

  AccountList account_list;
  auto& response = *result.value;
  const base::Value* accounts = response.FindKey(kAccountsKey);
  bool accounts_present =
      accounts && ParseAccounts(accounts, account_list, client_id);

  if (!accounts_present) {
    Fail();
    return;
  }

  std::move(callback).Run(FetchStatus::kSuccess, std::move(account_list));
}

void IdpNetworkRequestManager::OnTokenRequestResponse(
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(token_request_callback_).Run(response_error, std::string());
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
        .Run(FetchStatus::kInvalidResponseError, std::string());
  };

  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    Fail();
    return;
  }

  auto& response = *result.value;
  const base::Value* id_token = response.FindKey(kIdTokenKey);
  bool token_present = id_token && id_token->is_string();

  if (!token_present) {
    Fail();
    return;
  }
  std::move(token_request_callback_)
      .Run(FetchStatus::kSuccess, id_token->GetString());
}

void IdpNetworkRequestManager::OnRevokeResponse(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();
  RevokeResponse status =
      response_body ? RevokeResponse::kSuccess : RevokeResponse::kError;
  std::move(revoke_callback_).Run(status);
}

void IdpNetworkRequestManager::OnLogoutCompleted(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();
  std::move(logout_callback_).Run();
}

void IdpNetworkRequestManager::FetchClientMetadata(
    const GURL& endpoint,
    const std::string& client_id,
    FetchClientMetadataCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!client_metadata_callback_);

  client_metadata_callback_ = std::move(callback);

  GURL target_url = endpoint.Resolve(
      "?client_id=" + net::EscapeQueryParamValue(client_id, true));

  url_loader_ =
      CreateUncredentialedUrlLoader(target_url, /* send_referrer= */ true);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnClientMetadataLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::OnClientMetadataLoaded(
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(client_metadata_callback_).Run(response_error, ClientMetadata());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnClientMetadataParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnClientMetadataParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    std::move(client_metadata_callback_)
        .Run(FetchStatus::kInvalidResponseError, ClientMetadata());
    return;
  }

  auto& response = *result.value;
  auto ExtractUrl = [&](const char* key) {
    const base::Value* endpoint = response.FindKey(key);
    if (!endpoint || !endpoint->is_string()) {
      return std::string();
    }
    return endpoint->GetString();
  };

  ClientMetadata data;
  data.privacy_policy_url = ExtractUrl(kPrivacyPolicyKey);
  data.terms_of_service_url = ExtractUrl(kTermsOfServiceKey);

  std::move(client_metadata_callback_).Run(FetchStatus::kSuccess, data);
}

std::unique_ptr<network::SimpleURLLoader>
IdpNetworkRequestManager::CreateUncredentialedUrlLoader(
    const GURL& target_url,
    bool send_referrer) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      CreateTrafficAnnotation();

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = target_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kRequestBodyContentType);
  AddCsrfHeader(resource_request.get());
  if (send_referrer) {
    resource_request->referrer = relying_party_origin_.GetURL();
    // Since referrer_policy only affects redirects and we disable redirects
    // below, we don't need to set referrer_policy here.
  }
  // TODO(cbiesinger): Not following redirects is important for security because
  // this bypasses CORB. Ensure there is a test added.
  // https://crbug.com/1155312.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->request_initiator = relying_party_origin_;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();
  DCHECK(client_security_state_);
  resource_request->trusted_params->client_security_state =
      client_security_state_.Clone();

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          traffic_annotation);
}

std::unique_ptr<network::SimpleURLLoader>
IdpNetworkRequestManager::CreateCredentialedUrlLoader(
    const GURL& target_url,
    bool send_referrer,
    absl::optional<std::string> request_body) const {
  auto resource_request = CreateCredentialedResourceRequest(
      target_url, send_referrer, relying_party_origin_,
      client_security_state_.Clone());
  if (request_body) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kRequestBodyContentType);
  }

  auto traffic_annotation = CreateTrafficAnnotation();
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  if (request_body)
    loader->AttachStringForUpload(*request_body, kRequestBodyContentType);
  return loader;
}

bool IdpNetworkRequestManager::IsMockIdpNetworkRequestManager() const {
  return false;
}

}  // namespace content
