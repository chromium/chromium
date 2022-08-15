// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/color_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "url/origin.h"

namespace content {

namespace {
using LoginState = IdentityRequestAccount::LoginState;

using AccountList = IdpNetworkRequestManager::AccountList;
using ClientMetadata = IdpNetworkRequestManager::ClientMetadata;
using Endpoints = IdpNetworkRequestManager::Endpoints;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;

// TODO(kenrb): These need to be defined in the explainer or draft spec and
// referenced here.

// Path to find the manifest list on the eTLD+1 host.
constexpr char kManifestListPath[] = "/.well-known/web-identity";

// manifest list JSON keys
constexpr char kProviderUrlListKey[] = "provider_urls";

// fedcm.json configuration keys.
// TODO(crbug.com/1339373): Rename id_token_endpoint to another name.
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

constexpr char kTokenKey[] = "token";

// Revoke request body keys.
constexpr char kClientIdKey[] = "client_id";
constexpr char kRevokeAccountKey[] = "account_id";
constexpr char kRevokeRequestKey[] = "request";

// Body content types.
constexpr char kUrlEncodedContentType[] = "application/x-www-form-urlencoded";
constexpr char kResponseBodyContentType[] = "application/json";

// 1 MiB is an arbitrary upper bound that should account for any reasonable
// response size that is a part of this protocol.
constexpr int maxResponseSizeInKiB = 1024;

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
    url::Origin rp_origin,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  auto target_origin = url::Origin::Create(target_url);
  auto site_for_cookies = net::SiteForCookies::FromOrigin(target_origin);
  AddCsrfHeader(resource_request.get());
  // We set the initiator to nullopt to denote browser-initiated so that this
  // request is considered first-party. We want to send first-party cookies
  // because this is not a real third-party request as it is mediated by the
  // browser, and third-party cookies will be going away with 3pc deprecation,
  // but we still need to send cookies in these requests.
  // We use nullopt instead of target_origin because we want to send a
  // `Sec-Fetch-Site: none` header instead of `Sec-Fetch-Site: same-origin`.
  resource_request->request_initiator = absl::nullopt;
  resource_request->url = target_url;
  resource_request->site_for_cookies = site_for_cookies;
  if (send_referrer) {
    resource_request->referrer = rp_origin.GetURL();
    // Since referrer_policy only affects redirects and we disable redirects
    // below, we don't need to set referrer_policy here.
  }
  // TODO(cbiesinger): Not following redirects is important for security because
  // this bypasses CORB. Ensure there is a test added.
  // https://crbug.com/1155312.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kResponseBodyContentType);

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
                   AccountList& account_list,
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
              icons, brand_icon_ideal_size.value(),
              brand_icon_minimum_size.value(),
              blink::mojom::ManifestImageResource_Purpose::MASKABLE);
    }
  }
}

FetchStatus GetResponseError(std::string* response_body, int response_code) {
  if (response_code == net::HTTP_NOT_FOUND)
    return FetchStatus::kHttpNotFoundError;

  if (!response_body)
    return FetchStatus::kNoResponseError;

  return FetchStatus::kSuccess;
}

FetchStatus GetParsingError(
    const data_decoder::DataDecoder::ValueOrError& result) {
  if (!result.has_value())
    return FetchStatus::kInvalidResponseError;

  auto& response = *result;
  if (!response.is_dict())
    return FetchStatus::kInvalidResponseError;

  return FetchStatus::kSuccess;
}

void OnJsonParsed(
    IdpNetworkRequestManager::ParseJsonCallback parse_json_callback,
    data_decoder::DataDecoder::ValueOrError result) {
  FetchStatus parsing_error = GetParsingError(result);
  std::move(parse_json_callback).Run(parsing_error, std::move(result));
}

void OnDownloadedJson(
    IdpNetworkRequestManager::ParseJsonCallback parse_json_callback,
    std::unique_ptr<std::string> response_body,
    int response_code) {
  FetchStatus response_error =
      GetResponseError(response_body.get(), response_code);

  if (response_error != FetchStatus::kSuccess) {
    std::move(parse_json_callback)
        .Run(response_error, data_decoder::DataDecoder::ValueOrError());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&OnJsonParsed, std::move(parse_json_callback)));
}

void OnManifestListParsed(
    IdpNetworkRequestManager::FetchManifestListCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (callback.IsCancelled())
    return;

  std::set<GURL> urls;

  if (fetch_status != FetchStatus::kSuccess) {
    std::move(callback).Run(fetch_status, urls);
    return;
  }

  const base::Value::Dict* dict = result->GetIfDict();
  if (!dict) {
    std::move(callback).Run(FetchStatus::kInvalidResponseError, urls);
    return;
  }

  const base::Value::List* list = dict->FindList(kProviderUrlListKey);
  if (!list) {
    std::move(callback).Run(FetchStatus::kInvalidResponseError, urls);
    return;
  }

  for (const auto& value : *list) {
    const std::string* url = value.GetIfString();
    if (!url) {
      std::move(callback).Run(FetchStatus::kInvalidResponseError,
                              std::set<GURL>());
      return;
    }
    urls.insert(GURL(*url));
  }

  std::move(callback).Run(FetchStatus::kSuccess, urls);
}

void OnManifestParsed(absl::optional<int> idp_brand_icon_ideal_size,
                      absl::optional<int> idp_brand_icon_minimum_size,
                      IdpNetworkRequestManager::FetchManifestCallback callback,
                      FetchStatus fetch_status,
                      data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status != FetchStatus::kSuccess) {
    std::move(callback).Run(fetch_status, Endpoints(),
                            IdentityProviderMetadata());
    return;
  }

  auto& response = *result;
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

  std::move(callback).Run(FetchStatus::kSuccess, endpoints,
                          std::move(idp_metadata));
}

void OnClientMetadataParsed(
    IdpNetworkRequestManager::FetchClientMetadataCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status != FetchStatus::kSuccess) {
    std::move(callback).Run(fetch_status, ClientMetadata());
    return;
  }

  auto& response = *result;
  auto ExtractUrl = [&](const char* key) {
    const base::Value* endpoint = response.FindKey(key);
    if (!endpoint || !endpoint->is_string()) {
      return std::string();
    }
    return endpoint->GetString();
  };

  IdpNetworkRequestManager::ClientMetadata data;
  data.privacy_policy_url = ExtractUrl(kPrivacyPolicyKey);
  data.terms_of_service_url = ExtractUrl(kTermsOfServiceKey);

  std::move(callback).Run(FetchStatus::kSuccess, data);
}

void OnAccountsRequestParsed(
    std::string client_id,
    IdpNetworkRequestManager::AccountsRequestCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status != FetchStatus::kSuccess) {
    std::move(callback).Run(fetch_status, AccountList());
    return;
  }

  AccountList account_list;
  auto& response = *result;
  const base::Value* accounts = response.FindKey(kAccountsKey);
  bool accounts_present =
      accounts && ParseAccounts(accounts, account_list, client_id);

  if (!accounts_present) {
    std::move(callback).Run(FetchStatus::kInvalidResponseError, AccountList());
    return;
  }

  std::move(callback).Run(FetchStatus::kSuccess, std::move(account_list));
}

void OnTokenRequestParsed(
    IdpNetworkRequestManager::TokenRequestCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status != FetchStatus::kSuccess) {
    std::move(callback).Run(fetch_status, std::string());
    return;
  }

  auto& response = *result;
  const base::Value* token = response.FindKey(kTokenKey);
  bool token_present = token && token->is_string();

  if (!token_present) {
    std::move(callback).Run(FetchStatus::kInvalidResponseError, std::string());
    return;
  }
  std::move(callback).Run(FetchStatus::kSuccess, token->GetString());
}

void OnLogoutCompleted(IdpNetworkRequestManager::LogoutCallback callback,
                       std::unique_ptr<std::string> response_body,
                       int response_code) {
  std::move(callback).Run();
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

// static
absl::optional<GURL> IdpNetworkRequestManager::ComputeManifestListUrl(
    const GURL& provider) {
  GURL manifest_list_url;
  if (net::IsLocalhost(provider)) {
    manifest_list_url = provider.GetWithEmptyPath();
  } else {
    std::string etld_plus_one = GetDomainAndRegistry(
        provider, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (etld_plus_one.empty())
      return absl::nullopt;
    manifest_list_url = GURL(provider.scheme() + "://" + etld_plus_one);
  }

  GURL::Replacements replacements;
  replacements.SetPathStr(kManifestListPath);
  return manifest_list_url.ReplaceComponents(replacements);
}

void IdpNetworkRequestManager::FetchManifestList(
    FetchManifestListCallback callback) {
  absl::optional<GURL> manifest_list_url =
      IdpNetworkRequestManager::ComputeManifestListUrl(provider_);

  if (!manifest_list_url) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&OnManifestListParsed, std::move(callback),
                                  FetchStatus::kHttpNotFoundError,
                                  data_decoder::DataDecoder::ValueOrError()));
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      CreateUncredentialedUrlLoader(*manifest_list_url,
                                    /* send_referrer= */ false,
                                    /* follow_redirects= */ true);
  DownloadJsonAndParse(
      std::move(url_loader),
      base::BindOnce(&OnManifestListParsed, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::FetchManifest(
    absl::optional<int> idp_brand_icon_ideal_size,
    absl::optional<int> idp_brand_icon_minimum_size,
    FetchManifestCallback callback) {
  GURL target_url =
      provider_.Resolve(IdpNetworkRequestManager::kManifestFilePath);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      CreateUncredentialedUrlLoader(target_url, /* send_referrer= */ false);
  DownloadJsonAndParse(
      std::move(url_loader),
      base::BindOnce(&OnManifestParsed, idp_brand_icon_ideal_size,
                     idp_brand_icon_minimum_size, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendAccountsRequest(
    const GURL& accounts_url,
    const std::string& client_id,
    AccountsRequestCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      CreateCredentialedUrlLoader(accounts_url, /* send_referrer= */ false);
  DownloadJsonAndParse(
      std::move(url_loader),
      base::BindOnce(&OnAccountsRequestParsed, client_id, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendTokenRequest(
    const GURL& token_url,
    const std::string& account,
    const std::string& url_encoded_post_data,
    TokenRequestCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      CreateCredentialedUrlLoader(token_url,
                                  /* send_referrer= */ true,
                                  url_encoded_post_data);
  DownloadJsonAndParse(
      std::move(url_loader),
      base::BindOnce(&OnTokenRequestParsed, std::move(callback)),
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

void IdpNetworkRequestManager::SendLogout(const GURL& logout_url,
                                          LogoutCallback callback) {
  // TODO(kenrb): Add browser test verifying that the response to this can
  // clear cookies. https://crbug.com/1155312.

  auto resource_request = CreateCredentialedResourceRequest(
      logout_url, /* send_referrer= */ false, relying_party_origin_,
      client_security_state_.Clone());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept, "*/*");

  auto traffic_annotation = CreateTrafficAnnotation();

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  DownloadUrl(std::move(url_loader),
              base::BindOnce(&OnLogoutCompleted, std::move(callback)),
              maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::DownloadJsonAndParse(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    ParseJsonCallback parse_json_callback,
    size_t max_download_size) {
  DownloadUrl(std::move(url_loader),
              base::BindOnce(&OnDownloadedJson, std::move(parse_json_callback)),
              max_download_size);
}

void IdpNetworkRequestManager::DownloadUrl(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    DownloadCallback callback,
    size_t max_download_size) {
  network::SimpleURLLoader* url_loader_ptr = url_loader.get();
  // Callback is a member of IdpNetworkRequestManager in order to cancel
  // callback if IdpNetworkRequestManager object is destroyed prior to callback
  // being run.
  url_loader_ptr->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnDownloadedUrl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      max_download_size);
}

void IdpNetworkRequestManager::OnDownloadedUrl(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    IdpNetworkRequestManager::DownloadCallback callback,
    std::unique_ptr<std::string> response_body) {
  auto* response_info = url_loader->ResponseInfo();
  int response_code = response_info && response_info->headers
                          ? response_info->headers->response_code()
                          : -1;

  url_loader.reset();
  std::move(callback).Run(std::move(response_body), response_code);
}

void IdpNetworkRequestManager::FetchClientMetadata(
    const GURL& endpoint,
    const std::string& client_id,
    FetchClientMetadataCallback callback) {
  GURL target_url = endpoint.Resolve(
      "?client_id=" + base::EscapeQueryParamValue(client_id, true));

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      CreateUncredentialedUrlLoader(target_url, /* send_referrer= */ true);

  DownloadJsonAndParse(
      std::move(url_loader),
      base::BindOnce(&OnClientMetadataParsed, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

std::unique_ptr<network::SimpleURLLoader>
IdpNetworkRequestManager::CreateUncredentialedUrlLoader(
    const GURL& target_url,
    bool send_referrer,
    bool follow_redirects) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      CreateTrafficAnnotation();

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = target_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kResponseBodyContentType);
  AddCsrfHeader(resource_request.get());
  if (send_referrer) {
    resource_request->referrer = relying_party_origin_.GetURL();
    // Since referrer_policy only affects redirects and we never send a
    // referrer when we follow redirects, we don't need to set referrer_policy
    // here.
    DCHECK(!follow_redirects);
  }
  if (follow_redirects) {
    resource_request->redirect_mode = network::mojom::RedirectMode::kFollow;
  } else {
    // TODO(cbiesinger): Not following redirects is important for security
    // because this bypasses CORB. Ensure there is a test added.
    // https://crbug.com/1155312.
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  }
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
    absl::optional<std::string> url_encoded_post_data) const {
  auto resource_request = CreateCredentialedResourceRequest(
      target_url, send_referrer, relying_party_origin_,
      client_security_state_.Clone());
  if (url_encoded_post_data) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kUrlEncodedContentType);
  }

  auto traffic_annotation = CreateTrafficAnnotation();
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  if (url_encoded_post_data)
    loader->AttachStringForUpload(*url_encoded_post_data,
                                  kUrlEncodedContentType);
  return loader;
}

}  // namespace content
