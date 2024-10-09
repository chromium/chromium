// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <random>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/federated_auth_disconnect_request.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/browser/webid/federated_auth_user_info_request.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using base::Value;
using blink::mojom::DisconnectStatus;
using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::IdentityProviderConfig;
using blink::mojom::IdentityProviderConfigPtr;
using blink::mojom::IdentityProviderGetParametersPtr;
using blink::mojom::IdentityProviderRequestOptions;
using blink::mojom::IdentityProviderRequestOptionsPtr;
using blink::mojom::RegisterIdpStatus;
using blink::mojom::RequestTokenStatus;
using blink::mojom::RequestUserInfoStatus;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using DisconnectStatusForMetrics = content::FedCmDisconnectStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using TokenResponseType =
    content::IdpNetworkRequestManager::FedCmTokenResponseType;
using ErrorDialogType = content::IdpNetworkRequestManager::FedCmErrorDialogType;
using ErrorUrlType = content::IdpNetworkRequestManager::FedCmErrorUrlType;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using ErrorDialogResult = content::FedCmErrorDialogResult;
using CompleteRequestWithErrorCallback =
    base::OnceCallback<void(blink::mojom::FederatedAuthRequestResult,
                            std::optional<content::FedCmRequestIdTokenStatus>,
                            std::optional<TokenError> token_error,
                            bool)>;

namespace content {

namespace {
static constexpr base::TimeDelta kTokenRequestDelay = base::Seconds(3);
static constexpr base::TimeDelta kMaxRejectionTime = base::Seconds(60);

// Users spend less time on Android to dismiss the UI. Given the difference, we
// use two set of values. The values are calculated based on UMA data to follow
// lognormal distribution.
#if BUILDFLAG(IS_ANDROID)
static constexpr double kRejectionLogNormalMu = 7.4;
static constexpr double kRejectionLogNormalSigma = 1.24;
#else
static constexpr double kRejectionLogNormalMu = 8.6;
static constexpr double kRejectionLogNormalSigma = 1.4;
#endif  // BUILDFLAG(IS_ANDROID)

static constexpr char kDefaultFieldName[] = "name";
static constexpr char kDefaultFieldEmail[] = "email";
static constexpr char kDefaultFieldPicture[] = "picture";

bool IsRequestingDefaultPermissions(const std::vector<std::string>& fields) {
  return base::Contains(fields, kDefaultFieldName) &&
         base::Contains(fields, kDefaultFieldEmail) &&
         base::Contains(fields, kDefaultFieldPicture);
}

std::vector<std::string> DisclosureFieldsToStringList(
    const std::vector<IdentityRequestDialogDisclosureField>& fields) {
  std::vector<std::string> list;
  for (auto field : fields) {
    switch (field) {
      case IdentityRequestDialogDisclosureField::kName:
        list.push_back(kDefaultFieldName);
        break;
      case IdentityRequestDialogDisclosureField::kEmail:
        list.push_back(kDefaultFieldEmail);
        break;
      case IdentityRequestDialogDisclosureField::kPicture:
        list.push_back(kDefaultFieldPicture);
        break;
    }
  }
  return list;
}

std::string ComputeUrlEncodedTokenPostData(
    RenderFrameHost& render_frame_host,
    const url::Origin& idp_origin,
    const std::string& client_id,
    const std::string& nonce,
    const std::string& account_id,
    bool is_auto_reauthn,
    const RpMode& rp_mode,
    const std::optional<std::vector<std::string>>& fields,
    const std::vector<std::string>& disclosure_shown_for,
    const base::flat_map<std::string, std::string>& params) {
  std::string query;
  if (!client_id.empty()) {
    query +=
        "client_id=" + base::EscapeUrlEncodedData(client_id, /*use_plus=*/true);
  }

  if (!nonce.empty()) {
    if (!query.empty()) {
      query += "&";
    }
    query += "nonce=" + base::EscapeUrlEncodedData(nonce, /*use_plus=*/true);
  }

  if (!account_id.empty()) {
    if (!query.empty()) {
      query += "&";
    }
    query += "account_id=" +
             base::EscapeUrlEncodedData(account_id, /*use_plus=*/true);
  }
  // For new users signing up, we show some disclosure text to remind them about
  // data sharing between IDP and RP. For returning users signing in, such
  // disclosure text is not necessary. This field indicates in the request
  // whether the user has been shown such disclosure text.
  std::string disclosure_text_shown_param =
      IsRequestingDefaultPermissions(disclosure_shown_for) ? "true" : "false";
  if (!query.empty()) {
    query += "&";
  }
  query += "disclosure_text_shown=" + disclosure_text_shown_param;

  // Shares with IdP that whether the identity credential was automatically
  // selected. This could help developers to better comprehend the token
  // request and segment metrics accordingly.
  std::string is_auto_selected = is_auto_reauthn ? "true" : "false";
  if (!query.empty()) {
    query += "&";
  }
  query += "is_auto_selected=" + is_auto_selected;

  // TODO(crbug.com/40284792): ActiveMode is enabled by default on the browser
  // side to support origin trials. To avoid sending "mode=passive" for all
  // existing traffic, we restrict it to traffic that uses the active mode for
  // now. We should remove this restriction before shipping the active flow.
  if (IsFedCmActiveModeEnabled() && rp_mode == RpMode::kActive) {
    // Shares with IdP the type of the request.
    std::string rp_mode_str = rp_mode == RpMode::kActive ? "active" : "passive";
    if (!query.empty()) {
      query += "&";
    }
    query += "mode=" + rp_mode_str;
  }

  if (webid::IsFedCmAuthzEnabled(render_frame_host, idp_origin)) {
    std::vector<std::string> fields_to_use;
    if (fields) {
      fields_to_use = *fields;
    } else {
      fields_to_use = {kDefaultFieldName, kDefaultFieldEmail,
                       kDefaultFieldPicture};
    }
    if (!fields_to_use.empty()) {
      query += "&fields=" +
               base::EscapeUrlEncodedData(base::JoinString(fields_to_use, ","),
                                          /*use_plus=*/true);
    }

    if (!disclosure_shown_for.empty()) {
      query += "&disclosure_shown_for=" +
               base::EscapeUrlEncodedData(
                   base::JoinString(disclosure_shown_for, ","),
                   /*use_plus=*/true);
    }

    if (!params.empty()) {
      base::Value::Dict param_dict;
      for (const auto& pair : params) {
        // TODO(crbug.com/368087170): Remove before shipping this.
        query += "&param_" +
                 base::EscapeUrlEncodedData(pair.first, /*use_plus=*/true) +
                 "=" +
                 base::EscapeUrlEncodedData(pair.second, /*use_plus=*/true);
        // For JSON serialization
        param_dict.Set(pair.first, pair.second);
      }
      std::optional<std::string> json = base::WriteJson(param_dict);
      if (json) {
        query +=
            "&params=" + base::EscapeUrlEncodedData(*json, /*use_plus=*/true);
      }
    }
  }

  return query;
}

RequestTokenStatus FederatedAuthRequestResultToRequestTokenStatus(
    FederatedAuthRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return RequestTokenStatus::kSuccess;
    }
    case FederatedAuthRequestResult::kTooManyRequests: {
      return RequestTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kCanceled: {
      return RequestTokenStatus::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy:
    case FederatedAuthRequestResult::kDisabledInSettings:
    case FederatedAuthRequestResult::kDisabledInFlags:
    case FederatedAuthRequestResult::kWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kWellKnownNoResponse:
    case FederatedAuthRequestResult::kWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kWellKnownListEmpty:
    case FederatedAuthRequestResult::kWellKnownInvalidContentType:
    case FederatedAuthRequestResult::kConfigNotInWellKnown:
    case FederatedAuthRequestResult::kWellKnownTooBig:
    case FederatedAuthRequestResult::kConfigHttpNotFound:
    case FederatedAuthRequestResult::kConfigNoResponse:
    case FederatedAuthRequestResult::kConfigInvalidResponse:
    case FederatedAuthRequestResult::kConfigInvalidContentType:
    case FederatedAuthRequestResult::kClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kClientMetadataNoResponse:
    case FederatedAuthRequestResult::kClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kClientMetadataInvalidContentType:
    case FederatedAuthRequestResult::kAccountsHttpNotFound:
    case FederatedAuthRequestResult::kAccountsNoResponse:
    case FederatedAuthRequestResult::kAccountsInvalidResponse:
    case FederatedAuthRequestResult::kAccountsListEmpty:
    case FederatedAuthRequestResult::kAccountsInvalidContentType:
    case FederatedAuthRequestResult::kIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kIdTokenNoResponse:
    case FederatedAuthRequestResult::kIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kIdTokenIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenInvalidContentType:
    case FederatedAuthRequestResult::kRpPageNotVisible:
    case FederatedAuthRequestResult::kSilentMediationFailure:
    case FederatedAuthRequestResult::kThirdPartyCookiesBlocked:
    case FederatedAuthRequestResult::kNotSignedInWithIdp:
    case FederatedAuthRequestResult::kMissingTransientUserActivation:
    case FederatedAuthRequestResult::kReplacedByActiveMode:
    case FederatedAuthRequestResult::kInvalidFieldsSpecified:
    case FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque:
    case FederatedAuthRequestResult::kTypeNotMatching:
    case FederatedAuthRequestResult::kError: {
      return RequestTokenStatus::kError;
    }
  }
}

IdpNetworkRequestManager::MetricsEndpointErrorCode
FederatedAuthRequestResultToMetricsEndpointErrorCode(
    blink::mojom::FederatedAuthRequestResult result) {
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kNone;
    }
    case FederatedAuthRequestResult::kTooManyRequests:
    case FederatedAuthRequestResult::kMissingTransientUserActivation:
    case FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque:
    case FederatedAuthRequestResult::kInvalidFieldsSpecified:
    case FederatedAuthRequestResult::kCanceled: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kRpFailure;
    }
    case FederatedAuthRequestResult::kAccountsInvalidResponse:
    case FederatedAuthRequestResult::kAccountsListEmpty:
    case FederatedAuthRequestResult::kAccountsInvalidContentType: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kAccountsEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kIdTokenIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenInvalidContentType: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kTokenEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kDisabledInFlags:
    case FederatedAuthRequestResult::kDisabledInSettings:
    case FederatedAuthRequestResult::kThirdPartyCookiesBlocked:
    case FederatedAuthRequestResult::kRpPageNotVisible:
    case FederatedAuthRequestResult::kReplacedByActiveMode:
    case FederatedAuthRequestResult::kNotSignedInWithIdp: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kUserFailure;
    }
    case FederatedAuthRequestResult::kWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kWellKnownNoResponse:
    case FederatedAuthRequestResult::kConfigHttpNotFound:
    case FederatedAuthRequestResult::kConfigNoResponse:
    case FederatedAuthRequestResult::kClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kClientMetadataNoResponse:
    case FederatedAuthRequestResult::kAccountsHttpNotFound:
    case FederatedAuthRequestResult::kAccountsNoResponse:
    case FederatedAuthRequestResult::kIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kIdTokenNoResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerUnavailable;
    }
    case FederatedAuthRequestResult::kConfigNotInWellKnown:
    case FederatedAuthRequestResult::kWellKnownTooBig: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kManifestError;
    }
    case FederatedAuthRequestResult::kWellKnownListEmpty:
    case FederatedAuthRequestResult::kWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kConfigInvalidResponse:
    case FederatedAuthRequestResult::kClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kWellKnownInvalidContentType:
    case FederatedAuthRequestResult::kConfigInvalidContentType:
    case FederatedAuthRequestResult::kClientMetadataInvalidContentType: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerInvalidResponse;
    }
    case FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy:
    case FederatedAuthRequestResult::kError:
    case FederatedAuthRequestResult::kSilentMediationFailure:
    case FederatedAuthRequestResult::kTypeNotMatching: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kOther;
    }
  }
}

// The time from when the accounts dialog is shown to when a user explicitly
// closes it follows normal distribution. To make the random failures
// indistinguishable from user declines, we use lognormal distribution to
// generate the random number.
base::TimeDelta GetRandomRejectionTime() {
  base::RandomBitGenerator generator;
  std::lognormal_distribution<double> distribution(kRejectionLogNormalMu,
                                                   kRejectionLogNormalSigma);

  base::TimeDelta rejection_time =
      base::Seconds(distribution(generator) / 1000);

  return std::min(kMaxRejectionTime, rejection_time);
}

std::string FormatOriginForDisplay(const url::Origin& origin) {
  return webid::FormatUrlForDisplay(origin.GetURL());
}

std::string GetTopFrameOriginForDisplay(const url::Origin& top_frame_origin) {
  return FormatOriginForDisplay(top_frame_origin);
}

bool IsFrameActive(RenderFrameHost* frame) {
  return frame && frame->IsActive();
}

bool IsFrameVisible(RenderFrameHost* frame) {
  return frame && frame->IsActive() &&
         frame->GetVisibilityState() == content::PageVisibilityState::kVisible;
}

void MaybeAppendQueryParameters(
    const FederatedAuthRequestImpl::IdentityProviderLoginUrlInfo&
        idp_login_info,
    GURL* login_url) {
  if (idp_login_info.login_hint.empty() && idp_login_info.domain_hint.empty()) {
    return;
  }
  std::string old_query = login_url->query();
  if (!old_query.empty()) {
    old_query += "&";
  }
  std::string new_query_string = old_query;
  if (!idp_login_info.login_hint.empty()) {
    new_query_string +=
        "login_hint=" + base::EscapeUrlEncodedData(idp_login_info.login_hint,
                                                   /*use_plus=*/false);
  }
  if (!idp_login_info.domain_hint.empty()) {
    if (!new_query_string.empty()) {
      new_query_string += "&";
    }
    new_query_string +=
        "domain_hint=" + base::EscapeUrlEncodedData(idp_login_info.domain_hint,
                                                    /*use_plus=*/false);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query_string);
  *login_url = login_url->ReplaceComponents(replacements);
}

}  // namespace

FederatedAuthRequestImpl::FetchData::FetchData() = default;
FederatedAuthRequestImpl::FetchData::~FetchData() = default;

FederatedAuthRequestImpl::IdentityProviderGetInfo::IdentityProviderGetInfo(
    blink::mojom::IdentityProviderRequestOptionsPtr provider,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode)
    : provider(std::move(provider)), rp_context(rp_context), rp_mode(rp_mode) {}

FederatedAuthRequestImpl::IdentityProviderGetInfo::~IdentityProviderGetInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderGetInfo::IdentityProviderGetInfo(
    const IdentityProviderGetInfo& other) {
  *this = other;
}

FederatedAuthRequestImpl::IdentityProviderGetInfo&
FederatedAuthRequestImpl::IdentityProviderGetInfo::operator=(
    const IdentityProviderGetInfo& other) {
  provider = other.provider->Clone();
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  return *this;
}

FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    const blink::mojom::IdentityProviderRequestOptionsPtr& provider,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata metadata,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode)
    : provider(provider->Clone()),
      endpoints(std::move(endpoints)),
      metadata(std::move(metadata)),
      rp_context(rp_context),
      rp_mode(rp_mode) {}

FederatedAuthRequestImpl::IdentityProviderInfo::~IdentityProviderInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    const IdentityProviderInfo& other) {
  provider = other.provider->Clone();
  endpoints = other.endpoints;
  metadata = other.metadata;
  has_failing_idp_signin_status = other.has_failing_idp_signin_status;
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  data = other.data;
}

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    IdentityRegistry* identity_registry,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : DocumentService(host, std::move(receiver)),
      api_permission_delegate_(api_permission_context),
      auto_reauthn_permission_delegate_(auto_reauthn_permission_context),
      permission_delegate_(permission_context),
      identity_registry_(identity_registry) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_token_callback_) {
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kUnhandledRequest,
                             /*token_error=*/std::nullopt,
                             /*should_delay_callback=*/false);
  }
  // Calls |FederatedAuthUserInfoRequest|'s destructor to complete the user
  // info request. This is needed because otherwise some resources like
  // `fedcm_metrics_` may no longer be usable when the destructor get invoked
  // naturally.
  user_info_requests_.clear();

  // Calls |FederatedAuthDisconnectRequest|'s destructor to complete the
  // revocation request. This is needed because otherwise some resources like
  // `fedcm_metrics_` may no longer be usable when the destructor get invoked
  // naturally.
  disconnect_request_.reset();

  // Since FederatedAuthRequestImpl is a subclass of
  // DocumentService<blink::mojom::FederatedAuthRequest>, it only lives as long
  // as the current document.
  if (num_requests_ > 0) {
    FedCmMetrics::RecordNumRequestsPerDocument(
        render_frame_host().GetPageUkmSourceId(), num_requests_);
  }
}

// static
void FederatedAuthRequestImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  CHECK(host);

  BrowserContext* browser_context = host->GetBrowserContext();
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_context =
          browser_context->GetFederatedIdentityApiPermissionContext();
  raw_ptr<FederatedIdentityAutoReauthnPermissionContextDelegate>
      auto_reauthn_permission_context =
          browser_context->GetFederatedIdentityAutoReauthnPermissionContext();
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_context =
      browser_context->GetFederatedIdentityPermissionContext();
  raw_ptr<IdentityRegistry> identity_registry =
      IdentityRegistry::FromWebContents(WebContents::FromRenderFrameHost(host));

  if (!api_permission_context || !auto_reauthn_permission_context ||
      !permission_context) {
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new FederatedAuthRequestImpl(
      *host, api_permission_context, auto_reauthn_permission_context,
      permission_context, identity_registry, std::move(receiver));
}

FederatedAuthRequestImpl& FederatedAuthRequestImpl::CreateForTesting(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    IdentityRegistry* identity_registry,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  return *new FederatedAuthRequestImpl(
      host, api_permission_context, auto_reauthn_permission_context,
      permission_context, identity_registry, std::move(receiver));
}

std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
FederatedAuthRequestImpl::MaybeAddRegisteredProviders(
    std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>& providers) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result;

  std::vector<GURL> registered_config_urls =
      permission_delegate_->GetRegisteredIdPs();

  // TODO(crbug.com/40252825): we insert the registered IdPs to
  // the list of IdPs in a reverse chronological order:
  // first IdPs to be registered goes first. It is not clear
  // yet what's the right order, but this seems like a reasonable
  // starting point.
  std::reverse(registered_config_urls.begin(), registered_config_urls.end());

  for (auto& provider : providers) {
    if (!provider->config->use_registered_config_urls) {
      result.emplace_back(provider->Clone());
      continue;
    }

    for (auto& configURL : registered_config_urls) {
      blink::mojom::IdentityProviderRequestOptionsPtr idp = provider->Clone();
      idp->config->use_registered_config_urls = false;
      idp->config->config_url = configURL;
      result.emplace_back(std::move(idp));
    }
  }

  // TODO(crbug.com/40252825): Consider removing duplicate
  // IdPs in case they were present in the registry as well
  // as added individually.

  return result;
}

void FederatedAuthRequestImpl::RequestToken(
    std::vector<IdentityProviderGetParametersPtr> idp_get_params_ptrs,
    MediationRequirement requirement,
    RequestTokenCallback callback) {
  // idp_get_params_ptrs should never be empty since it is the renderer-side
  // code which populates it.
  if (idp_get_params_ptrs.empty()) {
    ReportBadMessageAndDeleteThis("idp_get_params_ptrs is empty.");
    return;
  }
  // This could only happen with a compromised renderer process. We ensure that
  // the provider list size is > 0 on the renderer side at the beginning of
  // parsing |IdentityCredentialRequestOptions|.
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    if (idp_get_params_ptr->providers.size() == 0) {
      ReportBadMessageAndDeleteThis("The provider list should not be empty.");
      return;
    }
    if (idp_get_params_ptr->mode == RpMode::kActive &&
        requirement == MediationRequirement::kSilent) {
      ReportBadMessageAndDeleteThis(
          "mediation: silent is not supported in active mode.");
      return;
    }
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    ReportBadMessageAndDeleteThis(
        "FedCM should not be allowed in fenced frame trees.");
    return;
  }

  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      render_frame_host(), &intercept, &should_complete_request_immediately);
  should_complete_request_immediately =
      (intercept && should_complete_request_immediately) ||
      api_permission_delegate_->ShouldCompleteRequestImmediately();

  // Expand the providers list with registered providers.
  if (IsFedCmIdPRegistrationEnabled()) {
    for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
      std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers =
          MaybeAddRegisteredProviders(idp_get_params_ptr->providers);
      if (providers.empty()) {
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            "No identity providers are registered.");
        base::TimeDelta delay;
        if (!should_complete_request_immediately) {
          delay = GetRandomRejectionTime();
        }
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), RequestTokenStatus::kError,
                           std::nullopt, "",
                           /*error=*/nullptr,
                           /*is_auto_selected=*/false),
            delay);
        return;
      }
      idp_get_params_ptr->providers = std::move(providers);
    }
  }

  if (!render_frame_host().GetPage().IsPrimary()) {
    // This should not be possible but seems to be happening, so we log
    // the lifecycle state for further investigation.
    RenderFrameHostImpl* host_impl =
        static_cast<RenderFrameHostImpl*>(&render_frame_host());
    FedCmLifecycleStateFailureReason reason =
        FedCmLifecycleStateFailureReason::kOther;
    switch (host_impl->lifecycle_state()) {
      case RenderFrameHostImpl::LifecycleStateImpl::kSpeculative:
        reason = FedCmLifecycleStateFailureReason::kSpeculative;
        break;
      case RenderFrameHostImpl::LifecycleStateImpl::kPendingCommit:
        reason = FedCmLifecycleStateFailureReason::kPendingCommit;
        break;
      case RenderFrameHostImpl::LifecycleStateImpl::kPrerendering:
        reason = FedCmLifecycleStateFailureReason::kPrerendering;
        break;
      case RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache:
        reason = FedCmLifecycleStateFailureReason::kInBackForwardCache;
        break;
      case RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers:
        reason = FedCmLifecycleStateFailureReason::kRunningUnloadHandlers;
        break;
      case RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted:
        reason = FedCmLifecycleStateFailureReason::kReadyToBeDeleted;
        break;
      default:
        break;
    };
    RecordLifecycleStateFailureReason(reason);
    std::move(callback).Run(RequestTokenStatus::kError, std::nullopt, "",
                            /*error=*/nullptr,
                            /*is_auto_selected=*/false);
    return;
  }

  // It should not be possible to receive multiple IDPs when the
  // `kFedCmMultipleIdentityProviders` flag is disabled. But such a message
  // could be received from a compromised renderer.
  const bool is_multi_idp_input = idp_get_params_ptrs.size() > 1u ||
                                  idp_get_params_ptrs[0]->providers.size() > 1u;
  if (is_multi_idp_input && !IsFedCmMultipleIdentityProvidersEnabled()) {
    std::move(callback).Run(RequestTokenStatus::kError, std::nullopt, "",
                            /*error=*/nullptr,
                            /*is_auto_selected=*/false);
    return;
  }

  had_transient_user_activation_ =
      render_frame_host().HasTransientUserActivation();

  MaybeCreateFedCmMetrics();
  int old_session_id = fedcm_metrics_->session_id();
  fedcm_metrics_->SetSessionID(webid::GetNewSessionID());
  // Store the previous `idp_order_` value from this class. Note that this is {}
  // unless there is a pending request from the same RFH. In particular, this is
  // still {} if there is a pending request but from a different RFH.
  std::vector<GURL> old_idp_order = std::move(idp_order_);
  idp_order_ = {};
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      idp_order_.push_back(idp_ptr->config->config_url);
    }
  }

  if (HasPendingRequest()) {
    FederatedAuthRequestImpl* pending_request =
        webid::GetPageData(render_frame_host().GetPage())
            ->PendingWebIdentityRequest();

    RpMode pending_request_rp_mode = pending_request->GetRpMode();
    RpMode new_request_rp_mode = idp_get_params_ptrs[0]->mode;
    fedcm_metrics_->RecordMultipleRequestsRpMode(
        pending_request_rp_mode, new_request_rp_mode, idp_order_);

    bool can_replace_pending_request =
        IsFedCmActiveModeEnabled() && had_transient_user_activation_ &&
        new_request_rp_mode == RpMode::kActive &&
        pending_request_rp_mode != RpMode::kActive;
    if (!can_replace_pending_request) {
      // Cancel this new request.
      fedcm_metrics_->RecordRequestTokenStatus(
          TokenStatus::kTooManyRequests, requirement, idp_order_,
          /*num_idps_mismatch=*/0,
          /*selected_idp_config_url=*/std::nullopt,
          (IsFedCmActiveModeEnabled() &&
           idp_get_params_ptrs[0]->mode == blink::mojom::RpMode::kActive)
              ? RpMode::kActive
              : RpMode::kPassive);

      AddDevToolsIssue(
          blink::mojom::FederatedAuthRequestResult::kTooManyRequests);
      AddConsoleErrorMessage(
          blink::mojom::FederatedAuthRequestResult::kTooManyRequests);

      std::move(callback).Run(RequestTokenStatus::kErrorTooManyRequests,
                              std::nullopt, "", /*error=*/nullptr,
                              /*is_auto_selected=*/false);
      // The old request is alive, so set the session ID to the old one.
      fedcm_metrics_->SetSessionID(old_session_id);
      idp_order_ = std::move(old_idp_order);
      return;
    }

    // Cancel the pending request before starting the new active flow request.
    // Set the old values before completing in case the pending request
    // corresponds to one in this object.
    int new_session_id = fedcm_metrics_->session_id();
    std::vector<GURL> new_idp_order = std::move(idp_order_);
    fedcm_metrics_->SetSessionID(old_session_id);
    idp_order_ = std::move(old_idp_order);
    pending_request->CompleteRequestWithError(
        FederatedAuthRequestResult::kReplacedByActiveMode,
        TokenStatus::kReplacedByActiveMode,
        /*token_error=*/std::nullopt,
        /*should_delay_callback=*/false);
    CHECK(!auth_request_token_callback_);

    // Some members were reset to false during CleanUp when replacing a passive
    // flow from the same frame so we need to set them again.
    had_transient_user_activation_ = true;
    fedcm_metrics_->SetSessionID(new_session_id);
    idp_order_ = std::move(new_idp_order);
  }

  should_complete_request_immediately_ = should_complete_request_immediately;
  mediation_requirement_ = requirement;
  auth_request_token_callback_ = std::move(callback);
  webid::GetPageData(render_frame_host().GetPage())
      ->SetPendingWebIdentityRequest(this);
  network_manager_ = CreateNetworkManager();
  request_dialog_controller_ = CreateDialogController();
  start_time_ = base::TimeTicks::Now();
  // TODO(crbug.com/40218857): handle active mode with multiple IdP.
  if (IsFedCmActiveModeEnabled() &&
      idp_get_params_ptrs[0]->mode == blink::mojom::RpMode::kActive) {
    rp_mode_ = RpMode::kActive;
    std::optional<base::TimeTicks> user_info_accounts_response_time =
        webid::GetPageData(render_frame_host().GetPage())
            ->ConsumeUserInfoAccountsResponseTime(
                idp_get_params_ptrs[0]->providers[0]->config->config_url);
    if (user_info_accounts_response_time) {
      fedcm_metrics_->RecordTimeBetweenUserInfoAndActiveModeAPI(
          start_time_ - user_info_accounts_response_time.value());
    }
    if (!had_transient_user_activation_) {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kMissingTransientUserActivation,
          TokenStatus::kMissingTransientUserActivation,
          /*token_error=*/std::nullopt,
          /*should_delay_callback=*/false);
      return;
    }
  } else {
    rp_mode_ = RpMode::kPassive;
  }

  if (origin().opaque()) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque,
        TokenStatus::kRpOriginIsOpaque,
        /*token_error=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  FederatedApiPermissionStatus permission_status = GetApiPermissionStatus();

  std::optional<TokenStatus> error_token_status;
  FederatedAuthRequestResult request_result =
      FederatedAuthRequestResult::kError;

  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_token_status = TokenStatus::kDisabledInFlags;
      request_result = FederatedAuthRequestResult::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      error_token_status = TokenStatus::kDisabledInSettings;
      request_result = FederatedAuthRequestResult::kDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      error_token_status = TokenStatus::kDisabledEmbargo;
      request_result = FederatedAuthRequestResult::kDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (error_token_status && rp_mode_ == RpMode::kPassive) {
    CompleteRequestWithError(request_result, *error_token_status,
                             /*token_error=*/std::nullopt,
                             /*should_delay_callback=*/true);
    return;
  }

  ++num_requests_;

  std::set<GURL> unique_idps;
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      // Throw an error if duplicate IDPs are specified.
      const bool is_unique_idp =
          unique_idps.insert(idp_ptr->config->config_url).second;
      if (!is_unique_idp) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 /*token_status=*/std::nullopt,
                                 /*token_error=*/std::nullopt,
                                 /*should_delay_callback=*/false);
        return;
      }

      url::Origin idp_origin = url::Origin::Create(idp_ptr->config->config_url);
      if (!network::IsOriginPotentiallyTrustworthy(idp_origin)) {
        CompleteRequestWithError(
            FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy,
            TokenStatus::kIdpNotPotentiallyTrustworthy,
            /*token_error=*/std::nullopt,
            /*should_delay_callback=*/false);
        return;
      }
    }
  }

  bool any_idp_has_custom_scopes = false;
  bool any_idp_has_parameters = false;
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      bool has_failing_idp_signin_status =
          webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
              render_frame_host(), idp_ptr->config->config_url,
              permission_delegate_);

      url::Origin idp_origin = url::Origin::Create(idp_ptr->config->config_url);
      if (has_failing_idp_signin_status &&
          webid::GetIdpSigninStatusMode(render_frame_host(), idp_origin) ==
              FedCmIdpSigninStatusMode::ENABLED) {
        if (idp_get_params_ptr->mode == blink::mojom::RpMode::kPassive) {
          if (IsFedCmMultipleIdentityProvidersEnabled()) {
            // In the multi IDP case, we do not want to complete the request
            // right away as there are other IDPs which may be logged in. But we
            // also do not want to fetch this IDP.
            unique_idps.erase(idp_ptr->config->config_url);
            continue;
          }
          // If the user is known to be signed-out and the RP is request
          // a passive, we fail the request early before fetching anything.
          CompleteRequestWithError(
              FederatedAuthRequestResult::kNotSignedInWithIdp,
              TokenStatus::kNotSignedInWithIdp,
              /*token_error=*/std::nullopt,
              /*should_delay_callback=*/true);
          return;
        } else if (idp_get_params_ptr->mode == blink::mojom::RpMode::kActive) {
          // Only a compromised renderer can set mode = active without the
          // ActiveMode enabled (which controls the JS WebIDL), so we crash
          // here if we ever get to this situation.
          if (!IsFedCmActiveModeEnabled()) {
            ReportBadMessageAndDeleteThis("FedCM active mode is not enabled.");
            return;
          }
          // We fail sooner before, but just to double check, we assert that
          // we are inside a user gesture here again.
          CHECK(had_transient_user_activation_);
        }
      }
      if (ShouldFailBeforeFetchingAccounts(idp_ptr->config->config_url)) {
        if (IsFedCmMultipleIdentityProvidersEnabled()) {
          // In the multi IDP case, we do not want to complete the request right
          // away as there are other IDPs which may be logged in. But we also do
          // not want to fetch this IDP.
          unique_idps.erase(idp_ptr->config->config_url);
          continue;
        }
        CompleteRequestWithError(
            FederatedAuthRequestResult::kSilentMediationFailure,
            TokenStatus::kSilentMediationFailure,
            /*token_error=*/std::nullopt,
            /*should_delay_callback=*/false);
        return;
      }

      if (webid::IsFedCmAuthzEnabled(render_frame_host(), idp_origin)) {
        any_idp_has_custom_scopes =
            any_idp_has_custom_scopes || GetDisclosureFields(*idp_ptr).empty();
        any_idp_has_parameters =
            any_idp_has_parameters || !idp_ptr->params.empty();
      }

      blink::mojom::RpContext rp_context = idp_get_params_ptr->context;
      blink::mojom::RpMode rp_mode = idp_get_params_ptr->mode;
      const GURL& idp_config_url = idp_ptr->config->config_url;
      token_request_get_infos_.emplace(
          idp_config_url,
          IdentityProviderGetInfo(std::move(idp_ptr), rp_context, rp_mode));
    }
  }
  if (any_idp_has_parameters || any_idp_has_custom_scopes) {
    FedCmRpParameters parameters;
    if (any_idp_has_custom_scopes && any_idp_has_parameters) {
      parameters = FedCmRpParameters::kHasParametersAndNonDefaultScope;
    } else if (any_idp_has_parameters) {
      parameters = FedCmRpParameters::kHasParameters;
    } else {
      DCHECK(any_idp_has_custom_scopes);
      parameters = FedCmRpParameters::kHasNonDefaultScope;
    }
    fedcm_metrics_->RecordRpParameters(parameters);
  }

  if (IsFedCmMultipleIdentityProvidersEnabled() && unique_idps.empty()) {
    // At this point either all IDPs are signed out or mediation:silent was used
    // and there are no returning accounts.
    bool should_delay_callback =
        mediation_requirement_ == MediationRequirement::kSilent ? false : true;
    auto result = mediation_requirement_ == MediationRequirement::kSilent
                      ? FederatedAuthRequestResult::kSilentMediationFailure
                      : FederatedAuthRequestResult::kNotSignedInWithIdp;
    auto token_status = mediation_requirement_ == MediationRequirement::kSilent
                            ? TokenStatus::kSilentMediationFailure
                            : TokenStatus::kNotSignedInWithIdp;
    CompleteRequestWithError(result, token_status, /*token_error=*/std::nullopt,
                             should_delay_callback);
    return;
  }

  // Show loading dialog while fetching endpoints if it is a active flow. This
  // is needed even if the LoginStatus is "logged-out" because we need to fetch
  // the config file to get the login_url which may take some time.
  if (rp_mode_ == RpMode::kActive) {
    CHECK(idp_order_.size() > 0);
    // TODO(crbug.com/40218857): Handle active mode with multiple IdP.
    const GURL& idp_config_url = idp_order_[0];
    auto get_info_it = token_request_get_infos_.find(idp_config_url);
    CHECK(get_info_it != token_request_get_infos_.end());
    if (!request_dialog_controller_->ShowLoadingDialog(
            GetTopFrameOriginForDisplay(GetEmbeddingOrigin()),
            FormatOriginForDisplay(url::Origin::Create(idp_config_url)),
            get_info_it->second.rp_context, rp_mode_,
            base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                           weak_ptr_factory_.GetWeakPtr()))) {
      return;
    }
  }

  CHECK(!unique_idps.empty());
  FetchEndpointsForIdps(std::move(unique_idps), /*for_idp_signin=*/false);
}

void FederatedAuthRequestImpl::RequestUserInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    RequestUserInfoCallback callback) {
  if (!render_frame_host().GetPage().IsPrimary()) {
    ReportBadMessageAndDeleteThis(
        "FedCM should not be allowed in nested frame trees.");
    return;
  }
  // FedCMMetrics class is currently not used for UserInfo API. If we log UKM
  // metrics later on, we should call MaybeCreateFedCmMetrics() here.

  auto network_manager = IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
  auto user_info_request = FederatedAuthUserInfoRequest::Create(
      std::move(network_manager), permission_delegate_,
      api_permission_delegate_, &render_frame_host(), std::move(provider));
  FederatedAuthUserInfoRequest* user_info_request_ptr = user_info_request.get();
  user_info_requests_.insert(std::move(user_info_request));

  user_info_request_ptr->SetCallbackAndStart(
      base::BindOnce(&FederatedAuthRequestImpl::CompleteUserInfoRequest,
                     weak_ptr_factory_.GetWeakPtr(), user_info_request_ptr,
                     std::move(callback)));
}

void FederatedAuthRequestImpl::CancelTokenRequest() {
  if (!auth_request_token_callback_) {
    // This can happen if the renderer requested an abort() after the browser
    // invoked the callback but before the renderer received the callback.
    return;
  }

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.

  CompleteRequestWithError(FederatedAuthRequestResult::kCanceled,
                           TokenStatus::kAborted,
                           /*token_error=*/std::nullopt,
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::ResolveTokenRequest(
    const std::optional<std::string>& account_id,
    const std::string& token,
    ResolveTokenRequestCallback callback) {
  if (!webid::IsFedCmAuthzEnabled(render_frame_host(), origin())) {
    std::move(callback).Run(false);
    return;
  }

  if (!identity_registry_ && !SetupIdentityRegistryFromPopup()) {
    std::move(callback).Run(false);
    return;
  }

  bool accepted =
      identity_registry_->NotifyResolve(origin(), account_id, token);
  std::move(callback).Run(accepted);
}

void FederatedAuthRequestImpl::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    blink::mojom::IdpSigninStatus status) {
  if (render_frame_host().IsNestedWithinFencedFrame()) {
    RecordSetLoginStatusIgnoredReason(
        FedCmSetLoginStatusIgnoredReason::kInFencedFrame);
    return;
  }
  // We only allow setting the IDP signin status when the subresource is loaded
  // from the same site as the document, and the document is same site with
  // all ancestors. This is to protect from an RP embedding a tracker resource
  // that would set this signin status for the tracker, enabling the FedCM
  // request.
  if (!webid::IsSameSiteWithAncestors(idp_origin, &render_frame_host())) {
    RecordSetLoginStatusIgnoredReason(
        FedCmSetLoginStatusIgnoredReason::kCrossOrigin);
    return;
  }
  permission_delegate_->SetIdpSigninStatus(
      idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

void FederatedAuthRequestImpl::RegisterIdP(const GURL& idp,
                                           RegisterIdPCallback callback) {
  if (!IsFedCmIdPRegistrationEnabled()) {
    std::move(callback).Run(RegisterIdpStatus::kErrorFeatureDisabled);
    return;
  }

  if (!origin().IsSameOriginWith(url::Origin::Create(idp))) {
    std::move(callback).Run(RegisterIdpStatus::kErrorCrossOriginConfig);
    return;
  }

  if (!render_frame_host().HasTransientUserActivation()) {
    std::move(callback).Run(RegisterIdpStatus::kErrorNoTransientActivation);
    return;
  }

  if (!request_dialog_controller_) {
    request_dialog_controller_ = CreateDialogController();
  }

  request_dialog_controller_->RequestIdPRegistrationPermision(
      origin(),
      base::BindOnce(&FederatedAuthRequestImpl::OnRegisterIdPPermissionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), idp));
}

void FederatedAuthRequestImpl::OnRegisterIdPPermissionResponse(
    RegisterIdPCallback callback,
    const GURL& idp,
    bool accepted) {
  if (accepted) {
    permission_delegate_->RegisterIdP(idp);
  }
  std::move(callback).Run(accepted ? RegisterIdpStatus::kSuccess
                                   : RegisterIdpStatus::kErrorDeclined);
}

void FederatedAuthRequestImpl::UnregisterIdP(const GURL& idp,
                                             UnregisterIdPCallback callback) {
  if (!IsFedCmIdPRegistrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  if (!origin().IsSameOriginWith(url::Origin::Create(idp))) {
    std::move(callback).Run(false);
    return;
  }
  permission_delegate_->UnregisterIdP(idp);
  std::move(callback).Run(true);
}

void FederatedAuthRequestImpl::OnIdpSigninStatusReceived(
    const url::Origin& idp_config_origin,
    bool idp_signin_status) {
  if (!idp_signin_status) {
    return;
  }

  for (const auto& [get_idp_config_url, get_info] : token_request_get_infos_) {
    if (url::Origin::Create(get_idp_config_url) == idp_config_origin) {
      permission_delegate_->RemoveIdpSigninStatusObserver(this);
      FetchEndpointsForIdps({get_idp_config_url}, /*for_idp_signin=*/true);
      break;
    }
  }
}

bool FederatedAuthRequestImpl::HasPendingRequest() const {
  bool has_pending_request = webid::GetPageData(render_frame_host().GetPage())
                                 ->PendingWebIdentityRequest() != nullptr;
  DCHECK(has_pending_request || !auth_request_token_callback_);
  return has_pending_request;
}

void FederatedAuthRequestImpl::FetchEndpointsForIdps(
    const std::set<GURL>& idp_config_urls,
    bool for_idp_signin) {
  int icon_ideal_size =
      request_dialog_controller_->GetBrandIconIdealSize(rp_mode_);
  int icon_minimum_size =
      request_dialog_controller_->GetBrandIconMinimumSize(rp_mode_);

  {
    std::set<GURL> pending_idps = std::move(fetch_data_.pending_idps);
    pending_idps.insert(idp_config_urls.begin(), idp_config_urls.end());
    fetch_data_ = FetchData();
    fetch_data_.pending_idps = std::move(pending_idps);
    fetch_data_.for_idp_signin = for_idp_signin;
  }

  provider_fetcher_ = std::make_unique<FederatedProviderFetcher>(
      render_frame_host(), network_manager_.get());
  provider_fetcher_->Start(
      fetch_data_.pending_idps, rp_mode_, icon_ideal_size, icon_minimum_size,
      base::BindOnce(&FederatedAuthRequestImpl::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnAllConfigAndWellKnownFetched(
    std::vector<FederatedProviderFetcher::FetchResult> fetch_results) {
  provider_fetcher_.reset();

  well_known_and_config_fetched_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordWellKnownAndConfigFetchTime(
      well_known_and_config_fetched_time_ - start_time_);

  for (const FederatedProviderFetcher::FetchResult& fetch_result :
       fetch_results) {
    const GURL& identity_provider_config_url =
        fetch_result.identity_provider_config_url;
    auto get_info_it =
        token_request_get_infos_.find(identity_provider_config_url);
    CHECK(get_info_it != token_request_get_infos_.end());

    metrics_endpoints_[identity_provider_config_url] =
        fetch_result.endpoints.metrics;

    std::unique_ptr<IdentityProviderInfo> idp_info =
        std::make_unique<IdentityProviderInfo>(
            get_info_it->second.provider, std::move(fetch_result.endpoints),
            fetch_result.metadata ? std::move(*fetch_result.metadata)
                                  : IdentityProviderMetadata(),
            get_info_it->second.rp_context, get_info_it->second.rp_mode);

    if (fetch_result.error) {
      const FederatedProviderFetcher::FetchError& fetch_error =
          *fetch_result.error;
      if (fetch_error.additional_console_error_message) {
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            *fetch_error.additional_console_error_message);
      }
      OnFetchDataForIdpFailed(
          std::move(idp_info), fetch_error.result, fetch_error.token_status,
          /*should_delay_callback=*/rp_mode_ == RpMode::kPassive);
      continue;
    }

    if (IsFedCmIdPRegistrationEnabled()) {
      if (get_info_it->second.provider->config->type) {
        if (!base::Contains(fetch_result.metadata->types,
                            get_info_it->second.provider->config->type)) {
          OnFetchDataForIdpFailed(
              std::move(idp_info),
              blink::mojom::FederatedAuthRequestResult::kTypeNotMatching,
              content::FedCmRequestIdTokenStatus::kConfigNotMatchingType,
              /*should_delay_callback=*/false);
          continue;
        }
      }
    }

    if (!IsFedCmFlexibleFieldsEnabled()) {
      const auto& fields = get_info_it->second.provider->fields;
      if (fields && !fields->empty()) {
        // If one of the default fields is present, all three must be present
        // for now. We may relax this requirement in the future based on IDP
        // opt-in, so we do this check here (as opposed to in RequestToken)
        // so that the timing of the promise rejection and the network
        // requests do not change if/when we do that.
        // We only reject in this limited circumstance (and allow unknown
        // fields) for forward compatibility.
        bool contains_name = base::Contains(*fields, kDefaultFieldName);
        bool contains_email = base::Contains(*fields, kDefaultFieldEmail);
        bool contains_picture = base::Contains(*fields, kDefaultFieldPicture);
        if (contains_name || contains_email || contains_picture) {
          if (!(contains_name && contains_email && contains_picture)) {
            CompleteRequestWithError(
                FederatedAuthRequestResult::kInvalidFieldsSpecified,
                TokenStatus::kInvalidFieldsSpecified,
                /*token_error=*/std::nullopt,
                /*should_delay_callback=*/false);
            return;
          }
        }
      }
    }

    // The login url should be valid unless IdP login status API is disabled.
    if (idp_info->metadata.idp_login_url.is_valid()) {
      idp_login_infos_[idp_info->metadata.idp_login_url] = {
          idp_info->provider->login_hint, idp_info->provider->domain_hint};
    }

    // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
    // false during the API call. e.g. by the login/logout HEADER.
    // In the active flow we get here even if the IDP sign-in bit was false
    // originally, because we need the well-known and config files to find the
    // login URL.
    idp_info->has_failing_idp_signin_status =
        webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
            render_frame_host(), identity_provider_config_url,
            permission_delegate_);
    if (idp_info->has_failing_idp_signin_status &&
        webid::GetIdpSigninStatusMode(
            render_frame_host(),
            url::Origin::Create(identity_provider_config_url)) ==
            FedCmIdpSigninStatusMode::ENABLED) {
      // If the user is logged out and we are in a active-mode, allow the user
      // to sign-in to the IdP and return early.
      if (rp_mode_ == blink::mojom::RpMode::kActive) {
        MaybeShowActiveModeModalDialog(identity_provider_config_url,
                                       idp_info->metadata.idp_login_url);
        return;
      }
      // Do not send metrics for IDP where the user is not signed-in in order
      // to prevent IDP from using the user IP to make a probabilistic model
      // of which websites a user visits.
      idp_info->endpoints.metrics = GURL();

      OnFetchDataForIdpFailed(std::move(idp_info),
                              FederatedAuthRequestResult::kNotSignedInWithIdp,
                              TokenStatus::kNotSignedInWithIdp,
                              /*should_delay_callback=*/true);
      continue;
    }

    GURL accounts_endpoint = idp_info->endpoints.accounts;
    std::string client_id = idp_info->provider->config->client_id;
    const GURL& config_url = idp_info->provider->config->config_url;
    network_manager_->SendAccountsRequest(
        accounts_endpoint, client_id,
        base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(idp_info)));
    fedcm_metrics_->RecordAccountsRequestSent(config_url);
  }
}

void FederatedAuthRequestImpl::CompleteDisconnectRequest(
    DisconnectCallback callback,
    blink::mojom::DisconnectStatus status) {
  // `disconnect_request_` may be null here if the completion is invoked from
  // the FederatedAuthRequestImpl destructor, which destroys
  // `disconnect_request_`. The FederatedAuthDisconnectRequest destructor would
  // trigger the callback.
  if (!disconnect_request_ &&
      status == blink::mojom::DisconnectStatus::kSuccess) {
    NOTREACHED_IN_MIGRATION()
        << "The successful disconnect request is nowhere to be found";
    return;
  }
  std::move(callback).Run(status);
  disconnect_request_.reset();
}

void FederatedAuthRequestImpl::OnClientMetadataResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::vector<IdentityRequestAccountPtr>&& accounts,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata client_metadata) {
  client_metadata_fetched_time_ = base::TimeTicks::Now();

  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.
  if (!idp_info->metadata.brand_background_color &&
      idp_info->metadata.brand_text_color) {
    idp_info->metadata.brand_text_color = std::nullopt;
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "The FedCM text color is ignored because background color was not "
        "provided");
  }
  if (idp_info->metadata.brand_background_color &&
      idp_info->metadata.brand_text_color) {
    float text_contrast_ratio = color_utils::GetContrastRatio(
        *idp_info->metadata.brand_background_color,
        *idp_info->metadata.brand_text_color);
    if (text_contrast_ratio < color_utils::kMinimumReadableContrastRatio) {
      idp_info->metadata.brand_text_color = std::nullopt;
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "The FedCM text color is ignored because it does not contrast enough "
          "with the provided background color");
    }
  }
  FetchAccountPictures(std::move(idp_info), std::move(accounts),
                       client_metadata);
}

std::vector<IdentityRequestDialogDisclosureField>
FederatedAuthRequestImpl::GetDisclosureFields(
    const blink::mojom::IdentityProviderRequestOptions& provider) {
  const std::vector<IdentityRequestDialogDisclosureField> kDefaultPermissions =
      {IdentityRequestDialogDisclosureField::kName,
       IdentityRequestDialogDisclosureField::kEmail,
       IdentityRequestDialogDisclosureField::kPicture};

  url::Origin idp_origin = url::Origin::Create(provider.config->config_url);
  if (!webid::IsFedCmAuthzEnabled(render_frame_host(), idp_origin)) {
    return kDefaultPermissions;
  }

  const auto& fields = provider.fields;
  if (!fields) {
    // If "fields" is not passed, defaults the parameter to
    // ["name", "email" and "picture"].
    return kDefaultPermissions;
  }

  // If fields is explicitly empty, we should not mediate.
  if (fields->empty()) {
    return {};
  }

  std::vector<IdentityRequestDialogDisclosureField> list;
  for (const auto& field : *fields) {
    if (field == kDefaultFieldName) {
      list.push_back(IdentityRequestDialogDisclosureField::kName);
    } else if (field == kDefaultFieldEmail) {
      list.push_back(IdentityRequestDialogDisclosureField::kEmail);
    } else if (field == kDefaultFieldPicture) {
      list.push_back(IdentityRequestDialogDisclosureField::kPicture);
    }
  }
  return list;
}

bool FederatedAuthRequestImpl::CanShowContinueOnPopup() const {
  if (mediation_requirement_ == MediationRequirement::kSilent) {
    return false;
  }

  if (mediation_requirement_ == MediationRequirement::kRequired) {
    // In this case, we always have a user gesture (the user had to choose
    // an account), so we can show a popup.
    return true;
  }

  if (identity_selection_type_ == kExplicit) {
    return true;
  }
  DCHECK_EQ(identity_selection_type_, kAutoPassive);
  return had_transient_user_activation_;
}

void FederatedAuthRequestImpl::OnFetchDataForIdpSucceeded(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::vector<IdentityRequestAccountPtr>&& accounts,
    const IdpNetworkRequestManager::ClientMetadata& client_metadata) {
  fetch_data_.did_succeed_for_at_least_one_idp = true;

  const GURL& idp_config_url = idp_info->provider->config->config_url;

  std::vector<IdentityRequestDialogDisclosureField> disclosure_fields =
      GetDisclosureFields(*idp_info->provider);

  const std::string idp_for_display =
      webid::FormatUrlForDisplay(idp_config_url);
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      idp_for_display, idp_info->metadata,
      ClientMetadata{client_metadata.terms_of_service_url,
                     client_metadata.privacy_policy_url,
                     client_metadata.brand_icon_url},
      idp_info->rp_context, disclosure_fields,
      /*has_login_status_mismatch=*/false);
  for (auto& account : accounts) {
    account->identity_provider = idp_info->data;
  }
  // If the IDP data existed before, we need to remove the old accounts data.
  // This can happen with the 'use other account' feature.
  if (idp_infos_.find(idp_config_url) != idp_infos_.end()) {
    std::erase_if(accounts_, [&idp_config_url](const auto& account) {
      return account->identity_provider->idp_metadata.config_url ==
             idp_config_url;
    });
  }
  idp_infos_[idp_config_url] = std::move(idp_info);
  idp_accounts_[idp_config_url] = std::move(accounts);

  fetch_data_.pending_idps.erase(idp_config_url);
  MaybeShowAccountsDialog();
}

void FederatedAuthRequestImpl::OnFetchDataForIdpFailed(
    const std::unique_ptr<IdentityProviderInfo> idp_info,
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<TokenStatus> token_status,
    bool should_delay_callback) {
  const GURL& idp_config_url = idp_info->provider->config->config_url;
  fetch_data_.pending_idps.erase(idp_config_url);

  if (fetch_data_.pending_idps.empty() &&
      !fetch_data_.did_succeed_for_at_least_one_idp) {
    CompleteRequestWithError(result, token_status,
                             /*token_error=*/std::nullopt,
                             should_delay_callback);
    return;
  }

  AddDevToolsIssue(result);
  AddConsoleErrorMessage(result);

  if (IsFedCmMetricsEndpointEnabled()) {
    SendFailedTokenRequestMetrics(idp_info->endpoints.metrics, result);
  }

  metrics_endpoints_.erase(idp_config_url);

  // We do not call both OnFetchDataForIdpFailed() after OnFetchDataSucceeded()
  // for the same IDP.
  DCHECK(idp_infos_.find(idp_config_url) == idp_infos_.end());
  MaybeShowAccountsDialog();
}

void FederatedAuthRequestImpl::MaybeShowAccountsDialog() {
  if (!fetch_data_.pending_idps.empty()) {
    return;
  }

  // The accounts fetch could be delayed for legitimate reasons. A user may be
  // able to disable FedCM API (e.g. via settings or dismissing another FedCM UI
  // on the same RP origin) before the browser receives the accounts response.
  // We should exit early without showing any UI.
  // Note that for the active flow is not affected by the permission status.
  if (GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED &&
      rp_mode_ != RpMode::kActive) {
    CompleteRequestWithError(FederatedAuthRequestResult::kDisabledInSettings,
                             TokenStatus::kDisabledInSettings,
                             /*token_error=*/std::nullopt,
                             /*should_delay_callback=*/true);
    return;
  }

  // This map may have contents already if we came here through the "Add
  // Account" flow or the IDP login mismatch in multiple IDP case.
  idp_data_for_display_.clear();

  for (const auto& idp : idp_order_) {
    auto idp_info_it = idp_infos_.find(idp);
    if (idp_info_it != idp_infos_.end() && idp_info_it->second->data) {
      idp_data_for_display_.push_back(idp_info_it->second->data);
    }
    auto accounts_it = idp_accounts_.find(idp);
    if (accounts_it != idp_accounts_.end()) {
      accounts_.insert(accounts_.end(),
                       std::make_move_iterator(accounts_it->second.begin()),
                       std::make_move_iterator(accounts_it->second.end()));
    }
  }
  idp_accounts_.clear();
  std::stable_sort(
      accounts_.begin(), accounts_.end(),
      [&](const auto& account1, const auto& account2) {
        // First, show newly logged in accounts, if any.
        bool is_account1_new = IsNewlyLoggedIn(*account1);
        bool is_account2_new = IsNewlyLoggedIn(*account2);
        if (is_account1_new || is_account2_new) {
          return !is_account2_new;
        }
        // Show returning accounts before non-returning.
        if (account1->login_state == LoginState::kSignUp ||
            account2->login_state == LoginState::kSignUp) {
          return account1->login_state == LoginState::kSignIn;
        }
        // Within returning accounts, prefer those with last used
        // timestamp.
        if (!account1->last_used_timestamp || !account2->last_used_timestamp) {
          return !!account1->last_used_timestamp;
        }
        // If both have last used timestamp, prefer the latest.
        return *account1->last_used_timestamp > *account2->last_used_timestamp;
      });
  // Copy the newly logged in accounts into `new_accounts_`, if there are any.
  new_accounts_.clear();
  for (const auto& account : accounts_) {
    if (IsNewlyLoggedIn(*account)) {
      new_accounts_.push_back(account);
    } else {
      // Since this is sorted, once not newly logged in, exit.
      break;
    }
  }
  account_ids_before_login_.clear();

  // TODO(crbug.com/40246099): Handle auto_reauthn_ for multi IDP.
  bool auto_reauthn_enabled =
      mediation_requirement_ != MediationRequirement::kRequired;

  dialog_type_ = auto_reauthn_enabled ? kAutoReauth : kSelectAccount;
  bool is_auto_reauthn_setting_enabled = false;
  bool is_auto_reauthn_embargoed = false;
  std::optional<base::TimeDelta> time_from_embargo;
  bool requires_user_mediation = false;
  IdentityProviderDataPtr auto_reauthn_idp = nullptr;
  IdentityRequestAccountPtr auto_reauthn_account = nullptr;
  bool has_single_returning_account = false;
  if (auto_reauthn_enabled) {
    is_auto_reauthn_setting_enabled =
        auto_reauthn_permission_delegate_->IsAutoReauthnSettingEnabled();
    is_auto_reauthn_embargoed =
        auto_reauthn_permission_delegate_->IsAutoReauthnEmbargoed(
            GetEmbeddingOrigin());
    if (is_auto_reauthn_embargoed) {
      time_from_embargo =
          base::Time::Now() -
          auto_reauthn_permission_delegate_->GetAutoReauthnEmbargoStartTime(
              GetEmbeddingOrigin());

      // See `kFederatedIdentityAutoReauthnEmbargoDuration`.
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          "Auto re-authn was previously triggered less than 10 minutes ago. "
          "Only one auto re-authn request can be made every 10 minutes.");
    }
    requires_user_mediation = RequiresUserMediation();
    // Auto signs in returning users if they have a single returning account and
    // are signing in.
    has_single_returning_account =
        GetAccountForAutoReauthn(&auto_reauthn_idp, &auto_reauthn_account);
    if (dialog_type_ == kAutoReauth &&
        (requires_user_mediation || !is_auto_reauthn_setting_enabled ||
         is_auto_reauthn_embargoed || !has_single_returning_account)) {
      dialog_type_ = kSelectAccount;
    }
    if (!has_single_returning_account &&
        mediation_requirement_ == MediationRequirement::kSilent) {
      fedcm_metrics_->RecordAutoReauthnMetrics(
          has_single_returning_account, auto_reauthn_account.get(),
          dialog_type_ == kAutoReauth, !is_auto_reauthn_setting_enabled,
          is_auto_reauthn_embargoed, time_from_embargo,
          requires_user_mediation);

      // By this moment we know that the user has granted permission in the past
      // for the RP/IdP. Because otherwise we have returned already in
      // `ShouldFailBeforeFetchingAccounts`. It means that we can do the
      // following without privacy cost:
      // 1. Reject the promise immediately without delay
      // 2. Not to show any UI to respect `mediation: silent`
      // TODO(crbug.com/40266561): validate the statement above with
      // stakeholders
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Silent mediation issue: the user has used FedCM with multiple "
          "accounts on this site.");
      CompleteRequestWithError(
          FederatedAuthRequestResult::kSilentMediationFailure,
          TokenStatus::kSilentMediationFailure,
          /*token_error=*/std::nullopt,
          /*should_delay_callback=*/false);
      return;
    }

    if (dialog_type_ == kAutoReauth) {
      accounts_ = {auto_reauthn_account};
      idp_data_for_display_ = {auto_reauthn_idp};
      new_accounts_.clear();
      accounts_[0]->identity_provider = idp_data_for_display_[0];
    }
  }

  if (dialog_type_ != kAutoReauth) {
    identity_selection_type_ = kExplicit;
  } else if (!IsFedCmActiveModeEnabled() ||
             rp_mode_ == blink::mojom::RpMode::kPassive) {
    identity_selection_type_ = kAutoPassive;
  } else {
    identity_selection_type_ = kAutoActive;
  }

  if (auto_reauthn_enabled) {
    fedcm_metrics_->RecordAutoReauthnMetrics(
        has_single_returning_account, auto_reauthn_account.get(),
        dialog_type_ == kAutoReauth, !is_auto_reauthn_setting_enabled,
        is_auto_reauthn_embargoed, time_from_embargo, requires_user_mediation);
  }

  // The RenderFrameHost may be alive but not visible in the following
  // situations:
  // Situation #1: User switched tabs
  // Situation #2: User navigated the page with bfcache
  //
  // - If this fetch is as a result of an IdP sign-in status change, the FedCM
  // dialog is either visible or temporarily hidden. Update the contents of
  // the dialog.
  // - If the FedCM dialog has not already been shown, do not show the dialog
  // if the RenderFrameHost is hidden because the user does not seem interested
  // in the contents of the current page.
  if (!fetch_data_.for_idp_signin) {
    bool is_active = IsFrameActive(render_frame_host().GetMainFrame());
    fedcm_metrics_->RecordWebContentsStatusUponReadyToShowDialog(
        IsFrameVisible(render_frame_host().GetMainFrame()), is_active);

    if (!is_active) {
      CompleteRequestWithError(FederatedAuthRequestResult::kRpPageNotVisible,
                               TokenStatus::kRpPageNotVisible,
                               /*token_error=*/std::nullopt,
                               /*should_delay_callback=*/true);
      return;
    }

    ready_to_display_accounts_dialog_time_ = base::TimeTicks::Now();
    fedcm_metrics_->RecordShowAccountsDialogTime(
        idp_data_for_display_,
        ready_to_display_accounts_dialog_time_ - start_time_);

    fedcm_metrics_->RecordShowAccountsDialogTimeBreakdown(
        well_known_and_config_fetched_time_ - start_time_,
        accounts_fetched_time_ - well_known_and_config_fetched_time_,
        client_metadata_fetched_time_ != base::TimeTicks()
            ? client_metadata_fetched_time_ - accounts_fetched_time_
            : base::TimeDelta());
  }
  bool did_succeed_for_at_least_one_idp =
      fetch_data_.did_succeed_for_at_least_one_idp;

  fetch_data_ = FetchData();

  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());

  bool intercept = false;
  // In tests (content_shell or when --use-fake-ui-for-fedcm is used), the
  // dialog controller will immediately select an account. But if browser
  // automation is enabled, we don't want that to happen because automation
  // should be able to choose which account to select or to cancel.
  // So we use this call to see whether interception is enabled.
  // It is not needed in regular Chrome even when automation is used because
  // there, the dialog will wait for user input anyway.
  devtools_instrumentation::WillShowFedCmDialog(render_frame_host(),
                                                &intercept);
  // Since we don't reuse the controller for each request, and intercept
  // defaults to false, we only need to call this if intercept is true.
  if (intercept) {
    request_dialog_controller_->SetIsInterceptionEnabled(intercept);
  }

  // TODO(crbug.com/40245853): Handle UI where some IDPs are successful and some
  // IDPs are failing in the multi IDP case.
  // Note that ShowAccountsDialog() may result in the request being completed
  // immediately (for instance on Android when we cannot create a BottomSheet),
  // so invocations after this method should assume that the members may have
  // been cleaned up.
  if (!request_dialog_controller_->ShowAccountsDialog(
          GetTopFrameOriginForDisplay(GetEmbeddingOrigin()),
          idp_data_for_display_, accounts_,
          identity_selection_type_ == kExplicit ? SignInMode::kExplicit
                                                : SignInMode::kAuto,
          rp_mode_, new_accounts_,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&FederatedAuthRequestImpl::LoginToIdP,
                              weak_ptr_factory_.GetWeakPtr(),
                              /*can_append_hints=*/false),
          base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountsDisplayed,
                         weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }

  devtools_instrumentation::DidShowFedCmDialog(render_frame_host());

  if (identity_selection_type_ == kExplicit &&
      did_succeed_for_at_least_one_idp) {
    // We omit recording the accounts dialog shown metric for auto re-authn
    // because the metric is used to detect IDPs flashing UI. Auto re-authn
    // verifying UI cannot be flashed since it is destroyed automatically after
    // 3 seconds and cannot be destroyed earlier for a11y reasons.
    accounts_dialog_shown_time_ = base::TimeTicks::Now();
  }

  // Note that accounts dialog shown after mismatch dialog is also recorded.
  // Although not useful for catching malicious IDPs, it should only be a very
  // small percentage of the samples recorded.
  fedcm_metrics_->RecordAccountsDialogShown(idp_data_for_display_);
}

void FederatedAuthRequestImpl::OnAccountsDisplayed() {
  accounts_dialog_display_time_ = base::TimeTicks::Now();
}

void FederatedAuthRequestImpl::HandleAccountsFetchFailure(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::optional<bool> old_idp_signin_status,
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<TokenStatus> token_status) {
  url::Origin idp_origin =
      url::Origin::Create(idp_info->provider->config->config_url);
  FedCmIdpSigninStatusMode signin_status_mode =
      webid::GetIdpSigninStatusMode(render_frame_host(), idp_origin);
  if (!old_idp_signin_status.has_value() ||
      signin_status_mode == FedCmIdpSigninStatusMode::METRICS_ONLY) {
    if (rp_mode_ == blink::mojom::RpMode::kActive) {
      MaybeShowActiveModeModalDialog(idp_info->provider->config->config_url,
                                     idp_info->metadata.idp_login_url);
      return;
    }
    OnFetchDataForIdpFailed(std::move(idp_info), result, token_status,
                            /*should_delay_callback=*/true);
    return;
  }

  if (!IsFrameActive(render_frame_host().GetMainFrame())) {
    CompleteRequestWithError(FederatedAuthRequestResult::kRpPageNotVisible,
                             TokenStatus::kRpPageNotVisible,
                             /*token_error=*/std::nullopt,
                             /*should_delay_callback=*/true);
    return;
  }

  if (mediation_requirement_ == MediationRequirement::kSilent) {
    // By this moment we know that the user has granted permission in the past
    // for the RP/IdP. Because otherwise we have returned already in
    // `ShouldFailBeforeFetchingAccounts`. It means that we can do the
    // following without privacy cost:
    // 1. Reject the promise immediately without delay
    // 2. Not to show any UI to respect `mediation: silent`
    // TODO(crbug.com/40266561): validate the statement above with stakeholders
    OnFetchDataForIdpFailed(std::move(idp_info),
                            FederatedAuthRequestResult::kSilentMediationFailure,
                            TokenStatus::kSilentMediationFailure,
                            /*should_delay_callback=*/false);
    return;
  }

  OnIdpMismatch(std::move(idp_info));
}

void FederatedAuthRequestImpl::OnIdpMismatch(
    std::unique_ptr<IdentityProviderInfo> idp_info) {
  const GURL& idp_config_url = idp_info->provider->config->config_url;
  fetch_data_.pending_idps.erase(idp_config_url);

  const std::string idp_for_display =
      webid::FormatUrlForDisplay(idp_config_url);
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      idp_for_display, idp_info->metadata,
      ClientMetadata{GURL(), GURL(), GURL()}, idp_info->rp_context,
      GetDisclosureFields(*idp_info->provider),
      /*has_login_status_mismatch=*/true);
  idp_infos_[idp_config_url] = std::move(idp_info);

  if (!fetch_data_.pending_idps.empty()) {
    return;
  }

  // Invoke the accounts dialog flow if there is at least one account or more
  // than one IDP for which we should show the mismatch dialog.
  // TODO(crbug.com/331426009): make this code clearer by creating a separate
  // method for showing multiple mismatch UI.
  if (fetch_data_.did_succeed_for_at_least_one_idp || idp_infos_.size() > 1u) {
    MaybeShowAccountsDialog();
    // If there are no successful IDPs, this is the multi IDP case where all are
    // mismatch.
    if (!fetch_data_.did_succeed_for_at_least_one_idp) {
      mismatch_dialog_shown_time_ = base::TimeTicks::Now();
      has_shown_mismatch_ = true;
      devtools_instrumentation::DidShowFedCmDialog(render_frame_host());
    }
    return;
  }

  if (rp_mode_ == RpMode::kActive) {
    MaybeShowActiveModeModalDialog(
        idp_config_url, idp_infos_[idp_config_url]->metadata.idp_login_url);
    return;
  }

  ShowSingleIdpFailureDialog();
}

void FederatedAuthRequestImpl::ShowSingleIdpFailureDialog() {
  CHECK_EQ(idp_infos_.size(), 1u);
  IdentityProviderInfo* idp_info = idp_infos_.begin()->second.get();
  url::Origin idp_origin =
      url::Origin::Create(idp_info->provider->config->config_url);
  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());

  fetch_data_ = FetchData();

  // Set `idp_data_for_display_` so it is always the case that we can rely on it
  // to know which IDPs have been seen in the UI.
  CHECK(idp_info->data);
  idp_data_for_display_ = {idp_info->data};

  // If IdP login status mismatch dialog is already visible, calling
  // ShowFailureDialog() a 2nd time should notify the user that login
  // failed.
  dialog_type_ = kConfirmIdpLogin;
  config_url_ = idp_info->provider->config->config_url;
  login_url_ = idp_info->metadata.idp_login_url;

  // Store variables used in RecordMismatchDialogShown since they may be cleaned
  // up in ShowFailureDialog().
  bool has_shown_mismatch = has_shown_mismatch_;
  bool has_hints = !idp_info->provider->login_hint.empty() ||
                   !idp_info->provider->domain_hint.empty() ||
                   !idp_info->metadata.requested_label.empty();

  if (!request_dialog_controller_->ShowFailureDialog(
          GetTopFrameOriginForDisplay(GetEmbeddingOrigin()),
          FormatOriginForDisplay(idp_origin), idp_info->rp_context, rp_mode_,
          idp_info->metadata,
          base::BindOnce(&FederatedAuthRequestImpl::OnDismissFailureDialog,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&FederatedAuthRequestImpl::LoginToIdP,
                              weak_ptr_factory_.GetWeakPtr(),
                              /*can_append_hints=*/true))) {
    return;
  }

  CHECK(idp_data_for_display_.size() == 1u);
  fedcm_metrics_->RecordSingleIdpMismatchDialogShown(
      *idp_data_for_display_[0], has_shown_mismatch, has_hints);
  mismatch_dialog_shown_time_ = base::TimeTicks::Now();
  has_shown_mismatch_ = true;
  devtools_instrumentation::DidShowFedCmDialog(render_frame_host());
}

void FederatedAuthRequestImpl::CloseModalDialogView() {
#if BUILDFLAG(IS_ANDROID)
  SetupIdentityRegistryFromPopup();
#endif
  // Invoke OnClose on the opener.
  if (identity_registry_) {
    identity_registry_->NotifyClose(origin());
  }
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    std::vector<IdentityRequestAccountPtr> accounts) {
  accounts_fetched_time_ = base::TimeTicks::Now();

  GURL idp_config_url = idp_info->provider->config->config_url;
  const std::optional<bool> old_idp_signin_status =
      permission_delegate_->GetIdpSigninStatus(
          url::Origin::Create(idp_config_url));
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      render_frame_host(), idp_config_url, status,
      idp_info->has_failing_idp_signin_status, permission_delegate_);

  constexpr char kAccountsUrl[] = "accounts endpoint";
  switch (status.parse_status) {
    case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kAccountsHttpNotFound,
          TokenStatus::kAccountsHttpNotFound);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kAccountsNoResponse,
          TokenStatus::kAccountsNoResponse);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kAccountsInvalidResponse,
          TokenStatus::kAccountsInvalidResponse);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                                 FederatedAuthRequestResult::kAccountsListEmpty,
                                 TokenStatus::kAccountsListEmpty);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kAccountsInvalidContentType,
          TokenStatus::kAccountsInvalidContentType);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kSuccess: {
      RecordRawAccountsSize(accounts.size());
      if (webid::IsFedCmAuthzEnabled(render_frame_host(),
                                     url::Origin::Create(idp_config_url))) {
        if (!FilterAccountsWithLabel(idp_info->metadata.requested_label,
                                     accounts)) {
          // No accounts remain, so treat as account fetch failure.
          render_frame_host().AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError,
              "Accounts were received, but none matched the label.");
          // If there are no accounts after filtering based on the label,
          // treat this exactly the same as if we had received an empty accounts
          // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
          HandleAccountsFetchFailure(
              std::move(idp_info), old_idp_signin_status,
              FederatedAuthRequestResult::kAccountsListEmpty,
              TokenStatus::kAccountsListEmpty);
          return;
        }
      }
      if (!FilterAccountsWithLoginHint(idp_info->provider->login_hint,
                                       accounts)) {
        // No accounts remain, so treat as account fetch failure.
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            "Accounts were received, but none matched the loginHint.");
        // If there are no accounts after filtering based on the login hint,
        // treat this exactly the same as if we had received an empty accounts
        // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
        HandleAccountsFetchFailure(
            std::move(idp_info), old_idp_signin_status,
            FederatedAuthRequestResult::kAccountsListEmpty,
            TokenStatus::kAccountsListEmpty);
        return;
      }
      if (!FilterAccountsWithDomainHint(idp_info->provider->domain_hint,
                                        accounts)) {
        // No accounts remain, so treat as account fetch failure.
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            "Accounts were received, but none matched the domainHint.");
        // If there are no accounts after filtering based on the domain hint,
        // treat this exactly the same as if we had received an empty accounts
        // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
        HandleAccountsFetchFailure(
            std::move(idp_info), old_idp_signin_status,
            FederatedAuthRequestResult::kAccountsListEmpty,
            TokenStatus::kAccountsListEmpty);
        return;
      }
      // TODO(crbug.com/354977893): pass filtered out accounts to UI.
      auto filter = [](const IdentityRequestAccountPtr& account) {
        return account->is_filtered_out;
      };
      std::erase_if(accounts, filter);
      RecordReadyToShowAccountsSize(accounts.size());
      ComputeLoginStates(idp_info->provider->config->config_url, accounts);

      bool need_client_metadata = false;

      if (!GetDisclosureFields(*idp_info->provider).empty()) {
        for (const auto& account : accounts) {
          // ComputeLoginStates() should have populated
          // IdentityRequestAccount::login_state.
          DCHECK(account->login_state);
          if (*account->login_state == LoginState::kSignUp) {
            need_client_metadata = true;
            break;
          }
        }
      }

      if (need_client_metadata &&
          webid::IsEndpointSameOrigin(idp_info->provider->config->config_url,
                                      idp_info->endpoints.client_metadata)) {
        // Copy OnClientMetadataResponseReceived() parameters because `idp_info`
        // is moved.
        GURL client_metadata_endpoint = idp_info->endpoints.client_metadata;
        std::string client_id = idp_info->provider->config->client_id;
        int icon_ideal_size =
            request_dialog_controller_->GetBrandIconIdealSize(rp_mode_);
        int icon_minimum_size =
            request_dialog_controller_->GetBrandIconMinimumSize(rp_mode_);
        network_manager_->FetchClientMetadata(
            client_metadata_endpoint, client_id, icon_ideal_size,
            icon_minimum_size,
            base::BindOnce(
                &FederatedAuthRequestImpl::OnClientMetadataResponseReceived,
                weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                std::move(accounts)));
      } else {
        FetchAccountPictures(std::move(idp_info), std::move(accounts),
                             IdpNetworkRequestManager::ClientMetadata());
      }
    }
  }
}

void FederatedAuthRequestImpl::FetchAccountPictures(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const IdpNetworkRequestManager::ClientMetadata& client_metadata) {
  auto callback = BarrierClosure(
      accounts.size(),
      base::BindOnce(&FederatedAuthRequestImpl::OnAllAccountPicturesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                     accounts, client_metadata));
  for (const auto& account : accounts) {
    if (account->picture.is_valid()) {
      network_manager_->DownloadAndDecodeImage(
          account->picture,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountPictureReceived,
                         weak_ptr_factory_.GetWeakPtr(), callback,
                         account->picture));
    } else {
      // We have to still call the callback to make sure the barrier
      // callback gets the right number of calls.
      callback.Run();
    }
  }
}

void FederatedAuthRequestImpl::OnAccountPictureReceived(
    base::RepeatingClosure cb,
    GURL url,
    const gfx::Image& image) {
  downloaded_images_[url] = image;
  cb.Run();
}

void FederatedAuthRequestImpl::OnAllAccountPicturesReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::vector<IdentityRequestAccountPtr>&& accounts,
    const IdpNetworkRequestManager::ClientMetadata& client_metadata) {
  for (auto& account : accounts) {
    auto it = downloaded_images_.find(account->picture);
    if (it != downloaded_images_.end()) {
      // We do not use std::move here in case multiple accounts use the same
      // picture URL, and the underlying gfx::Image data is refcounted anyway.
      account->decoded_picture = it->second;
    }
  }

  downloaded_images_.clear();
  OnFetchDataForIdpSucceeded(std::move(idp_info), std::move(accounts),
                             client_metadata);
}

void FederatedAuthRequestImpl::ComputeLoginStates(
    const GURL& idp_config_url,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  url::Origin idp_origin = url::Origin::Create(idp_config_url);
  // Populate the accounts login state.
  for (auto& account : accounts) {
    // Record when IDP and browser have different user sign-in states.
    bool idp_claimed_sign_in = account->login_state == LoginState::kSignIn;
    account->last_used_timestamp = permission_delegate_->GetLastUsedTimestamp(
        origin(), GetEmbeddingOrigin(), idp_origin, account->id);

    if (idp_claimed_sign_in == account->last_used_timestamp.has_value()) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          idp_config_url, SignInStateMatchStatus::kMatch);
    } else if (idp_claimed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          idp_config_url, SignInStateMatchStatus::kIdpClaimedSignIn);
    } else {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          idp_config_url, SignInStateMatchStatus::kBrowserObservedSignIn);
    }

    // We set the login state based on the IDP response if it sends
    // back an approved_clients list. If it does not, we need to set
    // it here based on browser state.
    if (!account->login_state) {
      // Consider this a sign-in if we have seen a successful sign-up for
      // this account before.
      account->login_state = account->last_used_timestamp.has_value()
                                 ? LoginState::kSignIn
                                 : LoginState::kSignUp;
    }

    if (webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
            render_frame_host(), /*provider_url=*/idp_config_url,
            GetEmbeddingOrigin(), origin(), account->id, permission_delegate_,
            api_permission_delegate_)) {
      // At this moment we can trust login_state even though it's controlled
      // by IdP. If it's kSignUp, it could mean that the browser's sharing
      // permission is obsolete.
      account->browser_trusted_login_state = account->login_state.value();
    }
  }
}

void FederatedAuthRequestImpl::OnAccountSelected(const GURL& idp_config_url,
                                                 const std::string& account_id,
                                                 bool is_sign_in) {
  DCHECK(!account_id.empty());
  const IdentityProviderInfo& idp_info = *idp_infos_[idp_config_url];

  // Check if the user has disabled the FedCM API after the FedCM UI is
  // displayed. This ensures that requests are not wrongfully sent to IDPs when
  // settings are changed while an existing FedCM UI is displayed. Ideally, we
  // should enforce this check before all requests but users typically won't
  // have time to disable the FedCM API in other types of requests.
  // Note that for the active flow is not affected by the permission status.
  if (GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED &&
      rp_mode_ != RpMode::kActive) {
    CompleteRequestWithError(FederatedAuthRequestResult::kDisabledInSettings,
                             TokenStatus::kDisabledInSettings,
                             /*token_error=*/std::nullopt,
                             /*should_delay_callback=*/true);
    return;
  }

  if (identity_selection_type_ != kExplicit) {
    // Embargo auto re-authn to mitigate a deadloop where an auto
    // re-authenticated user gets auto re-authenticated again soon after logging
    // out of the active session.
    auto_reauthn_permission_delegate_->RecordEmbargoForAutoReauthn(
        GetEmbeddingOrigin());
  } else {
    // Once a user has explicitly selected an account, there is no need to block
    // auto re-authn with embargo.
    auto_reauthn_permission_delegate_->RemoveEmbargoForAutoReauthn(
        GetEmbeddingOrigin());
  }

  fedcm_metrics_->RecordIsSignInUser(is_sign_in);

  api_permission_delegate_->RemoveEmbargoAndResetCounts(GetEmbeddingOrigin());

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordContinueOnPopupTime(
      idp_config_url, select_account_time_ - accounts_dialog_display_time_);

  url::Origin idp_origin = url::Origin::Create(idp_config_url);

  IdpNetworkRequestManager::ContinueOnCallback continue_on;
  if (webid::IsFedCmAuthzEnabled(render_frame_host(), idp_origin)) {
    continue_on = base::BindOnce(
        &FederatedAuthRequestImpl::OnContinueOnResponseReceived,
        weak_ptr_factory_.GetWeakPtr(), idp_info.provider->Clone());
  }

  std::vector<std::string> disclosure_shown_for;
  if (!is_sign_in) {
    disclosure_shown_for =
        DisclosureFieldsToStringList(idp_info.data->disclosure_fields);
  }

  CHECK(idp_info.data);
  network_manager_->SendTokenRequest(
      idp_info.endpoints.token, account_id_,
      ComputeUrlEncodedTokenPostData(
          render_frame_host(), idp_origin, idp_info.provider->config->client_id,
          idp_info.provider->nonce, account_id,
          identity_selection_type_ != kExplicit, rp_mode_,
          idp_info.provider->fields, disclosure_shown_for,
          idp_info.provider->params),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()),
      std::move(continue_on),
      base::BindOnce(&FederatedAuthRequestImpl::RecordErrorMetrics,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()));
}

void FederatedAuthRequestImpl::OnDismissFailureDialog(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  // Clicking the close active and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android. Dismissal through closing the
  // pop-up window is not embargoed since the user has taken some action to
  // continue to open the pop-up window.
  bool should_embargo = false;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
    case IdentityRequestDialogController::DismissReason::kSwipe:
      should_embargo = true;
      break;
    default:
      break;
  }

  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  should_embargo &= rp_mode_ == RpMode::kPassive;
  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  CompleteRequestWithError(should_embargo
                               ? FederatedAuthRequestResult::kShouldEmbargo
                               : FederatedAuthRequestResult::kError,
                           should_embargo ? TokenStatus::kShouldEmbargo
                                          : TokenStatus::kNotSignedInWithIdp,
                           /*token_error=*/std::nullopt,
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::OnDismissErrorDialog(
    const GURL& idp_config_url,
    IdpNetworkRequestManager::FetchStatus status,
    std::optional<TokenError> token_error,
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  bool has_url = token_error && !token_error->url.is_empty();
  ErrorDialogResult result;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
      result = has_url ? ErrorDialogResult::kCloseWithMoreDetails
                       : ErrorDialogResult::kCloseWithoutMoreDetails;
      break;
    case IdentityRequestDialogController::DismissReason::kSwipe:
      result = has_url ? ErrorDialogResult::kSwipeWithMoreDetails
                       : ErrorDialogResult::kSwipeWithoutMoreDetails;
      break;
    case IdentityRequestDialogController::DismissReason::kGotItButton:
      result = has_url ? ErrorDialogResult::kGotItWithMoreDetails
                       : ErrorDialogResult::kGotItWithoutMoreDetails;
      break;
    case IdentityRequestDialogController::DismissReason::kMoreDetailsButton:
      result = ErrorDialogResult::kMoreDetails;
      break;
    default:
      result = has_url ? ErrorDialogResult::kOtherWithMoreDetails
                       : ErrorDialogResult::kOtherWithoutMoreDetails;
      break;
  }
  fedcm_metrics_->RecordErrorDialogResult(result, idp_config_url);

  CompleteTokenRequest(idp_config_url, status, /*token=*/std::nullopt,
                       token_error, /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::OnDialogDismissed(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  if (dialog_type_ == kContinueOnPopup) {
    fedcm_metrics_->RecordContinueOnPopupResult(
        FedCmContinueOnPopupResult::kWindowClosed);
    // Popups always get dismissed with reason kOther, so we never embargo.
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kContinuationPopupClosedByUser,
                             /*token_error=*/std::nullopt,
                             /*should_delay_callback=*/false);
    return;
  }

  // Clicking the close active and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android.
  bool should_embargo = false;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
    case IdentityRequestDialogController::DismissReason::kSwipe:
      should_embargo = true;
      break;
    default:
      break;
  }

  if (should_embargo) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    fedcm_metrics_->RecordCancelOnDialogTime(
        idp_data_for_display_,
        dismiss_dialog_time - accounts_dialog_display_time_);
  }
  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  should_embargo &= rp_mode_ == RpMode::kPassive;
  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  // Reject the promise immediately if the UI is dismissed without selecting
  // an account. Meanwhile, we fuzz the rejection time for other failures to
  // make it indistinguishable.
  CompleteRequestWithError(should_embargo
                               ? FederatedAuthRequestResult::kShouldEmbargo
                               : FederatedAuthRequestResult::kError,
                           should_embargo ? TokenStatus::kShouldEmbargo
                                          : TokenStatus::kNotSelectAccount,
                           /*token_error=*/std::nullopt,
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::ShowModalDialog(DialogType dialog_type,
                                               const GURL& idp_config_url,
                                               const GURL& url_to_show) {
  // Reset dialog type, since we are typically not showing a FedCM dialog while
  // the popup window is open. When using the active flow the dialog may
  // still be up in some cases, but we do not expect that browser automation
  // needs to interact with the account chooser in this case.
  if (dialog_type_ != kNone) {
    // This call ensures that we send a dialogClosed event if an account
    // chooser or mismatch dialog is open.
    devtools_instrumentation::DidCloseFedCmDialog(render_frame_host());
  }
  // TODO(crbug.com/336815315): Should we notify browser automation of this
  // dialog?
  dialog_type_ = dialog_type;
  config_url_ = idp_config_url;
  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Popup.DialogType", dialog_type_);

  WebContents* web_contents = request_dialog_controller_->ShowModalDialog(
      url_to_show, rp_mode_,
      base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
  // This may be null on Android, as the method cannot return the WebContents of
  // the CCT that will be created.
  if (web_contents) {
    IdentityRegistry::CreateForWebContents(
        web_contents, weak_ptr_factory_.GetWeakPtr(), idp_config_url);
  }

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the mismatch dialog to be shown to prevent
  // abuse through flashing UI. When users trigger the IDP sign-in flow, the
  // mismatch dialog is hidden so we record this metric upon user triggering the
  // flow.
  if (mismatch_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordMismatchDialogShownDuration(
        idp_data_for_display_,
        base::TimeTicks::Now() - mismatch_dialog_shown_time_.value());
    mismatch_dialog_shown_time_ = std::nullopt;
  }
}

void FederatedAuthRequestImpl::OnContinueOnResponseReceived(
    IdentityProviderRequestOptionsPtr idp,
    IdpNetworkRequestManager::FetchStatus status,
    const GURL& continue_on) {
  url::Origin idp_origin = url::Origin::Create(idp->config->config_url);
  // This is enforced by OnAccountSelected when we call SendTokenRequest.
  DCHECK(webid::IsFedCmAuthzEnabled(render_frame_host(), idp_origin));

  id_assertion_response_time_ = base::TimeTicks::Now();

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      &render_frame_host(), blink::mojom::WebFeature::kFedCmContinueOnResponse);

  // We only allow loading continue_on urls that are same-origin
  // with the IdP.
  // This isn't necessarily final, but seemed like a safer
  // and sufficient default for now.
  // This behavior may change in https://crbug.com/1429083
  bool is_same_origin =
      url::Origin::Create(continue_on).IsSameOriginWith(idp_origin);

  bool can_show_popup = CanShowContinueOnPopup();
  if (!is_same_origin || !can_show_popup) {
    if (!is_same_origin && !can_show_popup) {
      fedcm_metrics_->RecordContinueOnPopupStatus(
          FedCmContinueOnPopupStatus::kUrlNotSameOriginAndPopupNotAllowed);
    } else if (!is_same_origin) {
      fedcm_metrics_->RecordContinueOnPopupStatus(
          FedCmContinueOnPopupStatus::kUrlNotSameOrigin);
    } else if (!can_show_popup) {
      fedcm_metrics_->RecordContinueOnPopupStatus(
          FedCmContinueOnPopupStatus::kPopupNotAllowed);
    }

    CompleteRequestWithError(
        FederatedAuthRequestResult::kIdTokenInvalidResponse,
        TokenStatus::kIdTokenInvalidResponse,
        /*token_error=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  fedcm_metrics_->RecordContinueOnPopupStatus(
      FedCmContinueOnPopupStatus::kPopupOpened);
  ShowModalDialog(kContinueOnPopup, idp->config->config_url, continue_on);
}

void FederatedAuthRequestImpl::ShowErrorDialog(
    const GURL& idp_config_url,
    IdpNetworkRequestManager::FetchStatus status,
    std::optional<TokenError> token_error) {
  CHECK(idp_infos_.find(idp_config_url) != idp_infos_.end());

  dialog_type_ = kError;
  config_url_ = idp_config_url;
  token_request_status_ = status;
  token_error_ = token_error;

  // TODO(crbug.com/40282657): Refactor IdentityCredentialTokenError
  if (!request_dialog_controller_->ShowErrorDialog(
          GetTopFrameOriginForDisplay(GetEmbeddingOrigin()),
          FormatOriginForDisplay(url::Origin::Create(idp_config_url)),
          idp_infos_[idp_config_url]->rp_context, rp_mode_,
          idp_infos_[idp_config_url]->metadata, token_error,
          base::BindOnce(&FederatedAuthRequestImpl::OnDismissErrorDialog,
                         weak_ptr_factory_.GetWeakPtr(), idp_config_url, status,
                         token_error),
          token_error && !token_error->url.is_empty()
              ? base::BindOnce(&FederatedAuthRequestImpl::ShowModalDialog,
                               weak_ptr_factory_.GetWeakPtr(), kErrorUrlPopup,
                               config_url_, token_error->url)
              : base::NullCallback())) {
    return;
  }
  devtools_instrumentation::DidShowFedCmDialog(render_frame_host());
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    IdentityProviderRequestOptionsPtr idp,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::TokenResult result) {
  CHECK(result.token.empty() || !result.error);

  bool should_show_error_ui =
      result.error ||
      status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess;
  auto complete_request_callback =
      should_show_error_ui
          ? base::BindOnce(&FederatedAuthRequestImpl::ShowErrorDialog,
                           weak_ptr_factory_.GetWeakPtr(),
                           idp->config->config_url, status, result.error)
          : base::BindOnce(&FederatedAuthRequestImpl::CompleteTokenRequest,
                           weak_ptr_factory_.GetWeakPtr(),
                           idp->config->config_url, status, result.token,
                           /*token_error=*/std::nullopt,
                           /*should_delay_callback=*/false);

  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // `kTokenRequestDelay` seconds for better UX.
  // Note that for active flow we can complete without delay because the UI
  // difference between the verifying UI and its predecessors are minor in case
  // of manual sign-in where a user has read and made selection explicitly.
  id_assertion_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time =
      id_assertion_response_time_ - select_account_time_;
  if (should_complete_request_immediately_ ||
      (rp_mode_ == RpMode::kActive && identity_selection_type_ == kExplicit) ||
      fetch_time >= kTokenRequestDelay) {
    std::move(complete_request_callback).Run();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(complete_request_callback),
      kTokenRequestDelay - fetch_time);
}

void FederatedAuthRequestImpl::MarkUserAsSignedIn(
    const GURL& idp_config_url,
    const std::string& account_id) {
  // Auto re-authentication can only be triggered when there's already a
  // sharing permission OR the IdP is exempted with 3PC access. Either way
  // we shouldn't explicitly grant permission here.
  CHECK(!account_id_.empty());
  if (identity_selection_type_ == kExplicit) {
    permission_delegate_->GrantSharingPermission(
        origin(), GetEmbeddingOrigin(), url::Origin::Create(idp_config_url),
        account_id);
  } else {
    permission_delegate_->RefreshExistingSharingPermission(
        origin(), GetEmbeddingOrigin(), url::Origin::Create(idp_config_url),
        account_id);
  }

  SetRequiresUserMediation(false, base::DoNothing());
}

void FederatedAuthRequestImpl::CompleteTokenRequest(
    const GURL& idp_config_url,
    IdpNetworkRequestManager::FetchStatus status,
    std::optional<std::string> token,
    std::optional<TokenError> token_error,
    bool should_delay_callback) {
  DCHECK(!start_time_.is_null());
  constexpr char kIdAssertionUrl[] = "id assertion endpoint";
  switch (status.parse_status) {
    case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(FederatedAuthRequestResult::kIdTokenHttpNotFound,
                               TokenStatus::kIdTokenHttpNotFound, token_error,
                               should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(FederatedAuthRequestResult::kIdTokenNoResponse,
                               TokenStatus::kIdTokenNoResponse, token_error,
                               should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kIdTokenInvalidResponse,
          TokenStatus::kIdTokenInvalidResponse, token_error,
          should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kIdTokenInvalidContentType,
          TokenStatus::kIdTokenInvalidContentType, token_error,
          should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
      NOTREACHED_IN_MIGRATION()
          << "kEmptyListError is undefined for CompleteTokenRequest";
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kSuccess: {
      if (token_error) {
        MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
        if (error_url_type_ && *error_url_type_ == ErrorUrlType::kCrossSite) {
          CompleteRequestWithError(
              FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse,
              TokenStatus::kIdTokenCrossSiteIdpErrorResponse, token_error,
              should_delay_callback);
          return;
        }
        CompleteRequestWithError(
            FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
            TokenStatus::kIdTokenIdpErrorResponse, token_error,
            should_delay_callback);
        return;
      }

      MarkUserAsSignedIn(idp_config_url, account_id_);

      fedcm_metrics_->RecordTokenResponseAndTurnaroundTime(
          idp_config_url, id_assertion_response_time_ - select_account_time_,
          id_assertion_response_time_ - start_time_ -
              (accounts_dialog_display_time_ -
               ready_to_display_accounts_dialog_time_));

      CompleteRequest(FederatedAuthRequestResult::kSuccess,
                      TokenStatus::kSuccessUsingTokenInHttpResponse,
                      /*token_error=*/std::nullopt, idp_config_url,
                      token.value(),
                      /*should_delay_callback=*/false);
      return;
    }
  }
}

void FederatedAuthRequestImpl::CompleteRequestWithError(
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<TokenStatus> token_status,
    std::optional<TokenError> token_error,
    bool should_delay_callback) {
  CompleteRequest(result, token_status, token_error,
                  /*selected_idp_config_url=*/std::nullopt,
                  /*token=*/"", should_delay_callback);
}

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<TokenStatus> token_status,
    std::optional<TokenError> token_error,
    const std::optional<GURL>& selected_idp_config_url,
    const std::string& id_token,
    bool should_delay_callback) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess || id_token.empty());

  if (accounts_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordAccountsDialogShownDuration(
        idp_data_for_display_,
        base::TimeTicks::Now() - accounts_dialog_shown_time_.value());
    accounts_dialog_shown_time_ = std::nullopt;
  }

  if (mismatch_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordMismatchDialogShownDuration(
        idp_data_for_display_,
        base::TimeTicks::Now() - mismatch_dialog_shown_time_.value());
    mismatch_dialog_shown_time_ = std::nullopt;
  }

  if (!auth_request_token_callback_) {
    return;
  }

  if (token_status) {
    int num_idps_mismatch = std::count_if(
        idp_data_for_display_.begin(), idp_data_for_display_.end(),
        [](auto& provider) { return provider->has_login_status_mismatch; });
    fedcm_metrics_->RecordRequestTokenStatus(
        *token_status, mediation_requirement_, idp_order_, num_idps_mismatch,
        selected_idp_config_url, rp_mode_);
  }

  if (result == FederatedAuthRequestResult::kSuccess) {
    DCHECK(selected_idp_config_url);
    if (IsFedCmMetricsEndpointEnabled()) {
      SendSuccessfulTokenRequestMetrics(*selected_idp_config_url);
    }
  } else if (!errors_logged_to_console_) {
    errors_logged_to_console_ = true;

    AddDevToolsIssue(result);
    AddConsoleErrorMessage(result);

    if (IsFedCmMetricsEndpointEnabled()) {
      for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
        SendFailedTokenRequestMetrics(metrics_endpoint_kv.second, result);
      }
    }
  }

  bool is_auto_selected = identity_selection_type_ != kExplicit;

  if (ShouldNotifyDevtoolsForDialogType(dialog_type_)) {
    devtools_instrumentation::DidCloseFedCmDialog(render_frame_host());
  }

  if (!should_delay_callback || should_complete_request_immediately_) {
    CleanUp();
    webid::GetPageData(render_frame_host().GetPage())
        ->SetPendingWebIdentityRequest(nullptr);
    errors_logged_to_console_ = false;

    blink::mojom::TokenErrorPtr error;
    if (token_error) {
      error = blink::mojom::TokenError::New();
      error->code = token_error->code;
      error->url = token_error->url.spec();
    }
    RequestTokenStatus status =
        FederatedAuthRequestResultToRequestTokenStatus(result);
    std::move(auth_request_token_callback_)
        .Run(status, selected_idp_config_url, id_token, std::move(error),
             is_auto_selected);
    auth_request_token_callback_.Reset();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FederatedAuthRequestImpl::OnRejectRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        GetRandomRejectionTime());
  }
}

void FederatedAuthRequestImpl::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint,
    blink::mojom::FederatedAuthRequestResult result) {
  DCHECK(IsFedCmMetricsEndpointEnabled());
  if (!metrics_endpoint.is_valid()) {
    return;
  }

  network_manager_->SendFailedTokenRequestMetrics(
      metrics_endpoint,
      FederatedAuthRequestResultToMetricsEndpointErrorCode(result));
}

void FederatedAuthRequestImpl::SendSuccessfulTokenRequestMetrics(
    const GURL& idp_config_url) {
  DCHECK(IsFedCmMetricsEndpointEnabled());

  for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
    const GURL& metrics_endpoint = metrics_endpoint_kv.second;
    if (!metrics_endpoint.is_valid()) {
      continue;
    }

    if (metrics_endpoint_kv.first == idp_config_url) {
      network_manager_->SendSuccessfulTokenRequestMetrics(
          metrics_endpoint,
          ready_to_display_accounts_dialog_time_ - start_time_,
          select_account_time_ - accounts_dialog_display_time_,
          id_assertion_response_time_ - select_account_time_,
          id_assertion_response_time_ - start_time_ -
              (accounts_dialog_display_time_ -
               ready_to_display_accounts_dialog_time_));
    } else {
      // Send kUserFailure so that IDP cannot tell difference between user
      // selecting a different IDP and user dismissing dialog without
      // selecting any IDP.
      network_manager_->SendFailedTokenRequestMetrics(
          metrics_endpoint,
          IdpNetworkRequestManager::MetricsEndpointErrorCode::kUserFailure);
    }
  }
}

void FederatedAuthRequestImpl::CleanUp() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  permission_delegate_->RemoveIdpSigninStatusObserver(this);

  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  provider_fetcher_.reset();
  account_id_ = std::string();
  start_time_ = base::TimeTicks();
  well_known_and_config_fetched_time_ = base::TimeTicks();
  accounts_fetched_time_ = base::TimeTicks();
  client_metadata_fetched_time_ = base::TimeTicks();
  ready_to_display_accounts_dialog_time_ = base::TimeTicks();
  accounts_dialog_display_time_ = base::TimeTicks();
  select_account_time_ = base::TimeTicks();
  id_assertion_response_time_ = base::TimeTicks();
  accounts_dialog_shown_time_ = std::nullopt;
  mismatch_dialog_shown_time_ = std::nullopt;
  has_shown_mismatch_ = false;
  idp_accounts_.clear();
  new_accounts_.clear();
  accounts_.clear();
  idp_login_infos_.clear();
  idp_infos_.clear();
  idp_data_for_display_.clear();
  account_ids_before_login_.clear();
  fetch_data_ = FetchData();
  idp_order_.clear();
  metrics_endpoints_.clear();
  token_request_get_infos_.clear();
  login_url_ = GURL();
  config_url_ = GURL();
  token_error_ = std::nullopt;
  dialog_type_ = kNone;
  identity_selection_type_ = kExplicit;
  had_transient_user_activation_ = false;
  rp_mode_ = RpMode::kPassive;
}

void FederatedAuthRequestImpl::AddDevToolsIssue(
    FederatedAuthRequestResult result) {
  DCHECK_NE(result, FederatedAuthRequestResult::kSuccess);

  // It would be possible to add this inspector issue on the renderer, which
  // will receive the callback. However, it is preferable to do so on the
  // browser because this is closer to the source, which means adding
  // additional metadata is easier. In addition, in the future we may only
  // need to pass a small amount of information to the renderer in the case of
  // an error, so it would be cleaner to do this by reporting the inspector
  // issue from the browser.
  auto details = blink::mojom::InspectorIssueDetails::New();
  auto federated_auth_request_details =
      blink::mojom::FederatedAuthRequestIssueDetails::New(result);
  details->federated_auth_request_details =
      std::move(federated_auth_request_details);
  render_frame_host().ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue,
          std::move(details)));
}

void FederatedAuthRequestImpl::AddConsoleErrorMessage(
    FederatedAuthRequestResult result) {
  render_frame_host().AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      webid::GetConsoleErrorMessageFromResult(result));
}

void FederatedAuthRequestImpl::MaybeAddResponseCodeToConsole(
    const char* fetch_description,
    int response_code) {
  std::optional<std::string> console_message =
      webid::ComputeConsoleMessageForHttpResponseCode(fetch_description,
                                                      response_code);
  if (console_message) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, *console_message);
  }
}

url::Origin FederatedAuthRequestImpl::GetEmbeddingOrigin() const {
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

void FederatedAuthRequestImpl::CompleteUserInfoRequest(
    FederatedAuthUserInfoRequest* request,
    RequestUserInfoCallback callback,
    blink::mojom::RequestUserInfoStatus status,
    std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
  auto it = std::find_if(
      user_info_requests_.begin(), user_info_requests_.end(),
      [request](const std::unique_ptr<FederatedAuthUserInfoRequest>& ptr) {
        return ptr.get() == request;
      });
  // The request may not be found if the completion is invoked from
  // FederatedAuthRequestImpl destructor. The destructor clears
  // `user_info_requests_`, which destroys the FederatedAuthUserInfoRequests it
  // contains. The FederatedAuthUserInfoRequest destructor invokes this
  // callback.
  if (it == user_info_requests_.end() &&
      status == blink::mojom::RequestUserInfoStatus::kSuccess) {
    NOTREACHED_IN_MIGRATION()
        << "The successful user info request is nowhere to be found";
    return;
  }
  std::move(callback).Run(status, std::move(user_info));
  if (it != user_info_requests_.end()) {
    user_info_requests_.erase(it);
  }
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager() {
  if (mock_network_manager_) {
    return std::move(mock_network_manager_);
  }

  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

std::unique_ptr<IdentityRequestDialogController>
FederatedAuthRequestImpl::CreateDialogController() {
  if (mock_dialog_controller_) {
    return std::move(mock_dialog_controller_);
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForFedCM)) {
    std::string selected_account =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUseFakeUIForFedCM);
    return std::make_unique<FakeIdentityRequestDialogController>(
        selected_account.empty() ? std::nullopt
                                 : std::optional<std::string>(selected_account),
        web_contents);
  }

  return GetContentClient()->browser()->CreateIdentityRequestDialogController(
      web_contents);
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void FederatedAuthRequestImpl::OnClose() {
  CHECK(request_dialog_controller_);
  request_dialog_controller_->CloseModalDialog();

  // When IdentityProvider.close is called in the continuation popup, we
  // should abort the flow.
  if (dialog_type_ == kContinueOnPopup) {
    fedcm_metrics_->RecordContinueOnPopupResult(
        FedCmContinueOnPopupResult::kClosedByIdentityProviderClose);
    // Popups always get dismissed with reason kOther, so we never embargo.
    CompleteRequestWithError(
        FederatedAuthRequestResult::kError,
        TokenStatus::kContinuationPopupClosedByIdentityProviderClose,
        /*token_error=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }
}

bool FederatedAuthRequestImpl::OnResolve(
    GURL idp_config_url,
    const std::optional<std::string>& account_id,
    const std::string& token) {
  // Close the pop-up window post user permission.
  if (!request_dialog_controller_) {
    return false;
  }
  request_dialog_controller_->CloseModalDialog();

  MarkUserAsSignedIn(idp_config_url, account_id.value_or(account_id_));

  fedcm_metrics_->RecordContinueOnResponseAndTurnaroundTime(
      id_assertion_response_time_ - select_account_time_,
      base::TimeTicks::Now() - start_time_ -
          (accounts_dialog_display_time_ -
           ready_to_display_accounts_dialog_time_));
  fedcm_metrics_->RecordContinueOnPopupResult(
      FedCmContinueOnPopupResult::kTokenReceived);

  CompleteRequest(FederatedAuthRequestResult::kSuccess,
                  TokenStatus::kSuccessUsingIdentityProviderResolve,
                  /*token_error=*/std::nullopt, idp_config_url, token,
                  /*should_delay_callback=*/false);
  // TODO(crbug.com/40262526): handle the corner cases where CompleteRequest
  // can't actually fulfill the request.
  return true;
}

bool FederatedAuthRequestImpl::SetupIdentityRegistryFromPopup() {
#if BUILDFLAG(IS_ANDROID)
  if (identity_registry_) {
    return true;
  }

  if (!request_dialog_controller_) {
    request_dialog_controller_ = CreateDialogController();
    CHECK(request_dialog_controller_);
  }

  // Because ShowModalDialog does not return the web contents on Android, we
  // need to set up the IdentityRegistry now.
  WebContents* rp_web_contents = request_dialog_controller_->GetRpWebContents();
  // This can be null if resolve was called in a regular tab (as opposed to
  // a CCT opened from ShowModalDialog).
  if (!rp_web_contents) {
    return false;
  }
  FederatedAuthRequestImpl* rp_auth_request =
      webid::GetPageData(rp_web_contents->GetPrimaryPage())
          ->PendingWebIdentityRequest();
  if (!rp_auth_request) {
    return false;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  IdentityRegistry::CreateForWebContents(
      web_contents, rp_auth_request->weak_ptr_factory_.GetWeakPtr(),
      rp_auth_request->config_url_);
  identity_registry_ = IdentityRegistry::FromWebContents(web_contents);

  return true;
#else
  return false;
#endif
}

void FederatedAuthRequestImpl::OnRejectRequest() {
  if (!auth_request_token_callback_) {
    return;
  }
  DCHECK(errors_logged_to_console_);
  CompleteRequestWithError(FederatedAuthRequestResult::kError,
                           /*token_status=*/std::nullopt,
                           /*token_error=*/std::nullopt,
                           /*should_delay_callback=*/false);
}

FederatedApiPermissionStatus
FederatedAuthRequestImpl::GetApiPermissionStatus() {
  DCHECK(api_permission_delegate_);
  return api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin());
}

bool FederatedAuthRequestImpl::ShouldNotifyDevtoolsForDialogType(
    DialogType type) {
  return type != kNone && type != kLoginToIdpPopup &&
         type != kContinueOnPopup && type != kErrorUrlPopup;
}

void FederatedAuthRequestImpl::AcceptAccountsDialogForDevtools(
    const GURL& config_url,
    const IdentityRequestAccount& account) {
  bool is_sign_in =
      account.login_state == IdentityRequestAccount::LoginState::kSignIn;
  OnAccountSelected(config_url, account.id, is_sign_in);
}

void FederatedAuthRequestImpl::DismissAccountsDialogForDevtools(
    bool should_embargo) {
  // We somewhat arbitrarily pick a reason that does/does not trigger
  // cooldown.
  IdentityRequestDialogController::DismissReason reason =
      should_embargo
          ? IdentityRequestDialogController::DismissReason::kCloseButton
          : IdentityRequestDialogController::DismissReason::kOther;
  OnDialogDismissed(reason);
}

void FederatedAuthRequestImpl::AcceptConfirmIdpLoginDialogForDevtools() {
  DCHECK(login_url_.is_valid());
  LoginToIdP(/*can_append_hints=*/true, config_url_, login_url_);
}

void FederatedAuthRequestImpl::DismissConfirmIdpLoginDialogForDevtools() {
  // These values match what HandleAccountsFetchFailure passes.
  OnDismissFailureDialog(
      IdentityRequestDialogController::DismissReason::kOther);
}

bool FederatedAuthRequestImpl::UseAnotherAccountForDevtools(
    const IdentityProviderData& provider) {
  if (!provider.idp_metadata.supports_add_account) {
    return false;
  }
  LoginToIdP(/*can_append_hints=*/true, provider.idp_metadata.config_url,
             provider.idp_metadata.idp_login_url);
  return true;
}

bool FederatedAuthRequestImpl::HasMoreDetailsButtonForDevtools() {
  return token_error_ && token_error_->url.is_valid();
}

void FederatedAuthRequestImpl::ClickErrorDialogGotItForDevtools() {
  DCHECK(token_error_);
  OnDismissErrorDialog(
      config_url_, token_request_status_, token_error_,
      IdentityRequestDialogController::DismissReason::kGotItButton);
}

void FederatedAuthRequestImpl::ClickErrorDialogMoreDetailsForDevtools() {
  DCHECK(token_error_ && token_error_->url.is_valid());
  ShowModalDialog(kErrorUrlPopup, config_url_, token_error_->url);
  OnDismissErrorDialog(
      config_url_, token_request_status_, token_error_,
      IdentityRequestDialogController::DismissReason::kMoreDetailsButton);
}

void FederatedAuthRequestImpl::DismissErrorDialogForDevtools() {
  OnDismissErrorDialog(config_url_, token_request_status_, token_error_,
                       IdentityRequestDialogController::DismissReason::kOther);
}

bool FederatedAuthRequestImpl::GetAccountForAutoReauthn(
    IdentityProviderDataPtr* out_idp_data,
    IdentityRequestAccountPtr* out_account) {
  for (const auto& idp_info : idp_infos_) {
    if (idp_info.second->data->has_login_status_mismatch) {
      // If we need to show IDP login status mismatch UI, we cannot
      // auto-reauthenticate a user even if there really is a single returning
      // account.
      return false;
    }
  }
  for (const auto& account : accounts_) {
    if (account->login_state == LoginState::kSignUp) {
      continue;
    }
    // account.login_state could be set to kSignIn if the client is on the
    // `approved_clients` list provided by IDP. However, in this case we have
    // to trust the browser observed sign-in unless the IDP can be exempted.
    // For example, they have third party cookies access on the RP site.
    if (!webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
            render_frame_host(),
            /*provider_url=*/
            account->identity_provider->idp_metadata.config_url,
            GetEmbeddingOrigin(), origin(), account->id, permission_delegate_,
            api_permission_delegate_)) {
      continue;
    }

    if (*out_account) {
      return false;
    }
    *out_idp_data = account->identity_provider;
    *out_account = account;
  }

  if (*out_account) {
    return true;
  }

  return false;
}

bool FederatedAuthRequestImpl::ShouldFailBeforeFetchingAccounts(
    const GURL& config_url) {
  if (mediation_requirement_ != MediationRequirement::kSilent) {
    return false;
  }

  bool is_auto_reauthn_setting_enabled =
      auto_reauthn_permission_delegate_->IsAutoReauthnSettingEnabled();
  if (!is_auto_reauthn_setting_enabled) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: the user has disabled auto re-authn.");
  }

  bool is_auto_reauthn_embargoed =
      auto_reauthn_permission_delegate_->IsAutoReauthnEmbargoed(
          GetEmbeddingOrigin());
  std::optional<base::TimeDelta> time_from_embargo;
  if (is_auto_reauthn_embargoed) {
    time_from_embargo =
        base::Time::Now() -
        auto_reauthn_permission_delegate_->GetAutoReauthnEmbargoStartTime(
            GetEmbeddingOrigin());
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: auto re-authn is in quiet period because it "
        "was recently used on this site.");
  }

  bool has_sharing_permission_for_any_account =
      webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
          render_frame_host(), config_url, GetEmbeddingOrigin(), origin(),
          /*account_id=*/std::nullopt, permission_delegate_,
          api_permission_delegate_);

  if (!has_sharing_permission_for_any_account) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: the user has not used FedCM on this site with "
        "this identity provider.");
  }

  bool requires_user_mediation = RequiresUserMediation();
  if (requires_user_mediation) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation issue: preventSilentAccess() has been invoked on the "
        "site.");
  }

  if (requires_user_mediation || !is_auto_reauthn_setting_enabled ||
      is_auto_reauthn_embargoed || !has_sharing_permission_for_any_account) {
    // Record the relevant auto reauthn metrics before aborting the FedCM flow.
    fedcm_metrics_->RecordAutoReauthnMetrics(
        /*has_single_returning_account=*/std::nullopt,
        /*auto_signin_account=*/nullptr,
        /*auto_reauthn_success=*/false, !is_auto_reauthn_setting_enabled,
        is_auto_reauthn_embargoed, time_from_embargo, requires_user_mediation);
    return true;
  }
  return false;
}

bool FederatedAuthRequestImpl::RequiresUserMediation() {
  return auto_reauthn_permission_delegate_->RequiresUserMediation(origin());
}

void FederatedAuthRequestImpl::SetRequiresUserMediation(
    bool requires_user_mediation,
    base::OnceClosure callback) {
  auto_reauthn_permission_delegate_->SetRequiresUserMediation(
      origin(), requires_user_mediation);
  if (permission_delegate_) {
    permission_delegate_->OnSetRequiresUserMediation(origin(),
                                                     std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void FederatedAuthRequestImpl::LoginToIdP(bool can_append_hints,
                                          const GURL& idp_config_url,
                                          GURL login_url) {
  const auto& it = idp_login_infos_.find(login_url);
  CHECK(it != idp_login_infos_.end());
  if (can_append_hints) {
    // Before invoking UI, append the query parameters to the `idp_login_url` if
    // needed.
    MaybeAppendQueryParameters(it->second, &login_url);
  }
  permission_delegate_->AddIdpSigninStatusObserver(this);

  if (idp_infos_.size() > 1u ||
      IsFedCmUseOtherAccountEnabled(rp_mode_ == RpMode::kActive)) {
    account_ids_before_login_.clear();
    for (const auto& account : accounts_) {
      if (account->identity_provider->idp_metadata.idp_login_url == login_url) {
        account_ids_before_login_.insert(account->id);
      }
    }
  }

  login_url_ = login_url;
  ShowModalDialog(kLoginToIdpPopup, idp_config_url, login_url);
}

void FederatedAuthRequestImpl::MaybeShowActiveModeModalDialog(
    const GURL& idp_config_url,
    const GURL& idp_login_url) {
  if (IsFedCmMultipleIdentityProvidersEnabled() && idp_infos_.size() > 1) {
    // TODO(crbug.com/40283218): handle the active flow and the
    // Multi IdP API (what should happen if you are logged in to some
    // IdPs but not to others).
    // TODO(crbug.com/326987150): This is temporary so we should degrade
    // gracefully.
    return;
  }

  // We fail sooner before, but just to double check, we assert that
  // we are inside a user gesture here again.
  CHECK(had_transient_user_activation_);
  // TODO(crbug.com/40283219): we should probably make idp_login_url
  // optional instead of empty.
  LoginToIdP(/*can_append_hints=*/false, idp_config_url, idp_login_url);
  return;
}

void FederatedAuthRequestImpl::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  SetRequiresUserMediation(true, std::move(callback));
  if (permission_delegate_->HasSharingPermission(GetEmbeddingOrigin())) {
    RecordPreventSilentAccess(render_frame_host(), origin(),
                              GetEmbeddingOrigin());
  }
}

void FederatedAuthRequestImpl::Disconnect(
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
    DisconnectCallback callback) {
  MaybeCreateFedCmMetrics();
  // FedCMMetrics is used to record disconnect metrics, but does not use the
  // session_id_, which belongs to token request calls.
  if (disconnect_request_) {
    // Since we do not send any fetches in this case, consider the request to be
    // instant, e.g. duration is 0.
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        webid::GetDisconnectConsoleErrorMessage(
            FedCmDisconnectStatus::kTooManyRequests));
    fedcm_metrics_->RecordDisconnectMetrics(
        FedCmDisconnectStatus::kTooManyRequests, std::nullopt,
        render_frame_host(), origin(), GetEmbeddingOrigin(),
        options->config->config_url, webid::GetNewSessionID());
    std::move(callback).Run(DisconnectStatus::kErrorTooManyRequests);
    return;
  }

  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      render_frame_host(), &intercept, &should_complete_request_immediately);

  auto network_manager = CreateNetworkManager();

  disconnect_request_ = FederatedAuthDisconnectRequest::Create(
      std::move(network_manager), permission_delegate_, &render_frame_host(),
      fedcm_metrics_.get(), std::move(options));
  FederatedAuthDisconnectRequest* disconnect_request_ptr =
      disconnect_request_.get();

  disconnect_request_ptr->SetCallbackAndStart(
      base::BindOnce(&FederatedAuthRequestImpl::CompleteDisconnectRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      api_permission_delegate_);
}

void FederatedAuthRequestImpl::RecordErrorMetrics(
    IdentityProviderRequestOptionsPtr idp,
    TokenResponseType token_response_type,
    std::optional<ErrorDialogType> error_dialog_type,
    std::optional<ErrorUrlType> error_url_type) {
  fedcm_metrics_->RecordErrorMetricsBeforeShowingErrorDialog(
      token_response_type, error_dialog_type, error_url_type,
      idp->config->config_url);

  if (error_url_type) {
    // This is used to determine if we need to use the cross-site specific
    // devtools issue when failing the request.
    error_url_type_ = error_url_type;
  }
}

void FederatedAuthRequestImpl::MaybeCreateFedCmMetrics() {
  if (!fedcm_metrics_) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAithRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));

    fedcm_metrics_ = std::make_unique<FedCmMetrics>(
        render_frame_host().GetPageUkmSourceId());
  }
}

bool FederatedAuthRequestImpl::IsNewlyLoggedIn(
    const IdentityRequestAccount& account) {
  if (idp_infos_.size() <= 1u &&
      !IsFedCmUseOtherAccountEnabled(rp_mode_ == RpMode::kActive)) {
    return false;
  }
  if (login_url_.is_empty() ||
      login_url_ != account.identity_provider->idp_metadata.idp_login_url) {
    return false;
  }
  return !account_ids_before_login_.contains(account.id);
}

bool FederatedAuthRequestImpl::FilterAccountsWithLabel(
    const std::string& label,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (label.empty()) {
    return true;
  }

  // Filter out all accounts whose labels do not match the requested label.
  // Note that it is technically possible for us to end up with more than one
  // account afterwards, in which case the multiple account chooser would be
  // shown.
  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (!base::Contains(account->labels, label)) {
      account->is_filtered_out = true;
    } else {
      ++accounts_remaining;
    }
  }
  fedcm_metrics_->RecordNumMatchingAccounts(accounts_remaining, "AccountLabel");
  return accounts_remaining > 0u;
}

bool FederatedAuthRequestImpl::FilterAccountsWithLoginHint(
    const std::string& login_hint,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (login_hint.empty()) {
    return true;
  }

  // Filter out all accounts whose ID and whose email do not match the login
  // hint. Note that it is technically possible for us to end up with more than
  // one account afterwards, in which case the multiple account chooser would be
  // shown.
  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (account->is_filtered_out) {
      continue;
    }
    if (!base::Contains(account->login_hints, login_hint)) {
      account->is_filtered_out = true;
    } else {
      ++accounts_remaining;
    }
  }
  fedcm_metrics_->RecordNumMatchingAccounts(accounts_remaining, "LoginHint");
  return accounts_remaining > 0u;
}

bool FederatedAuthRequestImpl::FilterAccountsWithDomainHint(
    const std::string& domain_hint,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (domain_hint.empty()) {
    return true;
  }

  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (account->is_filtered_out) {
      continue;
    }
    if (domain_hint == FederatedAuthRequestImpl::kWildcardDomainHint) {
      if (account->domain_hints.empty()) {
        account->is_filtered_out = true;
        continue;
      }
    } else if (!base::Contains(account->domain_hints, domain_hint)) {
      account->is_filtered_out = true;
      continue;
    }
    ++accounts_remaining;
  }
  fedcm_metrics_->RecordNumMatchingAccounts(accounts_remaining, "DomainHint");
  return accounts_remaining > 0u;
}

}  // namespace content
