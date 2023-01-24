// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/url_formatter/elide_url.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/browser/webid/federated_auth_user_info_request.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::IdentityProviderConfig;
using blink::mojom::IdentityProviderConfigPtr;
using blink::mojom::IdentityProviderGetParametersPtr;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::RequestTokenStatus;
using blink::mojom::RequestUserInfoStatus;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;

namespace content {

namespace {
static constexpr base::TimeDelta kDefaultTokenRequestDelay = base::Seconds(3);
static constexpr base::TimeDelta kMaxRejectionTime = base::Seconds(60);

std::string ComputeUrlEncodedTokenPostData(const std::string& client_id,
                                           const std::string& nonce,
                                           const std::string& account_id,
                                           bool is_sign_in) {
  std::string query;
  if (!client_id.empty())
    query +=
        "client_id=" + base::EscapeUrlEncodedData(client_id, /*use_plus=*/true);

  if (!nonce.empty()) {
    if (!query.empty())
      query += "&";
    query += "nonce=" + base::EscapeUrlEncodedData(nonce, /*use_plus=*/true);
  }

  if (!account_id.empty()) {
    if (!query.empty())
      query += "&";
    query += "account_id=" +
             base::EscapeUrlEncodedData(account_id, /*use_plus=*/true);
  }
  // For new users signing up, we show some disclosure text to remind them about
  // data sharing between IDP and RP. For returning users signing in, such
  // disclosure text is not necessary. This field indicates in the request
  // whether the user has been shown such disclosure text.
  std::string disclosure_text_shown = is_sign_in ? "false" : "true";
  if (!query.empty())
    query += "&disclosure_text_shown=" + disclosure_text_shown;

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
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return RequestTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return RequestTokenStatus::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kErrorDisabledInSettings:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty:
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown:
    case FederatedAuthRequestResult::kErrorWellKnownTooBig:
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kErrorRpPageNotVisible:
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
    case FederatedAuthRequestResult::kErrorTooManyRequests:
    case FederatedAuthRequestResult::kErrorCanceled: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kRpFailure;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kAccountsEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kTokenEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kErrorDisabledInSettings:
    case FederatedAuthRequestResult::kErrorRpPageNotVisible: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kUserFailure;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerUnavailable;
    }
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown:
    case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kManifestError;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerInvalidResponse;
    }
    case FederatedAuthRequestResult::kError: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kOther;
    }
  }
}

// TODO(crbug.com/1344150): Use normal distribution after sufficient data is
// collected.
base::TimeDelta GetRandomRejectionTime() {
  return kMaxRejectionTime * base::RandDouble();
}

std::string FormatUrlForDisplay(const GURL& url) {
  // We do not use url_formatter::FormatUrlForSecurityDisplay() directly because
  // our UI intentionally shows only the eTLD+1, as it makes for a shorter text
  // that is also clearer to users. The identity provider's well-known file is
  // in the root of the eTLD+1, and sign-in status within identity provider and
  // relying party can be domain-wide because it relies on cookies.
  std::string formatted_url_str =
      net::IsLocalhost(url)
          ? url.host()
          : net::registry_controlled_domains::GetDomainAndRegistry(
                url,
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
      GURL(url.scheme() + "://" + formatted_url_str),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

std::string FormatOriginForDisplay(const url::Origin& origin) {
  return FormatUrlForDisplay(origin.GetURL());
}

FederatedAuthRequestPageData* GetPageData(RenderFrameHost* render_frame_host) {
  return FederatedAuthRequestPageData::GetOrCreateForPage(
      render_frame_host->GetPage());
}

void FilterAccountsWithLoginHint(
    const blink::mojom::IdentityProviderLoginHintPtr& login_hint,
    IdpNetworkRequestManager::AccountList& accounts) {
  // Do not filter if both email and id are empty.
  if (login_hint->email.empty() && login_hint->id.empty()) {
    return;
  }

  // Remove all accounts whose ID and whose email do not match the login hint.
  // Note that it is technically possible for us to end up with more than one
  // account afterwards, in which case the multiple account chooser would be
  // shown.
  auto Filter = [&login_hint](const IdentityRequestAccount& account) {
    if (!login_hint->email.empty() && !login_hint->id.empty()) {
      return account.id != login_hint->id && account.email != login_hint->email;
    } else if (!login_hint->email.empty()) {
      return account.email != login_hint->email;
    } else {
      return account.id != login_hint->id;
    }
  };
  bool should_filter = true;
  if (!login_hint->is_required) {
    // If |is_required| is false, do not use the filter if all elements would be
    // removed.
    size_t num_filtered_out =
        std::count_if(accounts.begin(), accounts.end(), Filter);
    should_filter = num_filtered_out != accounts.size();
  }
  if (should_filter) {
    accounts.erase(std::remove_if(accounts.begin(), accounts.end(), Filter),
                   accounts.end());
  }
}

std::unique_ptr<FedCmMetrics> CreateFedCmMetrics(
    const GURL& provider_config_url,
    const ukm::SourceId& source_id,
    bool is_disabled) {
  return std::make_unique<FedCmMetrics>(provider_config_url, source_id,
                                        base::RandInt(1, 1 << 30), is_disabled);
}

bool HasSingleReturningAccount(const std::vector<IdentityProviderData>& idps) {
  bool has_single_returning_account = false;
  for (const auto& idp : idps) {
    for (const auto& account : idp.accounts) {
      if (account.login_state != LoginState::kSignIn) {
        continue;
      }

      if (has_single_returning_account) {
        // More than one returning account
        return false;
      }
      has_single_returning_account = true;
    }
  }
  return has_single_returning_account;
}

}  // namespace

FederatedAuthRequestImpl::IdentityProviderGetInfo::IdentityProviderGetInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    bool prefer_auto_signin,
    blink::mojom::RpContext rp_context)
    : provider(std::move(provider)),
      prefer_auto_signin(prefer_auto_signin),
      rp_context(rp_context) {}

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
  prefer_auto_signin = other.prefer_auto_signin;
  rp_context = other.rp_context;
  return *this;
}

FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata metadata,
    bool prefer_auto_signin,
    blink::mojom::RpContext rp_context)
    : provider(std::move(provider)),
      endpoints(std::move(endpoints)),
      metadata(std::move(metadata)),
      prefer_auto_signin(prefer_auto_signin),
      rp_context(rp_context) {}

FederatedAuthRequestImpl::IdentityProviderInfo::~IdentityProviderInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    const IdentityProviderInfo& other) {
  provider = other.provider->Clone();
  endpoints = other.endpoints;
  metadata = other.metadata;
  prefer_auto_signin = other.prefer_auto_signin;
  has_failing_idp_signin_status = other.has_failing_idp_signin_status;
  rp_context = other.rp_context;
  data = other.data;
}

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : DocumentService(host, std::move(receiver)),
      api_permission_delegate_(api_permission_context),
      permission_delegate_(permission_context),
      token_request_delay_(kDefaultTokenRequestDelay) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_token_callback_) {
    DCHECK(!logout_callback_);
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kUnhandledRequest,
                             /*should_delay_callback=*/false);
  }
  if (user_info_request_) {
    // Calls |FederatedAuthUserInfoRequest|'s destructor to complete the user
    // info request. This is needed because otherwise some resources like
    // `fedcm_metrics_` may no longer be usable when the destructor get invoked
    // naturally.
    user_info_request_.reset();
  }
  if (logout_callback_) {
    // We do not complete the logout request, so unset the
    // PendingWebIdentityRequest on the Page so that other frames in the
    // same Page may still trigger new requests after the current
    // RenderFrameHost is destroyed.
    GetPageData(&render_frame_host())->SetHasPendingWebIdentityRequest(false);
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
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_context =
      browser_context->GetFederatedIdentityPermissionContext();
  if (!api_permission_context || !permission_context)
    return;

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new FederatedAuthRequestImpl(*host, api_permission_context,
                               permission_context, std::move(receiver));
}

FederatedAuthRequestImpl& FederatedAuthRequestImpl::CreateForTesting(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  return *new FederatedAuthRequestImpl(host, api_permission_context,
                                       permission_context, std::move(receiver));
}

void FederatedAuthRequestImpl::RequestToken(
    std::vector<IdentityProviderGetParametersPtr> idp_get_params_ptrs,
    RequestTokenCallback callback) {
  // idp_get_params_ptrs should never be empty since it is the renderer-side
  // code which populates it.
  if (idp_get_params_ptrs.empty()) {
    mojo::ReportBadMessage("idp_get_params_ptrs is empty.");
    return;
  }
  // It should not be possible to receive multiple IDPs when the
  // `kFedCmMultipleIdentityProviders` flag is disabled. But such a message
  // could be received from a compromised renderer.
  const bool is_multi_idp_input = idp_get_params_ptrs.size() > 1u ||
                                  idp_get_params_ptrs[0]->providers.size() > 1u;
  if (is_multi_idp_input && !IsFedCmMultipleIdentityProvidersEnabled()) {
    std::move(callback).Run(RequestTokenStatus::kError, absl::nullopt, "");
    return;
  }

  // Check that providers are non-empty.
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    if (idp_get_params_ptr->providers.size() == 0) {
      std::move(callback).Run(RequestTokenStatus::kError, absl::nullopt, "");
      return;
    }
  }

  if (!fedcm_metrics_) {
    // TODO(crbug.com/1307709): Handle FedCmMetrics for multiple IDPs.
    fedcm_metrics_ =
        CreateFedCmMetrics(idp_get_params_ptrs[0]->providers[0]->config_url,
                           render_frame_host().GetPageUkmSourceId(),
                           /*is_disabled=*/idp_get_params_ptrs.size() > 1);
  }

  if (HasPendingRequest()) {
    fedcm_metrics_->RecordRequestTokenStatus(TokenStatus::kTooManyRequests);
    std::move(callback).Run(RequestTokenStatus::kErrorTooManyRequests,
                            absl::nullopt, "");
    return;
  }

  auth_request_token_callback_ = std::move(callback);
  GetPageData(&render_frame_host())->SetHasPendingWebIdentityRequest(true);
  network_manager_ = CreateNetworkManager();
  request_dialog_controller_ = CreateDialogController();
  start_time_ = base::TimeTicks::Now();

  FederatedApiPermissionStatus permission_status =
      api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin());

  absl::optional<TokenStatus> error_token_status;
  FederatedAuthRequestResult request_result =
      FederatedAuthRequestResult::kError;

  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_token_status = TokenStatus::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED:
      error_token_status = TokenStatus::kThirdPartyCookiesBlocked;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      error_token_status = TokenStatus::kDisabledInSettings;
      request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      error_token_status = TokenStatus::kDisabledEmbargo;
      request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
      break;
  }

  if (error_token_status) {
    CompleteRequestWithError(request_result, *error_token_status,
                             /*should_delay_callback=*/true);
    return;
  }

  std::set<GURL> pending_idps;
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      // Throw an error if duplicate IDPs are specified.
      const bool is_unique_idp =
          pending_idps.insert(idp_ptr->config_url).second;
      if (!is_unique_idp) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 /*token_status=*/absl::nullopt,
                                 /*should_delay_callback=*/false);
        return;
      }

      if (!network::IsOriginPotentiallyTrustworthy(
              url::Origin::Create(idp_ptr->config_url))) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 TokenStatus::kIdpNotPotentiallyTrustworthy,
                                 /*should_delay_callback=*/false);
        return;
      }

      // TODO(crbug.com/1382545): Handle
      // ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp in the multi
      // IDP use case.
      bool has_failing_idp_signin_status =
          webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
              idp_ptr->config_url, permission_delegate_);

      if (has_failing_idp_signin_status &&
          GetFedCmIdpSigninStatusMode() == FedCmIdpSigninStatusMode::ENABLED) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 TokenStatus::kNotSignedInWithIdp,
                                 /*should_delay_callback=*/true);
        return;
      }
    }
  }
  CHECK(pending_idps_.empty());
  pending_idps_ = std::move(pending_idps);

  base::flat_map<GURL, IdentityProviderGetInfo> get_infos;

  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      idp_order_.push_back(idp_ptr->config_url);
      blink::mojom::RpContext rp_context =
          IsFedCmRpContextEnabled() ? idp_get_params_ptr->context
                                    : blink::mojom::RpContext::kSignIn;
      get_infos.emplace(
          idp_ptr->config_url,
          IdentityProviderGetInfo(idp_ptr.Clone(),
                                  idp_get_params_ptr->prefer_auto_sign_in &&
                                      IsFedCmAutoSigninEnabled(),
                                  rp_context));
    }
  }

  int icon_ideal_size = request_dialog_controller_->GetBrandIconIdealSize();
  int icon_minimum_size = request_dialog_controller_->GetBrandIconMinimumSize();

  provider_fetcher_ =
      std::make_unique<FederatedProviderFetcher>(network_manager_.get());
  provider_fetcher_->Start(
      idp_order_, icon_ideal_size, icon_minimum_size,
      base::BindOnce(&FederatedAuthRequestImpl::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(get_infos)));
}

void FederatedAuthRequestImpl::RequestUserInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    RequestUserInfoCallback callback) {
  if (!IsFedCmUserInfoEnabled()) {
    // This could happen with a compromised renderer. Exit early such that we
    // don't proceed when the flag is off or crash the browser.
    std::move(callback).Run(RequestUserInfoStatus::kError, absl::nullopt);
    return;
  }

  if (user_info_request_) {
    std::move(callback).Run(RequestUserInfoStatus::kErrorTooManyRequests,
                            absl::nullopt);
    return;
  }

  if (!fedcm_metrics_) {
    fedcm_metrics_ = CreateFedCmMetrics(
        provider->config_url, render_frame_host().GetPageUkmSourceId(),
        /*is_disabled=*/false);
  }

  auto network_manager = IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
  user_info_request_ = FederatedAuthUserInfoRequest::CreateAndStart(
      std::move(network_manager), api_permission_delegate_.get(),
      permission_delegate_.get(), &render_frame_host(), fedcm_metrics_.get(),
      std::move(provider),
      base::BindOnce(&FederatedAuthRequestImpl::CompleteUserInfoRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FederatedAuthRequestImpl::CancelTokenRequest() {
  if (!auth_request_token_callback_) {
    return;
  }

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.

  CompleteRequestWithError(FederatedAuthRequestResult::kErrorCanceled,
                           TokenStatus::kAborted,
                           /*should_delay_callback=*/false);
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::LogoutRps(
    std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
    LogoutRpsCallback callback) {
  if (HasPendingRequest()) {
    std::move(callback).Run(LogoutRpsStatus::kErrorTooManyRequests);
    return;
  }

  DCHECK(logout_requests_.empty());

  logout_callback_ = std::move(callback);
  GetPageData(&render_frame_host())->SetHasPendingWebIdentityRequest(true);

  if (logout_requests.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (base::ranges::any_of(logout_requests, [](auto& request) {
        return !request->url.is_valid();
      })) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::FARI_LOGOUT_BAD_ENDPOINT);
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  for (auto& request : logout_requests) {
    logout_requests_.push(std::move(request));
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin())) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  network_manager_ = CreateNetworkManager();

  if (!IsFedCmIdpSignoutEnabled()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin()) !=
      FederatedApiPermissionStatus::GRANTED) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  // TODO(kenrb): These should be parallelized rather than being dispatched
  // serially. https://crbug.com/1200581.
  DispatchOneLogout();
}

void FederatedAuthRequestImpl::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    blink::mojom::IdpSigninStatus status) {
  // We only allow setting the IDP signin status when the subresource is
  // loaded from the same origin as the document. This is to protect from
  // an RP embedding a tracker resource that would set this signin status
  // for the tracker, enabling the FedCM request.
  // This behavior may change in https://crbug.com/1382193
  if (!origin().IsSameOriginWith(idp_origin))
    return;
  permission_delegate_->SetIdpSigninStatus(
      idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

bool FederatedAuthRequestImpl::HasPendingRequest() const {
  bool has_pending_request =
      GetPageData(&render_frame_host())->HasPendingWebIdentityRequest();
  DCHECK(has_pending_request ||
         (!auth_request_token_callback_ && !logout_callback_));
  return has_pending_request;
}

void FederatedAuthRequestImpl::OnAllConfigAndWellKnownFetched(
    base::flat_map<GURL, IdentityProviderGetInfo> get_infos,
    std::vector<FederatedProviderFetcher::FetchResult> fetch_results) {
  provider_fetcher_.reset();

  for (const FederatedProviderFetcher::FetchResult& fetch_result :
       fetch_results) {
    const GURL& identity_provider_config_url =
        fetch_result.identity_provider_config_url;
    auto get_info_it = get_infos.find(identity_provider_config_url);
    CHECK(get_info_it != get_infos.end());

    metrics_endpoints_[identity_provider_config_url] =
        fetch_result.endpoints.metrics;

    std::unique_ptr<IdentityProviderInfo> idp_info =
        std::make_unique<IdentityProviderInfo>(
            std::move(get_info_it->second.provider),
            std::move(fetch_result.endpoints),
            fetch_result.metadata ? std::move(*fetch_result.metadata)
                                  : IdentityProviderMetadata(),
            get_info_it->second.prefer_auto_signin,
            get_info_it->second.rp_context);

    if (fetch_result.error) {
      const FederatedProviderFetcher::FetchError& fetch_error =
          *fetch_result.error;
      if (fetch_error.additional_console_error_message) {
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            *fetch_error.additional_console_error_message);
      }
      OnFetchDataForIdpFailed(std::move(idp_info), fetch_error.result,
                              fetch_error.token_status,
                              /*should_delay_callback=*/true);
      continue;
    }

    // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
    // false during the API call. e.g. by the login/logout HEADER.
    idp_info->has_failing_idp_signin_status =
        webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
            identity_provider_config_url, permission_delegate_);
    if (idp_info->has_failing_idp_signin_status &&
        GetFedCmIdpSigninStatusMode() == FedCmIdpSigninStatusMode::ENABLED) {
      // Do not send metrics for IDP where the user is not signed-in in order
      // to prevent IDP from using the user IP to make a probabilistic model
      // of which websites a user visits.
      idp_info->endpoints.metrics = GURL();

      OnFetchDataForIdpFailed(std::move(idp_info),
                              FederatedAuthRequestResult::kError,
                              TokenStatus::kNotSignedInWithIdp,
                              /*should_delay_callback=*/true);
      continue;
    }

    GURL accounts_endpoint = idp_info->endpoints.accounts;
    std::string client_id = idp_info->provider->client_id;
    network_manager_->SendAccountsRequest(
        accounts_endpoint, client_id,
        base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(idp_info)));
  }
}

void FederatedAuthRequestImpl::OnClientMetadataResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const IdpNetworkRequestManager::AccountList& accounts,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata client_metadata) {
  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.
  OnFetchDataForIdpSucceeded(std::move(idp_info), accounts, client_metadata);
}

void FederatedAuthRequestImpl::OnFetchDataForIdpSucceeded(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const IdpNetworkRequestManager::AccountList& accounts,
    const IdpNetworkRequestManager::ClientMetadata& client_metadata) {
  const GURL& idp_config_url = idp_info->provider->config_url;
  const std::string idp_for_display = FormatUrlForDisplay(idp_config_url);
  idp_info->data =
      IdentityProviderData(idp_for_display, accounts, idp_info->metadata,
                           ClientMetadata{client_metadata.terms_of_service_url,
                                          client_metadata.privacy_policy_url},
                           idp_info->rp_context);
  idp_infos_[idp_config_url] = std::move(idp_info);

  pending_idps_.erase(idp_config_url);
  MaybeShowAccountsDialog();
}

void FederatedAuthRequestImpl::OnFetchDataForIdpFailed(
    const std::unique_ptr<IdentityProviderInfo> idp_info,
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    bool should_delay_callback) {
  const GURL& idp_config_url = idp_info->provider->config_url;
  if (idp_order_.size() == 1u) {
    CompleteRequestWithError(result, token_status, should_delay_callback);
    return;
  }

  AddInspectorIssue(result);
  AddConsoleErrorMessage(result);

  if (IsFedCmMetricsEndpointEnabled())
    SendFailedTokenRequestMetrics(idp_info->endpoints.metrics, result);

  pending_idps_.erase(idp_config_url);
  metrics_endpoints_.erase(idp_config_url);
  std::vector<GURL>::iterator idp_order_new_end_it =
      std::remove(idp_order_.begin(), idp_order_.end(), idp_config_url);
  idp_order_.erase(idp_order_new_end_it, idp_order_.end());

  idp_infos_.erase(idp_config_url);
  // Do not use `idp_config_url` after this line because the reference is no
  // longer valid.

  MaybeShowAccountsDialog();
}

void FederatedAuthRequestImpl::MaybeShowAccountsDialog() {
  if (!pending_idps_.empty())
    return;

  bool is_visible = (render_frame_host().IsActive() &&
                     render_frame_host().GetVisibilityState() ==
                         content::PageVisibilityState::kVisible);
  fedcm_metrics_->RecordWebContentsVisibilityUponReadyToShowDialog(is_visible);
  // Does not show the dialog if the user has left the page. e.g. they may
  // open a new tab before browser is ready to show the dialog.
  if (!is_visible) {
    CompleteRequestWithError(FederatedAuthRequestResult::kErrorRpPageNotVisible,
                             TokenStatus::kRpPageNotVisible,
                             /*should_delay_callback=*/true);
    return;
  }

  show_accounts_dialog_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordShowAccountsDialogTime(show_accounts_dialog_time_ -
                                               start_time_);
  std::string rp_url_for_display = FormatOriginForDisplay(GetEmbeddingOrigin());

  // TODO(crbug.com/1383384): Handle prefer_auto_sign_in for multi IDP.
  bool prefer_auto_signin = true;
  std::vector<IdentityProviderData> idp_data_for_display;
  for (const auto& idp : idp_order_) {
    auto idp_info_it = idp_infos_.find(idp);
    if (idp_info_it != idp_infos_.end() && idp_info_it->second->data) {
      idp_data_for_display.push_back(*idp_info_it->second->data);
      prefer_auto_signin &= idp_info_it->second->prefer_auto_signin;
    }
  }

  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());

  // Auto signs in returning users if they have a single returning account and
  // are signing in.
  // TODO(yigu): Add additional controls for RP/IDP/User for this flow.
  // https://crbug.com/1236678.
  bool is_auto_sign_in =
      prefer_auto_signin && HasSingleReturningAccount(idp_data_for_display);

  // TODO(crbug.com/1382863): Handle UI where some IDPs are successful and some
  // IDPs are failing in the multi IDP case.
  request_dialog_controller_->ShowAccountsDialog(
      WebContents::FromRenderFrameHost(&render_frame_host()),
      rp_url_for_display, idp_data_for_display,
      is_auto_sign_in ? SignInMode::kAuto : SignInMode::kExplicit,
      base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::HandleAccountsFetchFailure(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    absl::optional<bool> old_idp_signin_status,
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status) {
  if (GetFedCmIdpSigninStatusMode() == FedCmIdpSigninStatusMode::DISABLED) {
    OnFetchDataForIdpFailed(std::move(idp_info), result, token_status,
                            /*should_delay_callback=*/true);
    return;
  }

  url::Origin idp_origin = url::Origin::Create(idp_info->provider->config_url);

  if (!old_idp_signin_status.has_value() ||
      GetFedCmIdpSigninStatusMode() == FedCmIdpSigninStatusMode::METRICS_ONLY) {
    OnFetchDataForIdpFailed(std::move(idp_info), result, token_status,
                            /*should_delay_callback=*/true);
    return;
  }

  bool is_visible = (render_frame_host().IsActive() &&
                     render_frame_host().GetVisibilityState() ==
                         content::PageVisibilityState::kVisible);
  if (!is_visible) {
    CompleteRequestWithError(FederatedAuthRequestResult::kErrorRpPageNotVisible,
                             TokenStatus::kRpPageNotVisible,
                             /*should_delay_callback=*/true);
    return;
  }

  // TODO(crbug.com/1357790): we should figure out how to handle multiple IDP
  // w.r.t. showing a static failure UI. e.g. one IDP is always successful and
  // one always returns 404.
  WebContents* rp_web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());
  // TODO(crbug.com/1382495): Handle failure UI in the multi IDP case.
  request_dialog_controller_->ShowFailureDialog(
      rp_web_contents, FormatOriginForDisplay(GetEmbeddingOrigin()),
      FormatOriginForDisplay(idp_origin),
      base::BindOnce(
          &FederatedAuthRequestImpl::OnDismissFailureDialog,
          weak_ptr_factory_.GetWeakPtr(), FederatedAuthRequestResult::kError,
          TokenStatus::kNotSignedInWithIdp, /*should_delay_callback=*/true));
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::AccountList accounts) {
  GURL idp_config_url = idp_info->provider->config_url;
  const absl::optional<bool> old_idp_signin_status =
      permission_delegate_->GetIdpSigninStatus(
          url::Origin::Create(idp_config_url));
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      idp_config_url, status, idp_info->has_failing_idp_signin_status,
      permission_delegate_, fedcm_metrics_.get());

  constexpr char kAccountsUrl[] = "accounts endpoint";
  switch (status.parse_status) {
    case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound,
          TokenStatus::kAccountsHttpNotFound);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
          TokenStatus::kAccountsNoResponse);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
          TokenStatus::kAccountsInvalidResponse);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty,
          TokenStatus::kAccountsListEmpty);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kSuccess: {
      if (IsFedCmLoginHintEnabled()) {
        FilterAccountsWithLoginHint(idp_info->provider->login_hint, accounts);
        if (accounts.empty()) {
          // If there are no accounts after filtering based on the login hint,
          // treat this exactly the same as if we had received an empty accounts
          // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
          HandleAccountsFetchFailure(
              std::move(idp_info), old_idp_signin_status,
              FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty,
              TokenStatus::kAccountsListEmpty);
          return;
        }
      }
      ComputeLoginStateAndReorderAccounts(idp_info->provider, accounts);

      bool need_client_metadata = false;
      for (const IdentityRequestAccount& account : accounts) {
        // ComputeLoginStateAndReorderAccounts() should have populated
        // IdentityRequestAccount::login_state.
        DCHECK(account.login_state);
        if (*account.login_state == LoginState::kSignUp) {
          need_client_metadata = true;
          break;
        }
      }

      if (need_client_metadata &&
          webid::IsEndpointUrlValid(idp_info->provider->config_url,
                                    idp_info->endpoints.client_metadata)) {
        // Copy OnClientMetadataResponseReceived() parameters because `idp_info`
        // is moved.
        GURL client_metadata_endpoint = idp_info->endpoints.client_metadata;
        std::string client_id = idp_info->provider->client_id;
        network_manager_->FetchClientMetadata(
            client_metadata_endpoint, client_id,
            base::BindOnce(
                &FederatedAuthRequestImpl::OnClientMetadataResponseReceived,
                weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                std::move(accounts)));
      } else {
        OnFetchDataForIdpSucceeded(std::move(idp_info), accounts,
                                   IdpNetworkRequestManager::ClientMetadata());
      }
    }
  }
}

void FederatedAuthRequestImpl::ComputeLoginStateAndReorderAccounts(
    const IdentityProviderConfigPtr& idp,
    IdpNetworkRequestManager::AccountList& accounts) {
  // Populate the accounts login state.
  for (auto& account : accounts) {
    // Record when IDP and browser have different user sign-in states.
    bool idp_claimed_sign_in = account.login_state == LoginState::kSignIn;
    bool browser_observed_sign_in = permission_delegate_->HasSharingPermission(
        origin(), GetEmbeddingOrigin(), url::Origin::Create(idp->config_url),
        account.id);

    if (idp_claimed_sign_in == browser_observed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kMatch);
    } else if (idp_claimed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kIdpClaimedSignIn);
    } else {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kBrowserObservedSignIn);
    }

    // We set the login state based on the IDP response if it sends
    // back an approved_clients list. If it does not, we need to set
    // it here based on browser state.
    if (account.login_state)
      continue;
    LoginState login_state = LoginState::kSignUp;
    // Consider this a sign-in if we have seen a successful sign-up for
    // this account before.
    if (browser_observed_sign_in) {
      login_state = LoginState::kSignIn;
    }
    account.login_state = login_state;
  }

  // Now that the login states have been computed, order accounts so that the
  // returning accounts go first and the other accounts go afterwards. Since the
  // number of accounts is likely very small, sorting by login_state should be
  // fast.
  std::sort(accounts.begin(), accounts.end(), [](const auto& a, const auto& b) {
    return a.login_state < b.login_state;
  });
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
  if (api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin()) !=
      FederatedApiPermissionStatus::GRANTED) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorDisabledInSettings,
        TokenStatus::kDisabledInSettings,
        /*should_delay_callback=*/true);
    return;
  }

  fedcm_metrics_->RecordIsSignInUser(is_sign_in);

  api_permission_delegate_->RemoveEmbargoAndResetCounts(GetEmbeddingOrigin());

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordContinueOnDialogTime(select_account_time_ -
                                             show_accounts_dialog_time_);

  network_manager_->SendTokenRequest(
      idp_info.endpoints.token, account_id_,
      ComputeUrlEncodedTokenPostData(idp_info.provider->client_id,
                                     idp_info.provider->nonce, account_id,
                                     is_sign_in),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()));
}

void FederatedAuthRequestImpl::OnDismissFailureDialog(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    bool should_delay_callback,
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  CompleteRequestWithError(result, token_status, should_delay_callback);
}

void FederatedAuthRequestImpl::OnDialogDismissed(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  // Clicking the close button and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android.
  bool should_embargo = false;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::CLOSE_BUTTON:
    case IdentityRequestDialogController::DismissReason::SWIPE:
      should_embargo = true;
      break;
    default:
      break;
  }

  if (should_embargo) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    fedcm_metrics_->RecordCancelOnDialogTime(dismiss_dialog_time -
                                             show_accounts_dialog_time_);
  }
  fedcm_metrics_->RecordCancelReason(dismiss_reason);

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
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    IdentityProviderConfigPtr idp,
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& id_token) {
  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc.. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // |token_request_delay_| seconds for better UX.
  token_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time = token_response_time_ - select_account_time_;
  if (ShouldCompleteRequestImmediately() ||
      fetch_time >= token_request_delay_) {
    CompleteTokenRequest(std::move(idp), status, id_token);
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FederatedAuthRequestImpl::CompleteTokenRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(idp), status,
                     id_token),
      token_request_delay_ - fetch_time);
}

void FederatedAuthRequestImpl::CompleteTokenRequest(
    IdentityProviderConfigPtr idp,
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& token) {
  DCHECK(!start_time_.is_null());
  constexpr char kIdAssertionUrl[] = "id assertion endpoint";
  switch (status.parse_status) {
    case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound,
          TokenStatus::kIdTokenHttpNotFound,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse,
          TokenStatus::kIdTokenNoResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
          TokenStatus::kIdTokenInvalidResponse,
          /*should_delay_callback=*/true);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
      NOTREACHED() << "kEmptyListError is undefined for CompleteTokenRequest";
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kSuccess: {
      // Grant sharing permission specific to *this account*.
      //
      // TODO(majidvp): But wait which account?
      //   1) The account that user selected in our UI (i.e., account_id_) or
      //   2) The one for which the IDP generated a token.
      //
      // Ideally these are one and the same but currently there is no
      // enforcement for that equality so they could be different. In the
      // future we may want to enforce that the token account (aka subject)
      // matches the user selected account. But for now these questions are
      // moot since we don't actually inspect the returned idtoken.
      // https://crbug.com/1199088
      CHECK(!account_id_.empty());
      permission_delegate_->GrantSharingPermission(
          origin(), GetEmbeddingOrigin(), url::Origin::Create(idp->config_url),
          account_id_);

      permission_delegate_->GrantActiveSession(
          origin(), url::Origin::Create(idp->config_url), account_id_);

      fedcm_metrics_->RecordTokenResponseAndTurnaroundTime(
          token_response_time_ - select_account_time_,
          token_response_time_ - start_time_);

      if (IsFedCmMetricsEndpointEnabled()) {
        for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
          const GURL& metrics_endpoint = metrics_endpoint_kv.second;
          if (!metrics_endpoint.is_valid())
            continue;

          if (metrics_endpoint_kv.first == idp->config_url) {
            network_manager_->SendSuccessfulTokenRequestMetrics(
                metrics_endpoint, show_accounts_dialog_time_ - start_time_,
                select_account_time_ - show_accounts_dialog_time_,
                token_response_time_ - select_account_time_,
                token_response_time_ - start_time_);
          } else {
            // Send kUserFailure so that IDP cannot tell difference between user
            // selecting a different IDP and user dismissing dialog without
            // selecting any IDP.
            network_manager_->SendFailedTokenRequestMetrics(
                metrics_endpoint, IdpNetworkRequestManager::
                                      MetricsEndpointErrorCode::kUserFailure);
          }
        }
      }

      CompleteRequest(FederatedAuthRequestResult::kSuccess,
                      TokenStatus::kSuccess, idp->config_url, token,
                      /*should_delay_callback=*/false);
      return;
    }
  }
}

void FederatedAuthRequestImpl::DispatchOneLogout() {
  auto logout_request = std::move(logout_requests_.front());
  DCHECK(logout_request->url.is_valid());
  std::string account_id = logout_request->account_id;
  auto logout_origin = url::Origin::Create(logout_request->url);
  logout_requests_.pop();

  if (permission_delegate_->HasActiveSession(logout_origin, origin(),
                                             account_id)) {
    network_manager_->SendLogout(
        logout_request->url,
        base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    permission_delegate_->RevokeActiveSession(logout_origin, origin(),
                                              account_id);
  } else {
    if (logout_requests_.empty()) {
      CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
      return;
    }

    DispatchOneLogout();
  }
}

void FederatedAuthRequestImpl::OnLogoutCompleted() {
  if (logout_requests_.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
    return;
  }

  DispatchOneLogout();
}

void FederatedAuthRequestImpl::CompleteRequestWithError(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    bool should_delay_callback) {
  CompleteRequest(result, token_status,
                  /*selected_idp_config_url=*/absl::nullopt,
                  /*token=*/"", should_delay_callback);
}

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    const absl::optional<GURL>& selected_idp_config_url,
    const std::string& id_token,
    bool should_delay_callback) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess || id_token.empty());

  if (!auth_request_token_callback_) {
    return;
  }

  if (token_status)
    fedcm_metrics_->RecordRequestTokenStatus(*token_status);

  if (!errors_logged_to_console_ &&
      result != FederatedAuthRequestResult::kSuccess) {
    errors_logged_to_console_ = true;

    AddInspectorIssue(result);
    AddConsoleErrorMessage(result);

    if (IsFedCmMetricsEndpointEnabled()) {
      for (const auto& metrics_endpoint_kv : metrics_endpoints_)
        SendFailedTokenRequestMetrics(metrics_endpoint_kv.second, result);
    }
  }

  CleanUp();

  if (!should_delay_callback || ShouldCompleteRequestImmediately()) {
    GetPageData(&render_frame_host())->SetHasPendingWebIdentityRequest(false);
    errors_logged_to_console_ = false;

    RequestTokenStatus status =
        FederatedAuthRequestResultToRequestTokenStatus(result);
    std::move(auth_request_token_callback_)
        .Run(status, selected_idp_config_url, id_token);
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
  if (!metrics_endpoint.is_valid())
    return;

  network_manager_->SendFailedTokenRequestMetrics(
      metrics_endpoint,
      FederatedAuthRequestResultToMetricsEndpointErrorCode(result));
}

void FederatedAuthRequestImpl::CleanUp() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  provider_fetcher_.reset();
  account_id_ = std::string();
  start_time_ = base::TimeTicks();
  show_accounts_dialog_time_ = base::TimeTicks();
  select_account_time_ = base::TimeTicks();
  token_response_time_ = base::TimeTicks();
  idp_infos_.clear();
  pending_idps_.clear();
  idp_order_.clear();
  metrics_endpoints_.clear();
}

void FederatedAuthRequestImpl::AddInspectorIssue(
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
  absl::optional<std::string> console_message =
      webid::ComputeConsoleMessageForHttpResponseCode(fetch_description,
                                                      response_code);
  if (console_message) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, *console_message);
  }
}

bool FederatedAuthRequestImpl::ShouldCompleteRequestImmediately() {
  return api_permission_delegate_->ShouldCompleteRequestImmediately();
}

url::Origin FederatedAuthRequestImpl::GetEmbeddingOrigin() const {
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutRpsStatus status) {
  network_manager_.reset();
  base::queue<blink::mojom::LogoutRpsRequestPtr>().swap(logout_requests_);
  if (logout_callback_) {
    std::move(logout_callback_).Run(status);
    logout_callback_.Reset();
    GetPageData(&render_frame_host())->SetHasPendingWebIdentityRequest(false);
  }
}

void FederatedAuthRequestImpl::CompleteUserInfoRequest(
    RequestUserInfoCallback callback,
    blink::mojom::RequestUserInfoStatus status,
    absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
  if (!user_info_request_) {
    return;
  }

  std::move(callback).Run(status, std::move(user_info));
  user_info_request_.reset();
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager() {
  if (mock_network_manager_)
    return std::move(mock_network_manager_);

  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

std::unique_ptr<IdentityRequestDialogController>
FederatedAuthRequestImpl::CreateDialogController() {
  if (mock_dialog_controller_)
    return std::move(mock_dialog_controller_);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForFedCM)) {
    std::string selected_account =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUseFakeUIForFedCM);
    return std::make_unique<FakeIdentityRequestDialogController>(
        selected_account.empty()
            ? absl::nullopt
            : absl::optional<std::string>(selected_account));
  }

  return GetContentClient()->browser()->CreateIdentityRequestDialogController();
}

void FederatedAuthRequestImpl::SetTokenRequestDelayForTests(
    base::TimeDelta delay) {
  token_request_delay_ = delay;
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void FederatedAuthRequestImpl::OnRejectRequest() {
  if (auth_request_token_callback_) {
    DCHECK(!logout_callback_);
    DCHECK(errors_logged_to_console_);
    CompleteRequestWithError(FederatedAuthRequestResult::kError, absl::nullopt,
                             /*should_delay_callback=*/false);
  }
}

}  // namespace content
