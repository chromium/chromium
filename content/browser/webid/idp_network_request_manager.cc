// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
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
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace content {

namespace {

using AccountsResponseInvalidReason =
    IdpNetworkRequestManager::AccountsResponseInvalidReason;
using ClientMetadata = IdpNetworkRequestManager::ClientMetadata;
using Endpoints = IdpNetworkRequestManager::Endpoints;
using ErrorDialogType = IdpNetworkRequestManager::FedCmErrorDialogType;
using ErrorUrlType = IdpNetworkRequestManager::FedCmErrorUrlType;
using FetchStatus = IdpNetworkRequestManager::FetchStatus;
using LoginState = IdentityRequestAccount::LoginState;
using ParseStatus = IdpNetworkRequestManager::ParseStatus;
using TokenError = IdentityCredentialTokenError;
using TokenResponseType = IdpNetworkRequestManager::FedCmTokenResponseType;
using TokenResult = IdpNetworkRequestManager::TokenResult;

// TODO(kenrb): These need to be defined in the explainer or draft spec and
// referenced here.

// Path to find the well-known file on the eTLD+1 host.
constexpr char kWellKnownPath[] = "/.well-known/web-identity";

// Well-known file JSON keys
constexpr char kProviderUrlListKey[] = "provider_urls";

// fedcm.json configuration keys.
constexpr char kIdAssertionEndpoint[] = "id_assertion_endpoint";
constexpr char kClientMetadataEndpointKey[] = "client_metadata_endpoint";
constexpr char kMetricsEndpoint[] = "metrics_endpoint";
constexpr char kDisconnectEndpoint[] = "disconnect_endpoint";
constexpr char kModesKey[] = "modes";
constexpr char kTypesKey[] = "types";

// Keys in the 'accounts' dictionary
constexpr char kIncludeKey[] = "include";

// Keys in the 'modes' dictionary.
constexpr char kActiveModeKey[] = "active";
constexpr char kPassiveModeKey[] = "passive";

// Keys in the specific mode dictionary.
constexpr char kSupportsUseOtherAccountKey[] = "supports_use_other_account";

// Shared between the well-known files and config files
constexpr char kAccountsEndpointKey[] = "accounts_endpoint";
constexpr char kLoginUrlKey[] = "login_url";

// Keys in fedcm.json 'branding' dictionary.
constexpr char kIdpBrandingBackgroundColorKey[] = "background_color";
constexpr char kIdpBrandingForegroundColorKey[] = "color";

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
constexpr char kHintsKey[] = "login_hints";
constexpr char kDomainHintsKey[] = "domain_hints";
constexpr char kLabelsKey[] = "labels";

// Keys in 'branding' 'icons' dictionary in config for the IDP icon and client
// metadata endpoint for the RP icon.
constexpr char kBrandingIconsKey[] = "icons";
constexpr char kBrandingIconUrl[] = "url";
constexpr char kBrandingIconSize[] = "size";

// The id assertion endpoint contains a token result.
constexpr char kTokenKey[] = "token";
// The id assertion endpoint contains a URL, which indicates that
// the serve wants to direct the user to continue on a pop-up
// window before it provides a token result.
constexpr char kContinueOnKey[] = "continue_on";
// The id assertion endpoint may contain an error dict containing a code and url
// which describes the error.
constexpr char kErrorKey[] = "error";
constexpr char kErrorCodeKey[] = "code";
constexpr char kErrorUrlKey[] = "url";

// Body content types.
constexpr char kUrlEncodedContentType[] = "application/x-www-form-urlencoded";
constexpr char kPlusJson[] = "+json";
constexpr char kApplicationJson[] = "application/json";
constexpr char kTextJson[] = "text/json";

// Error API codes.
constexpr char kGenericEmpty[] = "";
constexpr char kInvalidRequest[] = "invalid_request";
constexpr char kUnauthorizedClient[] = "unauthorized_client";
constexpr char kAccessDenied[] = "access_denied";
constexpr char kTemporarilyUnavailable[] = "temporarily_unavailable";
constexpr char kServerError[] = "server_error";

// Disconnect response keys.
constexpr char kDisconnectAccountId[] = "account_id";

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

GURL ResolveConfigUrl(const GURL& config_url, const std::string& endpoint) {
  if (endpoint.empty())
    return GURL();
  return config_url.Resolve(endpoint);
}

GURL ExtractUrl(const base::Value::Dict& response, const char* key) {
  const std::string* response_url = response.FindString(key);
  if (!response_url) {
    return GURL();
  }
  GURL url = GURL(*response_url);
  // TODO(crbug.com/40280145): Allow localhost URLs
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return GURL();
  }
  return url;
}

std::string ExtractString(const base::Value::Dict& response, const char* key) {
  const std::string* str = response.FindString(key);
  if (!str) {
    return "";
  }
  return *str;
}

IdentityRequestAccountPtr ParseAccount(const base::Value::Dict& account,
                                       const std::string& client_id) {
  auto* id = account.FindString(kAccountIdKey);
  auto* email = account.FindString(kAccountEmailKey);
  auto* name = account.FindString(kAccountNameKey);
  auto* given_name = account.FindString(kAccountGivenNameKey);
  auto* picture = account.FindString(kAccountPictureKey);
  auto* approved_clients = account.FindList(kAccountApprovedClientsKey);
  std::vector<std::string> account_hints;
  auto* hints = account.FindList(kHintsKey);
  if (hints) {
    for (const base::Value& entry : *hints) {
      if (entry.is_string()) {
        account_hints.emplace_back(entry.GetString());
      }
    }
  }
  std::vector<std::string> domain_hints;
  auto* domain_hints_list = account.FindList(kDomainHintsKey);
  if (domain_hints_list) {
    for (const base::Value& entry : *domain_hints_list) {
      if (entry.is_string()) {
        domain_hints.emplace_back(entry.GetString());
      }
    }
  }

  std::vector<std::string> labels;
  auto* labels_list = account.FindList(kLabelsKey);
  if (labels_list) {
    for (const base::Value& entry : *labels_list) {
      if (entry.is_string()) {
        labels.emplace_back(entry.GetString());
      }
    }
  }

  // required fields
  if (!(id && email && name)) {
    return nullptr;
  }

  auto trimmed_email =
      base::TrimWhitespace(base::UTF8ToUTF16(*email), base::TRIM_ALL);
  auto trimmed_name =
      base::TrimWhitespace(base::UTF8ToUTF16(*name), base::TRIM_ALL);
  // TODO(crbug.com/40849405): validate email address.
  if (trimmed_email.empty() || trimmed_name.empty()) {
    return nullptr;
  }

  RecordApprovedClientsExistence(approved_clients != nullptr);

  std::optional<LoginState> approved_value;
  if (approved_clients) {
    for (const base::Value& entry : *approved_clients) {
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
    RecordApprovedClientsSize(approved_clients->size());
  }

  return base::MakeRefCounted<IdentityRequestAccount>(
      *id, *email, *name, given_name ? *given_name : "",
      picture ? GURL(*picture) : GURL(), std::move(account_hints),
      std::move(domain_hints), std::move(labels), approved_value,
      /*browser_trusted_login_state=*/LoginState::kSignUp);
}

// Parses accounts from given Value. Returns true if parse is successful and
// adds parsed accounts to the |account_list|.
bool ParseAccounts(const base::Value::List& accounts,
                   std::vector<IdentityRequestAccountPtr>& account_list,
                   const std::string& client_id,
                   AccountsResponseInvalidReason& parsing_error) {
  DCHECK(account_list.empty());

  base::flat_set<std::string> account_ids;
  for (auto& account : accounts) {
    const base::Value::Dict* account_dict = account.GetIfDict();
    if (!account_dict) {
      parsing_error = AccountsResponseInvalidReason::kAccountIsNotDict;
      return false;
    }

    IdentityRequestAccountPtr parsed_account =
        ParseAccount(*account_dict, client_id);
    if (parsed_account) {
      if (account_ids.count(parsed_account->id)) {
        parsing_error = AccountsResponseInvalidReason::kAccountsShareSameId;
        return false;
      }
      account_ids.insert(parsed_account->id);
      account_list.push_back(std::move(parsed_account));
    } else {
      parsing_error =
          AccountsResponseInvalidReason::kAccountMissesRequiredField;
      return false;
    }
  }

  DCHECK(!account_list.empty());
  return true;
}

std::optional<SkColor> ParseCssColor(const std::string* value) {
  if (value == nullptr)
    return std::nullopt;

  SkColor color;
  if (!ParseCssColorString(*value, &color)) {
    return std::nullopt;
  }

  return SkColorSetA(color, 0xff);
}

GURL FindBestMatchingIconUrl(const base::Value::List* icons_value,
                             int brand_icon_ideal_size,
                             int brand_icon_minimum_size) {
  std::vector<blink::Manifest::ImageResource> icons;
  for (const base::Value& icon_value : *icons_value) {
    const base::Value::Dict* icon_value_dict = icon_value.GetIfDict();
    if (!icon_value_dict) {
      continue;
    }

    const std::string* icon_src = icon_value_dict->FindString(kBrandingIconUrl);
    if (!icon_src) {
      continue;
    }

    blink::Manifest::ImageResource icon;
    icon.src = GURL(*icon_src);
    if (!icon.src.is_valid() || !icon.src.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::MASKABLE};

    std::optional<int> icon_size = icon_value_dict->FindInt(kBrandingIconSize);
    int icon_size_int = icon_size.value_or(0);
    icon.sizes.emplace_back(icon_size_int, icon_size_int);

    icons.push_back(icon);
  }

  return blink::ManifestIconSelector::FindBestMatchingSquareIcon(
      icons, brand_icon_ideal_size, brand_icon_minimum_size,
      blink::mojom::ManifestImageResource_Purpose::MASKABLE);
}

// Parse IdentityProviderMetadata from given value. Overwrites |idp_metadata|
// with the parsed value.
void ParseIdentityProviderMetadata(const base::Value::Dict& idp_metadata_value,
                                   int brand_icon_ideal_size,
                                   int brand_icon_minimum_size,
                                   IdentityProviderMetadata& idp_metadata) {
  idp_metadata.brand_background_color = ParseCssColor(
      idp_metadata_value.FindString(kIdpBrandingBackgroundColorKey));
  idp_metadata.brand_text_color = ParseCssColor(
      idp_metadata_value.FindString(kIdpBrandingForegroundColorKey));

  const base::Value::List* icons_value =
      idp_metadata_value.FindList(kBrandingIconsKey);
  if (!icons_value) {
    return;
  }

  idp_metadata.brand_icon_url = FindBestMatchingIconUrl(
      icons_value, brand_icon_ideal_size, brand_icon_minimum_size);
}

// This method follows https://mimesniff.spec.whatwg.org/#json-mime-type.
bool IsJsonMimeType(const std::string& mime_type) {
  if (base::EndsWith(mime_type, kPlusJson)) {
    return true;
  }

  return mime_type == kApplicationJson || mime_type == kTextJson;
}

ParseStatus GetResponseError(std::string* response_body,
                             int response_code,
                             const std::string& mime_type) {
  if (response_code == net::HTTP_NOT_FOUND) {
    return ParseStatus::kHttpNotFoundError;
  }

  if (!response_body) {
    return ParseStatus::kNoResponseError;
  }

  if (!IsJsonMimeType(mime_type)) {
    return ParseStatus::kInvalidContentTypeError;
  }

  return ParseStatus::kSuccess;
}

ParseStatus GetParsingError(
    const data_decoder::DataDecoder::ValueOrError& result) {
  if (!result.has_value())
    return ParseStatus::kInvalidResponseError;

  return result->GetIfDict() ? ParseStatus::kSuccess
                             : ParseStatus::kInvalidResponseError;
}

void OnJsonParsed(
    IdpNetworkRequestManager::ParseJsonCallback parse_json_callback,
    int response_code,
    data_decoder::DataDecoder::ValueOrError result) {
  ParseStatus parse_status = GetParsingError(result);
  std::move(parse_json_callback)
      .Run({parse_status, response_code}, std::move(result));
}

void OnDownloadedJson(
    IdpNetworkRequestManager::ParseJsonCallback parse_json_callback,
    std::unique_ptr<std::string> response_body,
    int response_code,
    const std::string& mime_type) {
  ParseStatus parse_status =
      GetResponseError(response_body.get(), response_code, mime_type);

  if (parse_status != ParseStatus::kSuccess) {
    std::move(parse_json_callback)
        .Run({parse_status, response_code},
             data_decoder::DataDecoder::ValueOrError());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&OnJsonParsed, std::move(parse_json_callback),
                     response_code));
}

GURL ExtractEndpoint(const GURL& provider,
                     const base::Value::Dict& response,
                     const char* key) {
  const std::string* endpoint = response.FindString(key);
  if (!endpoint) {
    return GURL();
  }
  return ResolveConfigUrl(provider, *endpoint);
}

void OnWellKnownParsed(
    IdpNetworkRequestManager::FetchWellKnownCallback callback,
    const GURL& well_known_url,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (callback.IsCancelled())
    return;

  IdpNetworkRequestManager::WellKnown well_known;
  std::set<GURL> urls;

  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    std::move(callback).Run(fetch_status, std::move(well_known));
    return;
  }

  const base::Value::Dict* dict = result->GetIfDict();
  if (!dict) {
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        std::move(well_known));
    return;
  }

  well_known.accounts =
      ExtractEndpoint(well_known_url, *dict, kAccountsEndpointKey);
  well_known.login_url = ExtractEndpoint(well_known_url, *dict, kLoginUrlKey);

  const base::Value::List* list = dict->FindList(kProviderUrlListKey);
  if (!list) {
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        std::move(well_known));
    return;
  }

  if (list->empty()) {
    std::move(callback).Run(
        {ParseStatus::kEmptyListError, fetch_status.response_code},
        std::move(well_known));
    return;
  }

  for (const auto& value : *list) {
    const std::string* url_str = value.GetIfString();
    if (!url_str) {
      std::move(callback).Run(
          {ParseStatus::kInvalidResponseError, fetch_status.response_code},
          std::move(well_known));
      return;
    }
    GURL url(*url_str);
    if (!url.is_valid()) {
      url = well_known_url.Resolve(*url_str);
    }
    urls.insert(url);
  }

  well_known.provider_urls = std::move(urls);

  std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                          std::move(well_known));
}

void OnConfigParsed(const GURL& provider,
                    blink::mojom::RpMode rp_mode,
                    int idp_brand_icon_ideal_size,
                    int idp_brand_icon_minimum_size,
                    IdpNetworkRequestManager::FetchConfigCallback callback,
                    FetchStatus fetch_status,
                    data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    std::move(callback).Run(fetch_status, Endpoints(),
                            IdentityProviderMetadata());
    return;
  }

  const base::Value::Dict& response = result->GetDict();

  Endpoints endpoints;
  endpoints.token = ExtractEndpoint(provider, response, kIdAssertionEndpoint);
  endpoints.accounts =
      ExtractEndpoint(provider, response, kAccountsEndpointKey);
  endpoints.client_metadata =
      ExtractEndpoint(provider, response, kClientMetadataEndpointKey);
  endpoints.metrics = ExtractEndpoint(provider, response, kMetricsEndpoint);
  endpoints.disconnect =
      ExtractEndpoint(provider, response, kDisconnectEndpoint);

  const base::Value::Dict* idp_metadata_value =
      response.FindDict(kIdpBrandingKey);
  IdentityProviderMetadata idp_metadata;
  idp_metadata.config_url = provider;
  if (idp_metadata_value) {
    ParseIdentityProviderMetadata(*idp_metadata_value,
                                  idp_brand_icon_ideal_size,
                                  idp_brand_icon_minimum_size, idp_metadata);
  }
  idp_metadata.idp_login_url =
      ExtractEndpoint(provider, response, kLoginUrlKey);
  if (IsFedCmIdPRegistrationEnabled()) {
    const base::Value::List* types = response.FindList(kTypesKey);
    if (types) {
      for (const auto& type : *types) {
        if (type.is_string()) {
          idp_metadata.types.push_back(type.GetString());
        }
      }
    }
  }

  const base::Value::Dict* accounts_dict = response.FindDict(kAccountsKey);
  if (accounts_dict) {
    const std::string* requested_label = accounts_dict->FindString(kIncludeKey);
    if (requested_label) {
      idp_metadata.requested_label = *requested_label;
    }
  }

  if (IsFedCmUseOtherAccountEnabled(rp_mode == blink::mojom::RpMode::kActive)) {
    const base::Value::Dict* modes_dict = response.FindDict(kModesKey);
    const base::Value::Dict* selected_mode_dict = nullptr;
    if (modes_dict) {
      switch (rp_mode) {
        case blink::mojom::RpMode::kPassive:
          selected_mode_dict = modes_dict->FindDict(kPassiveModeKey);
          break;
        case blink::mojom::RpMode::kActive:
          selected_mode_dict = modes_dict->FindDict(kActiveModeKey);
          break;
      };
    }
    std::optional<bool> supports_add_account =
        selected_mode_dict
            ? selected_mode_dict->FindBool(kSupportsUseOtherAccountKey)
            : std::nullopt;
    if (supports_add_account) {
      idp_metadata.supports_add_account = *supports_add_account;
    }
  }
  std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                          endpoints, std::move(idp_metadata));
}

void OnClientMetadataParsed(
    int rp_brand_icon_ideal_size,
    int rp_brand_icon_minimum_size,
    IdpNetworkRequestManager::FetchClientMetadataCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    std::move(callback).Run(fetch_status, ClientMetadata());
    return;
  }

  IdpNetworkRequestManager::ClientMetadata data;
  const base::Value::Dict& response = result->GetDict();
  data.privacy_policy_url = ExtractUrl(response, kPrivacyPolicyKey);
  data.terms_of_service_url = ExtractUrl(response, kTermsOfServiceKey);

  const base::Value::List* icons_value = response.FindList(kBrandingIconsKey);
  if (icons_value) {
    data.brand_icon_url = FindBestMatchingIconUrl(
        icons_value, rp_brand_icon_ideal_size, rp_brand_icon_minimum_size);
  }

  std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                          data);
}

void OnAccountsRequestParsed(
    std::string client_id,
    IdpNetworkRequestManager::AccountsRequestCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  std::vector<IdentityRequestAccountPtr> account_list;
  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    RecordAccountsResponseInvalidReason(
        AccountsResponseInvalidReason::kResponseIsNotJsonOrDict);
    std::move(callback).Run(fetch_status, account_list);
    return;
  }

  const base::Value::Dict& response = result->GetDict();
  const base::Value::List* accounts = response.FindList(kAccountsKey);

  if (!accounts) {
    RecordAccountsResponseInvalidReason(
        AccountsResponseInvalidReason::kNoAccountsKey);
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        account_list);
    return;
  }

  if (accounts->empty()) {
    RecordAccountsResponseInvalidReason(
        AccountsResponseInvalidReason::kAccountListIsEmpty);
    std::move(callback).Run(
        {ParseStatus::kEmptyListError, fetch_status.response_code},
        account_list);
    return;
  }

  AccountsResponseInvalidReason parsing_error =
      AccountsResponseInvalidReason::kResponseIsNotJsonOrDict;
  bool accounts_valid =
      ParseAccounts(*accounts, account_list, client_id, parsing_error);

  if (!accounts_valid) {
    CHECK_NE(parsing_error,
             AccountsResponseInvalidReason::kResponseIsNotJsonOrDict);
    RecordAccountsResponseInvalidReason(parsing_error);
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        std::vector<IdentityRequestAccountPtr>());
    return;
  }

  std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                          std::move(account_list));
}

std::pair<GURL, std::optional<ErrorUrlType>> GetErrorUrlAndType(
    const std::string* url,
    const GURL& idp_url) {
  if (!url || url->empty()) {
    return std::make_pair(GURL(), std::nullopt);
  }

  GURL error_url = idp_url.Resolve(*url);
  if (!error_url.is_valid()) {
    return std::make_pair(GURL(), std::nullopt);
  }

  url::Origin error_origin = url::Origin::Create(error_url);
  url::Origin idp_origin = url::Origin::Create(idp_url);
  if (error_origin == idp_origin) {
    return std::make_pair(error_url, ErrorUrlType::kSameOrigin);
  }

  if (!webid::IsSameSite(error_origin, idp_origin)) {
    return std::make_pair(GURL(), ErrorUrlType::kCrossSite);
  }

  return std::make_pair(error_url, ErrorUrlType::kCrossOriginSameSite);
}

ErrorDialogType GetErrorDialogType(const std::string& code, const GURL& url) {
  bool has_url = !url.is_empty();
  if (code == kGenericEmpty) {
    return has_url ? ErrorDialogType::kGenericEmptyWithUrl
                   : ErrorDialogType::kGenericEmptyWithoutUrl;
  } else if (code == kInvalidRequest) {
    return has_url ? ErrorDialogType::kInvalidRequestWithUrl
                   : ErrorDialogType::kInvalidRequestWithoutUrl;
  } else if (code == kUnauthorizedClient) {
    return has_url ? ErrorDialogType::kUnauthorizedClientWithUrl
                   : ErrorDialogType::kUnauthorizedClientWithoutUrl;
  } else if (code == kAccessDenied) {
    return has_url ? ErrorDialogType::kAccessDeniedWithUrl
                   : ErrorDialogType::kAccessDeniedWithoutUrl;
  } else if (code == kTemporarilyUnavailable) {
    return has_url ? ErrorDialogType::kTemporarilyUnavailableWithUrl
                   : ErrorDialogType::kTemporarilyUnavailableWithoutUrl;
  } else if (code == kServerError) {
    return has_url ? ErrorDialogType::kServerErrorWithUrl
                   : ErrorDialogType::kServerErrorWithoutUrl;
  }
  return has_url ? ErrorDialogType::kGenericNonEmptyWithUrl
                 : ErrorDialogType::kGenericNonEmptyWithoutUrl;
}

TokenResponseType GetTokenResponseType(const std::string* token,
                                       const std::string* continue_on,
                                       const base::Value::Dict* error) {
  if (token && error && !continue_on) {
    return TokenResponseType::
        kTokenReceivedAndErrorReceivedAndContinueOnNotReceived;
  } else if (token && !error && !continue_on) {
    return TokenResponseType::
        kTokenReceivedAndErrorNotReceivedAndContinueOnNotReceived;
  } else if (!token && error && !continue_on) {
    return TokenResponseType::
        kTokenNotReceivedAndErrorReceivedAndContinueOnNotReceived;
  } else if (token && !error && continue_on) {
    return TokenResponseType::
        kTokenReceivedAndErrorNotReceivedAndContinueOnReceived;
  } else if (token && error && continue_on) {
    return TokenResponseType::
        kTokenReceivedAndErrorReceivedAndContinueOnReceived;
  } else if (!token && !error && continue_on) {
    return TokenResponseType::
        kTokenNotReceivedAndErrorNotReceivedAndContinueOnReceived;
  } else if (!token && error && continue_on) {
    return TokenResponseType::
        kTokenNotReceivedAndErrorReceivedAndContinueOnReceived;
  }
  DCHECK(!token);
  DCHECK(!error);
  DCHECK(!continue_on);
  return TokenResponseType::
      kTokenNotReceivedAndErrorNotReceivedAndContinueOnNotReceived;
}

bool IsOkResponseCode(int response_code) {
  return response_code / 100 == 2;
}

ErrorDialogType GetErrorDialogTypeAndSetTokenError(int response_code,
                                                   TokenResult& token_result) {
  if (response_code == net::HTTP_INTERNAL_SERVER_ERROR) {
    token_result.error = TokenError{kServerError, GURL()};
    return ErrorDialogType::kServerErrorWithoutUrl;
  }
  if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
    token_result.error = TokenError{kTemporarilyUnavailable, GURL()};
    return ErrorDialogType::kTemporarilyUnavailableWithoutUrl;
  }
  token_result.error = TokenError{kGenericEmpty, GURL()};
  return ErrorDialogType::kGenericEmptyWithoutUrl;
}

void OnTokenRequestParsed(
    IdpNetworkRequestManager::TokenRequestCallback callback,
    IdpNetworkRequestManager::ContinueOnCallback continue_on_callback,
    IdpNetworkRequestManager::RecordErrorMetricsCallback
        record_error_metrics_callback,
    const GURL& token_url,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  TokenResult token_result;

  bool parse_succeeded = fetch_status.parse_status == ParseStatus::kSuccess;
  DCHECK(!parse_succeeded || result.has_value());

  // We need to handle a number of cases, in order:
  // 1) Result has a custom error field - return error
  // 2) Parsing the result failed - return error
  // 3) HTTP error code - return error
  // 4) Result has token - return success
  // 5) Result has continue_on URL - return success
  // 6) Neither token nor continue_on nor HTTP error - return error

  const base::Value::Dict* response =
      parse_succeeded ? &result->GetDict() : nullptr;
  bool can_use_response =
      response && IsOkResponseCode(fetch_status.response_code);
  const std::string* token =
      can_use_response ? response->FindString(kTokenKey) : nullptr;
  // continue_on_callback is only set if authz is enabled.
  const std::string* continue_on = can_use_response && continue_on_callback
                                       ? response->FindString(kContinueOnKey)
                                       : nullptr;
  const base::Value::Dict* response_error =
      response ? response->FindDict(kErrorKey) : nullptr;
  TokenResponseType token_response_type =
      GetTokenResponseType(token, continue_on, response_error);

  if (response_error) {
    std::string error_code = ExtractString(*response_error, kErrorCodeKey);
    const std::string* url = response_error->FindString(kErrorUrlKey);
    GURL error_url;
    std::optional<ErrorUrlType> error_url_type;
    std::tie(error_url, error_url_type) = GetErrorUrlAndType(url, token_url);
    token_result.error = TokenError{error_code, error_url};
    std::move(record_error_metrics_callback)
        .Run(token_response_type, GetErrorDialogType(error_code, error_url),
             error_url_type);
    std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                            token_result);
    return;
  }

  if (!parse_succeeded || !IsOkResponseCode(fetch_status.response_code)) {
    ErrorDialogType type = GetErrorDialogTypeAndSetTokenError(
        fetch_status.response_code, token_result);
    std::move(record_error_metrics_callback)
        .Run(token_response_type, type, /*error_url_type=*/std::nullopt);
    if (parse_succeeded) {
      fetch_status.parse_status = ParseStatus::kInvalidResponseError;
    }
    std::move(callback).Run(fetch_status, token_result);
    return;
  }
  DCHECK(response);

  if (token) {
    token_result.token = *token;
    std::move(record_error_metrics_callback)
        .Run(token_response_type, /*error_dialog_type=*/std::nullopt,
             /*error_url_type=*/std::nullopt);
    std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                            token_result);
    return;
  }

  if (continue_on) {
    GURL url = token_url.Resolve(*continue_on);
    if (url.is_valid()) {
      std::move(record_error_metrics_callback)
          .Run(token_response_type, /*error_dialog_type=*/std::nullopt,
               /*error_url_type=*/std::nullopt);
      std::move(continue_on_callback)
          .Run({ParseStatus::kSuccess, fetch_status.response_code},
               std::move(url));
      return;
    }
  }

  ErrorDialogType type = GetErrorDialogTypeAndSetTokenError(
      fetch_status.response_code, token_result);
  std::move(record_error_metrics_callback)
      .Run(token_response_type, type, /*error_url_type=*/std::nullopt);
  std::move(callback).Run(
      {ParseStatus::kInvalidResponseError, fetch_status.response_code},
      token_result);
}

void OnLogoutCompleted(IdpNetworkRequestManager::LogoutCallback callback,
                       std::unique_ptr<std::string> response_body,
                       int response_code,
                       const std::string& mime_type) {
  std::move(callback).Run();
}

void OnDisconnectResponseParsed(
    IdpNetworkRequestManager::DisconnectCallback callback,
    FetchStatus fetch_status,
    data_decoder::DataDecoder::ValueOrError result) {
  if (fetch_status.parse_status != ParseStatus::kSuccess) {
    std::move(callback).Run(fetch_status, /*account_id=*/"");
    return;
  }

  const base::Value::Dict& response = result->GetDict();
  const std::string* account_id = response.FindString(kDisconnectAccountId);

  if (account_id && !account_id->empty()) {
    std::move(callback).Run(fetch_status, *account_id);
    return;
  }

  std::move(callback).Run(
      {ParseStatus::kInvalidResponseError, fetch_status.response_code},
      /*account_id=*/"");
}

}  // namespace

IdpNetworkRequestManager::Endpoints::Endpoints() = default;
IdpNetworkRequestManager::Endpoints::~Endpoints() = default;
IdpNetworkRequestManager::Endpoints::Endpoints(const Endpoints& other) =
    default;

IdpNetworkRequestManager::WellKnown::WellKnown() = default;
IdpNetworkRequestManager::WellKnown::~WellKnown() = default;
IdpNetworkRequestManager::WellKnown::WellKnown(const WellKnown& other) =
    default;

IdpNetworkRequestManager::TokenResult::TokenResult() = default;
IdpNetworkRequestManager::TokenResult::~TokenResult() = default;
IdpNetworkRequestManager::TokenResult::TokenResult(const TokenResult& other) =
    default;

// static
std::unique_ptr<IdpNetworkRequestManager> IdpNetworkRequestManager::Create(
    RenderFrameHostImpl* host) {
  // Use the browser process URL loader factory because it has cross-origin
  // read blocking disabled. This is safe because even though these are
  // renderer-initiated fetches, the browser parses the responses and does not
  // leak the values to the renderer. The renderer should only learn information
  // when the user selects an account to sign in.
  return std::make_unique<IdpNetworkRequestManager>(
      host->GetLastCommittedOrigin(),
      host->GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess(),
      host->BuildClientSecurityState());
}

IdpNetworkRequestManager::IdpNetworkRequestManager(
    const url::Origin& relying_party_origin,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    network::mojom::ClientSecurityStatePtr client_security_state)
    : relying_party_origin_(relying_party_origin),
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
std::optional<GURL> IdpNetworkRequestManager::ComputeWellKnownUrl(
    const GURL& provider) {
  GURL well_known_url;
  if (net::IsLocalhost(provider)) {
    well_known_url = provider.GetWithEmptyPath();
  } else {
    std::string etld_plus_one = GetDomainAndRegistry(
        provider, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (etld_plus_one.empty())
      return std::nullopt;
    well_known_url = GURL(provider.scheme() + "://" + etld_plus_one);
  }

  GURL::Replacements replacements;
  replacements.SetPathStr(kWellKnownPath);
  return well_known_url.ReplaceComponents(replacements);
}

void IdpNetworkRequestManager::FetchWellKnown(const GURL& provider,
                                              FetchWellKnownCallback callback) {
  std::optional<GURL> well_known_url =
      IdpNetworkRequestManager::ComputeWellKnownUrl(provider);

  if (!well_known_url) {
    // Pass net::HTTP_OK as the |response_code| so we do not add a console error
    // message about a fetch we didn't even attempt.
    FetchStatus fetch_status = {ParseStatus::kHttpNotFoundError, net::HTTP_OK};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&OnWellKnownParsed, std::move(callback),
                                  /*well_known_url=*/GURL(), fetch_status,
                                  data_decoder::DataDecoder::ValueOrError()));
    return;
  }

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(*well_known_url,
                                          /*send_origin=*/false,
                                          /* follow_redirects= */ true);
  DownloadJsonAndParse(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnWellKnownParsed, std::move(callback), *well_known_url),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::FetchConfig(const GURL& provider,
                                           blink::mojom::RpMode rp_mode,
                                           int idp_brand_icon_ideal_size,
                                           int idp_brand_icon_minimum_size,
                                           FetchConfigCallback callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(provider,
                                          /* send_origin= */ false);
  DownloadJsonAndParse(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnConfigParsed, provider, rp_mode,
                     idp_brand_icon_ideal_size, idp_brand_icon_minimum_size,
                     std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendAccountsRequest(
    const GURL& accounts_url,
    const std::string& client_id,
    AccountsRequestCallback callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCredentialedResourceRequest(
          accounts_url, CredentialedResourceRequestType::kNoOrigin);
  DownloadJsonAndParse(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnAccountsRequestParsed, client_id, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendTokenRequest(
    const GURL& token_url,
    const std::string& account,
    const std::string& url_encoded_post_data,
    TokenRequestCallback callback,
    ContinueOnCallback continue_on,
    RecordErrorMetricsCallback record_error_metrics_callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCredentialedResourceRequest(
          token_url,
          base::FeatureList::IsEnabled(features::kFedCmIdAssertionCORS)
              ? CredentialedResourceRequestType::kOriginWithCORS
              : CredentialedResourceRequestType::kOriginWithoutCORS);
  DownloadJsonAndParse(
      std::move(resource_request), url_encoded_post_data,
      base::BindOnce(&OnTokenRequestParsed, std::move(callback),
                     std::move(continue_on),
                     std::move(record_error_metrics_callback), token_url),
      maxResponseSizeInKiB * 1024,
      // We should parse the response body for the ID assertion endpoint request
      // even if the response code is non-2xx because the server may include the
      // error details with the Error API.
      /*allow_http_error_results=*/true);
}

void IdpNetworkRequestManager::SendSuccessfulTokenRequestMetrics(
    const GURL& metrics_endpoint_url,
    base::TimeDelta api_call_to_show_dialog_time,
    base::TimeDelta show_dialog_to_continue_clicked_time,
    base::TimeDelta account_selected_to_token_response_time,
    base::TimeDelta api_call_to_token_response_time) {
  std::string url_encoded_post_data = base::StringPrintf(
      "time_to_show_ui=%d"
      "&time_to_continue=%d"
      "&time_to_receive_token=%d"
      "&turnaround_time=%d",
      static_cast<int>(api_call_to_show_dialog_time.InMilliseconds()),
      static_cast<int>(show_dialog_to_continue_clicked_time.InMilliseconds()),
      static_cast<int>(
          account_selected_to_token_response_time.InMilliseconds()),
      static_cast<int>(api_call_to_token_response_time.InMilliseconds()));

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCredentialedResourceRequest(
          metrics_endpoint_url,
          CredentialedResourceRequestType::kOriginWithoutCORS);
  DownloadJsonAndParse(std::move(resource_request), url_encoded_post_data,
                       IdpNetworkRequestManager::ParseJsonCallback(),
                       maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint_url,
    MetricsEndpointErrorCode error_code) {
  std::string url_encoded_post_data =
      base::StringPrintf("error_code=%d", static_cast<int>(error_code));
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(metrics_endpoint_url,
                                          /*send_origin=*/false);

  DownloadJsonAndParse(std::move(resource_request), url_encoded_post_data,
                       IdpNetworkRequestManager::ParseJsonCallback(),
                       maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendLogout(const GURL& logout_url,
                                          LogoutCallback callback) {
  // TODO(kenrb): Add browser test verifying that the response to this can
  // clear cookies. https://crbug.com/1155312.

  auto resource_request = CreateCredentialedResourceRequest(
      logout_url, CredentialedResourceRequestType::kNoOrigin);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept, "*/*");

  DownloadUrl(std::move(resource_request),
              /*url_encoded_post_data=*/std::nullopt,
              base::BindOnce(&OnLogoutCompleted, std::move(callback)),
              maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendDisconnectRequest(
    const GURL& disconnect_url,
    const std::string& account_hint,
    const std::string& client_id,
    DisconnectCallback callback) {
  auto resource_request = CreateCredentialedResourceRequest(
      disconnect_url, CredentialedResourceRequestType::kOriginWithCORS);
  std::string url_encoded_post_data =
      "client_id=" + client_id + "&account_hint=" + account_hint;
  DownloadJsonAndParse(
      std::move(resource_request), url_encoded_post_data,
      base::BindOnce(&OnDisconnectResponseParsed, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::DownloadAndDecodeImage(const GURL& url,
                                                      ImageCallback callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(url, /*send_origin=*/false);

  DownloadUrl(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&IdpNetworkRequestManager::OnDownloadedImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::DownloadJsonAndParse(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::optional<std::string> url_encoded_post_data,
    ParseJsonCallback parse_json_callback,
    size_t max_download_size,
    bool allow_http_error_results) {
  DownloadUrl(std::move(resource_request), std::move(url_encoded_post_data),
              base::BindOnce(&OnDownloadedJson, std::move(parse_json_callback)),
              max_download_size, allow_http_error_results);
}

void IdpNetworkRequestManager::DownloadUrl(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::optional<std::string> url_encoded_post_data,
    DownloadCallback callback,
    size_t max_download_size,
    bool allow_http_error_results) {
  if (url_encoded_post_data) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kUrlEncodedContentType);
  }
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       CreateTrafficAnnotation());
  if (url_encoded_post_data) {
    url_loader->AttachStringForUpload(*url_encoded_post_data,
                                      kUrlEncodedContentType);
    if (allow_http_error_results) {
      url_loader->SetAllowHttpErrorResults(true);
    }
  }

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
  // Use the HTTP response code, if available. If it is not available, use the
  // NetError(). Note that it is acceptable to put these in the same int because
  // NetErrors are not positive, so they do not conflict with HTTP error codes.
  int response_code = response_info && response_info->headers
                          ? response_info->headers->response_code()
                          : url_loader->NetError();
  std::string mime_type;
  if (response_info && response_info->headers) {
    response_info->headers->GetMimeType(&mime_type);
  }

  url_loader.reset();
  std::move(callback).Run(std::move(response_body), response_code,
                          std::move(mime_type));
}

void IdpNetworkRequestManager::FetchClientMetadata(
    const GURL& endpoint,
    const std::string& client_id,
    int rp_brand_icon_ideal_size,
    int rp_brand_icon_minimum_size,
    FetchClientMetadataCallback callback) {
  GURL target_url = endpoint.Resolve(
      "?client_id=" + base::EscapeQueryParamValue(client_id, true));

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(target_url,
                                          /* send_origin= */ true);

  DownloadJsonAndParse(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnClientMetadataParsed, rp_brand_icon_ideal_size,
                     rp_brand_icon_minimum_size, std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::OnDownloadedImage(
    ImageCallback callback,
    std::unique_ptr<std::string> response_body,
    int response_code,
    const std::string& mime_type) {
  if (!response_body || response_code != net::HTTP_OK) {
    std::move(callback).Run(gfx::Image());
    return;
  }

  data_decoder::DecodeImageIsolated(
      base::as_byte_span(*response_body),
      data_decoder::mojom::ImageCodec::kDefault, /*shrink_to_fit=*/false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&IdpNetworkRequestManager::OnDecodedImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IdpNetworkRequestManager::OnDecodedImage(ImageCallback callback,
                                              const SkBitmap& decoded_bitmap) {
  std::move(callback).Run(gfx::Image::CreateFrom1xBitmap(decoded_bitmap));
}

std::unique_ptr<network::ResourceRequest>
IdpNetworkRequestManager::CreateUncredentialedResourceRequest(
    const GURL& target_url,
    bool send_origin,
    bool follow_redirects) const {
  // We want this to be unique, so we append a random string.
  static constexpr char kFedCmSchemeForIsolationKey[] = "fedcm-9c0367b4";

  auto resource_request = std::make_unique<network::ResourceRequest>();

  GURL::Replacements replacements;
  replacements.SetSchemeStr(kFedCmSchemeForIsolationKey);
  GURL target_url_for_isolation_info =
      target_url.ReplaceComponents(replacements);
  url::Origin target_origin_for_isolation_info =
      url::Origin::Create(target_url_for_isolation_info);

  resource_request->url = target_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kApplicationJson);
  resource_request->destination =
      network::mojom::RequestDestination::kWebIdentity;
  // See https://github.com/fedidcg/FedCM/issues/379 for why the Origin header
  // is sent instead of the Referrer header.
  if (send_origin) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                        relying_party_origin_.Serialize());
    DCHECK(!follow_redirects);
  }
  if (follow_redirects) {
    resource_request->redirect_mode = network::mojom::RedirectMode::kFollow;
  } else {
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  }
  resource_request->request_initiator = url::Origin();
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, relying_party_origin_,
      target_origin_for_isolation_info, net::SiteForCookies());
  DCHECK(client_security_state_);
  resource_request->trusted_params->client_security_state =
      client_security_state_.Clone();
  return resource_request;
}

std::unique_ptr<network::ResourceRequest>
IdpNetworkRequestManager::CreateCredentialedResourceRequest(
    const GURL& target_url,
    CredentialedResourceRequestType type) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  auto target_origin = url::Origin::Create(target_url);
  auto site_for_cookies = net::SiteForCookies::FromOrigin(target_origin);

  if (IsFedCmSameSiteNoneEnabled()) {
    // Setting the initiator to relying_party_origin_ ensures that we don't send
    // SameSite=Strict cookies.
    resource_request->request_initiator = relying_party_origin_;
  } else {
    // We set the initiator to nullopt to denote browser-initiated so that this
    // request is considered first-party. We want to send first-party cookies
    // because this is not a real third-party request as it is mediated by the
    // browser, and third-party cookies will be going away with 3pc deprecation,
    // but we still need to send cookies in these requests.
    // We use nullopt instead of target_origin because we want to send a
    // `Sec-Fetch-Site: none` header instead of `Sec-Fetch-Site: same-origin`.
    resource_request->request_initiator = std::nullopt;
  }

  resource_request->destination =
      network::mojom::RequestDestination::kWebIdentity;
  resource_request->url = target_url;
  resource_request->site_for_cookies = site_for_cookies;
  // TODO(crbug.com/40284123): Figure out why when using CORS we still need to
  // explicitly pass the Origin header.
  if (type != CredentialedResourceRequestType::kNoOrigin) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                        relying_party_origin_.Serialize());
  }
  if (type == CredentialedResourceRequestType::kOriginWithCORS) {
    resource_request->mode = network::mojom::RequestMode::kCors;
    resource_request->request_initiator = relying_party_origin_;
  }
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kApplicationJson);

  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  net::IsolationInfo::RequestType request_type =
      net::IsolationInfo::RequestType::kOther;
  if (IsFedCmSameSiteLaxEnabled()) {
    // We use kMainFrame so that we can send SameSite=Lax cookies.
    request_type = net::IsolationInfo::RequestType::kMainFrame;
  }
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      request_type, target_origin, target_origin, site_for_cookies);
  DCHECK(client_security_state_);
  resource_request->trusted_params->client_security_state =
      client_security_state_.Clone();
  return resource_request;
}

}  // namespace content
