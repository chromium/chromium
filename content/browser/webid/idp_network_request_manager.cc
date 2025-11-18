// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include <optional>
#include <string>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/identity_provider_info.h"
#include "content/browser/webid/mappers.h"
#include "content/browser/webid/metrics.h"
#include "content/browser/webid/network_request_manager.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/webid/constants.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "content/public/common/color_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace content {

using LoginState = IdentityRequestAccount::LoginState;
using TokenError = IdentityCredentialTokenError;

namespace webid {

using AccountsResponseInvalidReason =
    IdpNetworkRequestManager::AccountsResponseInvalidReason;
using ClientMetadata = IdpNetworkRequestManager::ClientMetadata;
using Endpoints = IdpNetworkRequestManager::Endpoints;
using ErrorDialogType = IdpNetworkRequestManager::FedCmErrorDialogType;
using ErrorUrlType = IdpNetworkRequestManager::FedCmErrorUrlType;
using TokenResponseType = IdpNetworkRequestManager::FedCmTokenResponseType;
using TokenResult = IdpNetworkRequestManager::TokenResult;

namespace {

// Path to find the well-known file on the eTLD+1 host.
constexpr char kWellKnownPath[] = "/.well-known/web-identity";

// Well-known file JSON keys
constexpr char kProviderUrlListKey[] = "provider_urls";

// fedcm.json configuration keys.
constexpr char kIdAssertionEndpoint[] = "id_assertion_endpoint";
constexpr char kVcIssuanceEndpoint[] = "vc_issuance_endpoint";
constexpr char kClientMetadataEndpointKey[] = "client_metadata_endpoint";
constexpr char kMetricsEndpoint[] = "metrics_endpoint";
constexpr char kDisconnectEndpoint[] = "disconnect_endpoint";
constexpr char kModesKey[] = "modes";
constexpr char kTypesKey[] = "types";
constexpr char kFormatsKey[] = "formats";
constexpr char kAccountLabelKey[] = "account_label";

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
constexpr char kClientIsThirdPartyToTopFrameOriginKey[] =
    "client_is_third_party_to_top_frame_origin";

// Accounts endpoint response keys.
constexpr char kAccountsKey[] = "accounts";
constexpr char kIdpBrandingKey[] = "branding";

// Keys in 'branding' 'icons' dictionary in config for the IDP icon and client
// metadata endpoint for the RP icon.
constexpr char kBrandingIconsKey[] = "icons";
constexpr char kBrandingIconUrl[] = "url";
constexpr char kBrandingIconSize[] = "size";

// The id assertion endpoint contains a token result.
constexpr char kTokenKey[] = "token";
constexpr char kIssuanceTokenKey[] = "issuance_token";
// The id assertion endpoint contains a URL, which indicates that
// the serve wants to direct the user to continue on a pop-up
// window before it provides a token result.
constexpr char kContinueOnKey[] = "continue_on";
// The id assertion endpoint may contain an error dict containing a code and url
// which describes the error.
constexpr char kErrorKey[] = "error";
constexpr char kErrorCodeKey[] = "code";
constexpr char kErrorUrlKey[] = "url";

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

// Returns true for nullptr for easy use with Dict::FindString.
bool IsEmptyOrWhitespace(const std::string* input) {
  if (!input) {
    return true;
  }

  auto trimmed_string =
      base::TrimWhitespace(base::UTF8ToUTF16(*input), base::TRIM_ALL);
  return trimmed_string.empty();
}

GURL ExtractUrl(const base::Value::Dict& response, const char* key) {
  const std::string* response_url = response.FindString(key);
  if (!response_url) {
    return GURL();
  }
  GURL url = GURL(*response_url);
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
  auto* id = account.FindString(webid::kAccountIdKey);
  auto* email = account.FindString(webid::kAccountEmailKey);
  auto* name = account.FindString(webid::kAccountNameKey);
  auto* phone = account.FindString(webid::kAccountPhoneNumberKey);
  auto* username = account.FindString(webid::kAccountUsernameKey);
  auto* given_name = account.FindString(webid::kAccountGivenNameKey);
  auto* picture = account.FindString(webid::kAccountPictureKey);
  auto* approved_clients = account.FindList(webid::kAccountApprovedClientsKey);
  std::vector<std::string> account_hints;
  auto* hints = account.FindList(webid::kHintsKey);
  if (hints) {
    for (const base::Value& entry : *hints) {
      if (entry.is_string()) {
        account_hints.emplace_back(entry.GetString());
      }
    }
  }
  std::vector<std::string> domain_hints;
  auto* domain_hints_list = account.FindList(webid::kDomainHintsKey);
  if (domain_hints_list) {
    for (const base::Value& entry : *domain_hints_list) {
      if (entry.is_string()) {
        domain_hints.emplace_back(entry.GetString());
      }
    }
  }

  std::vector<std::string> labels;
  const base::ListValue* labels_list = nullptr;
  if (webid::IsUseOtherAccountAndLabelsNewSyntaxEnabled()) {
    labels_list = account.FindList(webid::kLabelHintsKey);
  } else {
    labels_list = account.FindList(webid::kLabelsKey);
  }
  if (labels_list) {
    for (const base::Value& entry : *labels_list) {
      if (entry.is_string()) {
        labels.emplace_back(entry.GetString());
      }
    }
  }

  if (!id) {
    return nullptr;
  }

  std::string display_identifier;
  std::string display_name;
  std::string empty_string;
  if (webid::IsAlternativeIdentifiersEnabled()) {
    std::vector<std::string_view> identifiers;
    if (!IsEmptyOrWhitespace(name)) {
      identifiers.emplace_back(*name);
    } else {
      name = &empty_string;
    }
    if (!IsEmptyOrWhitespace(username)) {
      identifiers.emplace_back(*username);
    }
    if (!IsEmptyOrWhitespace(email)) {
      identifiers.emplace_back(*email);
    } else {
      email = &empty_string;
    }
    if (!IsEmptyOrWhitespace(phone)) {
      identifiers.emplace_back(*phone);
    }
    if (identifiers.empty()) {
      return nullptr;
    }
    display_name = identifiers[0];
    if (identifiers.size() > 1) {
      display_identifier = identifiers[1];
    }
  } else {
    // required fields
    // TODO(crbug.com/40849405): validate email address.
    if (IsEmptyOrWhitespace(email) || IsEmptyOrWhitespace(name)) {
      return nullptr;
    }
    display_identifier = *email;
    display_name = *name;
  }

  webid::RecordApprovedClientsExistence(approved_clients != nullptr);

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
    webid::RecordApprovedClientsSize(approved_clients->size());
  }

  return base::MakeRefCounted<IdentityRequestAccount>(
      *id, display_identifier, display_name, *email, *name,
      given_name ? *given_name : "", picture ? GURL(*picture) : GURL(),
      phone ? *phone : "", username ? *username : "", std::move(account_hints),
      std::move(domain_hints), std::move(labels), approved_value,
      /*browser_trusted_login_state=*/LoginState::kSignUp);
}

// Parses accounts from given Value. Returns true if parse is successful and
// adds parsed accounts to the |account_list|.
bool ParseAccounts(const base::Value::List& accounts,
                   std::vector<IdentityRequestAccountPtr>& account_list,
                   const std::string& client_id,
                   bool from_accounts_push,
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
      parsed_account->from_accounts_push = from_accounts_push;
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
                             int brand_icon_minimum_size,
                             const GURL& config_url) {
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

    if (!config_url.is_empty()) {
      icon.src = config_url.Resolve(*icon_src);
    } else {
      icon.src = GURL(*icon_src);
    }

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

  idp_metadata.brand_icon_url =
      FindBestMatchingIconUrl(icons_value, brand_icon_ideal_size,
                              brand_icon_minimum_size, idp_metadata.config_url);
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
  if (!well_known.accounts.is_empty() && !well_known.login_url.is_empty() &&
      !dict->Find(kProviderUrlListKey)) {
    std::move(callback).Run(fetch_status, std::move(well_known));
    return;
  }
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
  endpoints.issuance = ExtractEndpoint(provider, response, kVcIssuanceEndpoint);

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

  if (webid::IsDelegationEnabled()) {
    const base::Value::List* formats = response.FindList(kFormatsKey);
    if (formats) {
      for (const auto& format : *formats) {
        if (format.is_string()) {
          idp_metadata.formats.push_back(format.GetString());
        }
      }
    }
  }

  if (webid::IsIdPRegistrationEnabled()) {
    const base::Value::List* types = response.FindList(kTypesKey);
    if (types) {
      for (const auto& type : *types) {
        if (type.is_string()) {
          idp_metadata.types.push_back(type.GetString());
        }
      }
    }
  }

  const std::string* requested_label = nullptr;
  if (webid::IsUseOtherAccountAndLabelsNewSyntaxEnabled()) {
    requested_label = response.FindString(kAccountLabelKey);
  } else {
    const base::Value::Dict* accounts_dict = response.FindDict(kAccountsKey);
    if (accounts_dict) {
      requested_label = accounts_dict->FindString(kIncludeKey);
    }
  }
  if (requested_label) {
    idp_metadata.requested_label = *requested_label;
  }

  std::optional<bool> supports_add_account;
  if (webid::IsUseOtherAccountAndLabelsNewSyntaxEnabled()) {
    supports_add_account = response.FindBool(kSupportsUseOtherAccountKey);
  } else {
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
      }
    }
    if (selected_mode_dict) {
      supports_add_account =
          selected_mode_dict->FindBool(kSupportsUseOtherAccountKey);
    }
  }
  if (supports_add_account) {
    idp_metadata.supports_add_account = *supports_add_account;
  }
  std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                          endpoints, std::move(idp_metadata));
}

void OnClientMetadataParsed(
    bool is_cross_site_iframe,
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
  if (is_cross_site_iframe) {
    auto value = response.FindBool(kClientIsThirdPartyToTopFrameOriginKey);
    CrossSiteIframeType type_for_metrics;
    if (!value) {
      type_for_metrics = CrossSiteIframeType::kNoValueReceived;
    } else if (*value) {
      type_for_metrics = CrossSiteIframeType::kIframeIsThirdParty;
    } else {
      type_for_metrics = CrossSiteIframeType::kIframeIsSameParty;
    }
    RecordCrossSiteIframeType(type_for_metrics);
    data.client_is_third_party_to_top_frame_origin = value.value_or(false);
  }

  const base::Value::List* icons_value = response.FindList(kBrandingIconsKey);
  if (icons_value) {
    data.brand_icon_url =
        FindBestMatchingIconUrl(icons_value, rp_brand_icon_ideal_size,
                                rp_brand_icon_minimum_size, GURL());
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
    webid::RecordAccountsResponseInvalidReason(
        AccountsResponseInvalidReason::kResponseIsNotJsonOrDict);
    std::move(callback).Run(fetch_status, account_list);
    return;
  }

  const base::Value::Dict& response = result->GetDict();
  const base::Value::List* accounts = response.FindList(kAccountsKey);

  if (!accounts) {
    webid::RecordAccountsResponseInvalidReason(
        AccountsResponseInvalidReason::kNoAccountsKey);
    std::move(callback).Run(
        {ParseStatus::kInvalidResponseError, fetch_status.response_code},
        account_list);
    return;
  }

  if (accounts->empty()) {
    webid::RecordAccountsResponseInvalidReason(
        AccountsResponseInvalidReason::kAccountListIsEmpty);
    std::move(callback).Run(
        {ParseStatus::kEmptyListError, fetch_status.response_code},
        account_list);
    return;
  }

  AccountsResponseInvalidReason parsing_error =
      AccountsResponseInvalidReason::kResponseIsNotJsonOrDict;
  bool accounts_valid =
      ParseAccounts(*accounts, account_list, client_id,
                    fetch_status.from_accounts_push, parsing_error);

  if (!accounts_valid) {
    CHECK_NE(parsing_error,
             AccountsResponseInvalidReason::kResponseIsNotJsonOrDict);
    webid::RecordAccountsResponseInvalidReason(parsing_error);

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

  if (!net::SchemefulSite::IsSameSite(error_origin, idp_origin)) {
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

TokenResponseType GetTokenResponseType(const base::Value* token,
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

  const base::Value* token_value =
      can_use_response ? response->Find(kTokenKey) : nullptr;
  if (!webid::IsNonStringTokenEnabled() && token_value &&
      !token_value->is_string()) {
    token_value = nullptr;
  }

  const std::string* issuance_token =
      can_use_response ? response->FindString(kIssuanceTokenKey) : nullptr;

  // continue_on_callback is only set if authz is enabled.
  const std::string* continue_on = can_use_response && continue_on_callback
                                       ? response->FindString(kContinueOnKey)
                                       : nullptr;
  const base::Value::Dict* response_error =
      response ? response->FindDict(kErrorKey) : nullptr;

  TokenResponseType token_response_type =
      GetTokenResponseType(token_value, continue_on, response_error);

  if (response_error) {
    std::string error_code;
    if (webid::IsErrorAttributeEnabled()) {
      error_code = ExtractString(*response_error, kErrorKey);
    }
    if (error_code.empty()) {
      error_code = ExtractString(*response_error, kErrorCodeKey);
    }

    const std::string* url = response_error->FindString(kErrorUrlKey);
    GURL error_url;
    std::optional<ErrorUrlType> error_url_type;
    std::tie(error_url, error_url_type) = GetErrorUrlAndType(url, token_url);
    token_result.error = TokenError{error_code, error_url};
    std::move(record_error_metrics_callback)
        .Run(token_response_type, GetErrorDialogType(error_code, error_url),
             error_url_type);
    std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                            std::move(token_result));
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
    std::move(callback).Run(fetch_status, std::move(token_result));
    return;
  }
  DCHECK(response);

  if (issuance_token && webid::IsDelegationEnabled()) {
    token_result.token = base::Value(*issuance_token);
    std::move(record_error_metrics_callback)
        .Run(token_response_type, /*error_dialog_type=*/std::nullopt,
             /*error_url_type=*/std::nullopt);
    std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                            std::move(token_result));
    return;
  }

  if (token_value) {
    token_result.token = token_value->Clone();

    std::move(record_error_metrics_callback)
        .Run(token_response_type, /*error_dialog_type=*/std::nullopt,
             /*error_url_type=*/std::nullopt);
    std::move(callback).Run({ParseStatus::kSuccess, fetch_status.response_code},
                            std::move(token_result));
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
      std::move(token_result));
}

void OnLogoutCompleted(IdpNetworkRequestManager::LogoutCallback callback,
                       std::optional<std::string> response_body,
                       int response_code,
                       const std::string& mime_type,
                       bool cors_error) {
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

IdpNetworkRequestManager::ClientMetadata::ClientMetadata() = default;
IdpNetworkRequestManager::ClientMetadata::~ClientMetadata() = default;
IdpNetworkRequestManager::ClientMetadata::ClientMetadata(
    const ClientMetadata& other) = default;

IdpNetworkRequestManager::TokenResult::TokenResult() = default;
IdpNetworkRequestManager::TokenResult::~TokenResult() = default;
IdpNetworkRequestManager::TokenResult::TokenResult(TokenResult&& other) =
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
      host->GetMainFrame()->GetLastCommittedOrigin(),
      host->GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess(),
      host->GetBrowserContext()->GetFederatedIdentityPermissionContext(),
      host->BuildClientSecurityState(), host->GetFrameTreeNodeId());
}

IdpNetworkRequestManager::IdpNetworkRequestManager(
    const url::Origin& relying_party_origin,
    const url::Origin& rp_embedding_origin,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    network::mojom::ClientSecurityStatePtr client_security_state,
    content::FrameTreeNodeId frame_tree_node_id)
    : NetworkRequestManager(relying_party_origin,
                            loader_factory,
                            std::move(client_security_state),
                            network::mojom::RequestDestination::kWebIdentity,
                            frame_tree_node_id),
      rp_embedding_origin_(rp_embedding_origin),
      permission_delegate_(permission_delegate) {
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

net::NetworkTrafficAnnotationTag
IdpNetworkRequestManager::CreateTrafficAnnotation() {
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

void IdpNetworkRequestManager::FetchWellKnown(const GURL& provider,
                                              FetchWellKnownCallback callback) {
  std::optional<GURL> well_known_url =
      ComputeWellKnownUrl(provider, kWellKnownPath);

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
      base::BindOnce(&OnWellKnownParsed, std::move(callback), *well_known_url));
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
                     std::move(callback)));
}

bool IdpNetworkRequestManager::SendAccountsRequest(
    const url::Origin& idp_origin,
    const GURL& accounts_url,
    const std::string& client_id,
    AccountsRequestCallback callback) {
  if (webid::IsLightweightModeEnabled()) {
    base::Value::List accounts = permission_delegate_->GetAccounts(idp_origin);
    FetchStatus success_status = {
        .parse_status = ParseStatus::kSuccess,
        .response_code = 200,
        .from_accounts_push = true,
    };

    if (accounts.size() > 0) {
      OnAccountsRequestParsed(
          client_id, std::move(callback), success_status,
          data_decoder::DataDecoder::ValueOrError(
              base::Value::Dict().Set(kAccountsKey, std::move(accounts))));
      return false;
    }

    // If there were no stored accounts and the supplied accounts URL is empty,
    // behave as though we received an empty accounts_endpoint response.
    if (accounts_url.is_empty()) {
      OnAccountsRequestParsed(
          client_id, std::move(callback), success_status,
          data_decoder::DataDecoder::ValueOrError(
              base::Value::Dict().Set(kAccountsKey, base::Value::List())));
      return false;
    }
  }

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCredentialedResourceRequest(
          accounts_url, CredentialedResourceRequestType::kNoOrigin);
  DownloadJsonAndParse(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnAccountsRequestParsed, client_id, std::move(callback)));
  return true;
}

void IdpNetworkRequestManager::SendTokenRequest(
    const GURL& token_url,
    const std::string& account,
    const std::string& url_encoded_post_data,
    bool idp_blindness,
    TokenRequestCallback callback,
    ContinueOnCallback continue_on,
    RecordErrorMetricsCallback record_error_metrics_callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCredentialedResourceRequest(
          token_url, idp_blindness
                         ? CredentialedResourceRequestType::kNoOrigin
                         : CredentialedResourceRequestType::kOriginWithCORS);

  if (idp_blindness) {
    // IdP blindness can only be used when the feature is enabled.
    DCHECK(webid::IsDelegationEnabled());
    // We have to set this to a Origin: null because the underlying loader
    // will  not let us send a request without Origin header if the request
    // method is POST.
    resource_request->request_initiator = url::Origin();
  }

  DownloadJsonAndParse(
      std::move(resource_request), url_encoded_post_data,
      base::BindOnce(&OnTokenRequestParsed, std::move(callback),
                     std::move(continue_on),
                     std::move(record_error_metrics_callback), token_url),
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
      "outcome=success"
      "&time_to_show_ui=%d"
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
  // Typically, this IdpNetworkRequestManager will be destroyed after
  // we return, but because the SimpleURLLoader is owned by the callback
  // object, the load will not be aborted.
  // The result of the download is not important, so we pass an empty
  // DownloadCallback.
  DownloadUrl(std::move(resource_request), url_encoded_post_data,
              DownloadCallback(), maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint_url,
    bool did_show_ui,
    webid::MetricsEndpointErrorCode error_code) {
  std::string url_encoded_post_data = base::StringPrintf(
      "outcome=failure&error_code=%d&did_show_ui=%s",
      static_cast<int>(error_code), base::ToString(did_show_ui));
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(metrics_endpoint_url,
                                          /*send_origin=*/false);

  // Typically, this IdpNetworkRequestManager will be destroyed after
  // we return, but because the SimpleURLLoader is owned by the callback
  // object, the load will not be aborted.
  // The result of the download is not important, so we pass an empty
  // DownloadCallback.
  DownloadUrl(std::move(resource_request), url_encoded_post_data,
              DownloadCallback(), maxResponseSizeInKiB * 1024);
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
      base::BindOnce(&OnDisconnectResponseParsed, std::move(callback)));
}

bool IdpNetworkRequestManager::IsCrossSiteIframe() const {
  return webid::IsIframeOriginEnabled() && !rp_embedding_origin_.opaque() &&
         !net::SchemefulSite::IsSameSite(relying_party_origin_,
                                         rp_embedding_origin_);
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

void IdpNetworkRequestManager::FetchAccountPicturesAndBrandIcons(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const GURL& rp_brand_icon_url,
    FetchAccountPicturesAndBrandIconsCallback callback) {
  GURL idp_brand_icon_url = idp_info->metadata.brand_icon_url;
  GURL config_url = idp_info->metadata.config_url;

  auto barrier_callback = base::BarrierClosure(
      // Wait for all accounts plus the brand icon URLs.
      accounts.size() + 2,
      base::BindOnce(&IdpNetworkRequestManager::
                         OnAllAccountPicturesAndBrandIconUrlReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(idp_info), accounts, rp_brand_icon_url));

  for (const auto& account : accounts) {
    if (webid::IsLightweightModeEnabled() && account->from_accounts_push) {
      FetchCachedAccountImage(url::Origin::Create(config_url), account->picture,
                              barrier_callback);
    } else {
      FetchImage(account->picture, barrier_callback);
    }
  }
  FetchImage(idp_brand_icon_url, barrier_callback);
  FetchImage(rp_brand_icon_url, barrier_callback);
}

void IdpNetworkRequestManager::FetchIdpBrandIcon(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    FetchIdpBrandIconCallback callback) {
  GURL idp_brand_icon_url = idp_info->metadata.brand_icon_url;
  FetchImage(idp_brand_icon_url,
             base::BindOnce(&IdpNetworkRequestManager::OnIdpBrandIconReceived,
                            weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                            std::move(callback)));
}

void IdpNetworkRequestManager::FetchImage(const GURL& url,
                                          base::OnceClosure callback) {
  if (url.is_valid()) {
    DownloadAndDecodeImage(
        url, base::BindOnce(&IdpNetworkRequestManager::OnImageReceived,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            url));
  } else {
    // We have to still call the callback to make sure the barrier
    // callback gets the right number of calls.
    std::move(callback).Run();
  }
}

void IdpNetworkRequestManager::FetchCachedAccountImage(
    const url::Origin& idp_origin,
    const GURL& url,
    base::OnceClosure callback) {
  if (url.is_valid()) {
    DownloadAndDecodeCachedImage(
        idp_origin, url,
        base::BindOnce(&IdpNetworkRequestManager::OnImageReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       url));
  } else {
    std::move(callback).Run();
  }
}

void IdpNetworkRequestManager::OnImageReceived(base::OnceClosure callback,
                                               GURL url,
                                               const gfx::Image& image) {
  downloaded_images_[url] = image;
  std::move(callback).Run();
}

void IdpNetworkRequestManager::OnAllAccountPicturesAndBrandIconUrlReceived(
    FetchAccountPicturesAndBrandIconsCallback callback,
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::vector<IdentityRequestAccountPtr>&& accounts,
    const GURL& rp_brand_icon_url) {
  for (auto& account : accounts) {
    auto it = downloaded_images_.find(account->picture);
    if (it != downloaded_images_.end()) {
      // We do not use std::move here in case multiple accounts use the same
      // picture URL, and the underlying gfx::Image data is refcounted anyway.
      account->decoded_picture = it->second;
    }
  }

  gfx::Image rp_brand_icon;
  auto it = downloaded_images_.find(rp_brand_icon_url);
  if (it != downloaded_images_.end()) {
    rp_brand_icon = it->second;
  }

  it = downloaded_images_.find(idp_info->metadata.brand_icon_url);
  if (it != downloaded_images_.end()) {
    idp_info->metadata.brand_decoded_icon = it->second;
  }
  std::move(callback).Run(std::move(accounts), std::move(idp_info),
                          rp_brand_icon);
}

void IdpNetworkRequestManager::OnIdpBrandIconReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    FetchIdpBrandIconCallback callback) {
  auto it = downloaded_images_.find(idp_info->metadata.brand_icon_url);
  if (it != downloaded_images_.end()) {
    idp_info->metadata.brand_decoded_icon = it->second;
  }
  std::move(callback).Run(std::move(idp_info));
}

void IdpNetworkRequestManager::DownloadAndDecodeCachedImage(
    const url::Origin& idp_origin,
    const GURL& url,
    ImageCallback callback) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateCachedAccountPictureRequest(idp_origin, url,
                                        /*cache_only=*/true);
  DownloadUrl(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&IdpNetworkRequestManager::OnDownloadedImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::FetchClientMetadata(
    const GURL& endpoint,
    const std::string& client_id,
    int rp_brand_icon_ideal_size,
    int rp_brand_icon_minimum_size,
    FetchClientMetadataCallback callback) {
  std::string parameters =
      "?client_id=" + base::EscapeQueryParamValue(client_id, true);
  if (IsCrossSiteIframe()) {
    parameters += "&top_frame_origin=" +
                  base::EscapeQueryParamValue(rp_embedding_origin_.Serialize(),
                                              /*use_plus=*/false);
  }
  GURL target_url = endpoint.Resolve(parameters);

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateUncredentialedResourceRequest(target_url,
                                          /* send_origin= */ true);

  DownloadJsonAndParse(
      std::move(resource_request),
      /*url_encoded_post_data=*/std::nullopt,
      base::BindOnce(&OnClientMetadataParsed, IsCrossSiteIframe(),
                     rp_brand_icon_ideal_size, rp_brand_icon_minimum_size,
                     std::move(callback)));
}

void IdpNetworkRequestManager::OnDownloadedImage(
    ImageCallback callback,
    std::optional<std::string> response_body,
    int response_code,
    const std::string& mime_type,
    bool cors_error) {
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
IdpNetworkRequestManager::CreateCachedAccountPictureRequest(
    const url::Origin& idp_origin,
    const GURL& target_url,
    bool cache_only) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = target_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  resource_request->destination =
      network::mojom::RequestDestination::kWebIdentity;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;

  resource_request->request_initiator = url::Origin();
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/idp_origin,
      /*frame_origin=*/url::Origin::Create(target_url), net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      net::NetworkIsolationPartition::kFedCmUncredentialedRequests);
  DCHECK(client_security_state_);
  resource_request->trusted_params->client_security_state =
      client_security_state_.Clone();

  if (cache_only) {
    resource_request->load_flags |= net::LOAD_ONLY_FROM_CACHE;
  }

  return resource_request;
}

void IdpNetworkRequestManager::CacheAccountPictures(
    const url::Origin& idp_origin,
    const std::vector<GURL>& picture_urls) {
  for (const GURL& url : picture_urls) {
    if (url.is_valid()) {
      DownloadUrl(CreateCachedAccountPictureRequest(idp_origin, url,
                                                    /*cache_only=*/false),
                  /*url_encoded_post_data=*/std::nullopt, DownloadCallback(),
                  maxResponseSizeInKiB * 1024);
    }
  }
}

}  // namespace webid
}  // namespace content
