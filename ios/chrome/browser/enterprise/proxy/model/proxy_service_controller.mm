// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_auth_challenge_util.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"
#import "ios/web/public/proxy/pvd_route_translator.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/auth.h"
#import "net/http/http_response_headers.h"
#import "url/gurl.h"

namespace {

// Navigation ID reported for challenges that are not attributed to a
// navigation. `EnterpriseProxyErrorService` records no error for it.
constexpr int64_t kUnknownNavigationId = 0;

// Returns whether `navigation_id` is the pending main-frame navigation of
// `web_state`.
bool IsPendingNavigation(web::WebState* web_state, int64_t navigation_id) {
  if (!web_state) {
    return false;
  }
  EnterpriseProxyTabHelper* tab_helper =
      EnterpriseProxyTabHelper::FromWebState(web_state);
  return tab_helper && tab_helper->pending_navigation_id() == navigation_id;
}

// Returns the ID of the pending main-frame navigation of `web_state` if the
// challenge for `destination_url` belongs to it, or `kUnknownNavigationId`.
//
// WebKit doesn't tell which request a challenge is for, so it is attributed to
// the navigation only while the navigation is pending and only if the
// challenged URL is the navigation's URL. Challenges for subresources and for
// requests made after commit are left unattributed, as on desktop where only
// main-frame requests get auth UI. The fragment is ignored because it is never
// sent with the request.
int64_t GetNavigationIdForChallenge(web::WebState* web_state,
                                    const GURL& destination_url) {
  EnterpriseProxyTabHelper* tab_helper =
      EnterpriseProxyTabHelper::FromWebState(web_state);
  if (!tab_helper || !tab_helper->pending_navigation_id().has_value() ||
      destination_url.GetWithoutRef() !=
          tab_helper->pending_navigation_url().GetWithoutRef()) {
    return kUnknownNavigationId;
  }
  return *tab_helper->pending_navigation_id();
}

// Adapts the credentials resolved by `enterprise_proxy_error_service` into the
// `(username, password, error)` triple that
// `web::WebStateDelegate::ProxyAuthCallback` expects. Without credentials, the
// error recorded for the navigation `navigation_id` of `web_state` is dropped
// if that navigation is no longer pending.
void RunProxyAuthCallback(
    enterprise_net::EnterpriseProxyErrorService* enterprise_proxy_error_service,
    base::WeakPtr<web::WebState> web_state,
    int64_t navigation_id,
    web::WebStateDelegate::ProxyAuthCallback callback,
    const std::optional<net::AuthCredentials>& credentials) {
  if (credentials.has_value()) {
    std::move(callback).Run(base::SysUTF16ToNSString(credentials->username()),
                            base::SysUTF16ToNSString(credentials->password()),
                            /*error=*/nil);
    return;
  }

  if (navigation_id == kUnknownNavigationId) {
    std::move(callback).Run(/*username=*/nil, /*password=*/nil, /*error=*/nil);
    return;
  }

  // If the attributed navigation is no longer pending (e.g. it finished or was
  // replaced while the access token was fetched), nothing will take the error
  // just recorded for it, so it is dropped.
  if (!IsPendingNavigation(web_state.get(), navigation_id)) {
    enterprise_proxy_error_service->RemoveDisguisedError(navigation_id);
    std::move(callback).Run(/*username=*/nil, /*password=*/nil, /*error=*/nil);
    return;
  }

  // TODO(crbug.com/543371697): Fail the navigation with a dedicated enterprise
  // proxy error once `ChromeWebClient::PrepareErrorPage()` shows an error page
  // for it. Until then, `EnterpriseProxyTabHelper` drops the error recorded for
  // the navigation when the navigation finishes.
  std::move(callback).Run(/*username=*/nil, /*password=*/nil, /*error=*/nil);
}

}  // namespace

ProxyServiceController::ProxyServiceController(
    enterprise_net::EnterpriseProxyService* enterprise_proxy_service,
    enterprise_net::EnterpriseProxyErrorService* enterprise_proxy_error_service,
    web::ProxyConfigurationProvider* proxy_configuration_provider)
    : enterprise_proxy_service_(enterprise_proxy_service),
      enterprise_proxy_error_service_(enterprise_proxy_error_service),
      proxy_configuration_provider_(proxy_configuration_provider) {
  CHECK(enterprise_proxy_service_);
  CHECK(enterprise_proxy_error_service_);
  CHECK(proxy_configuration_provider_);
  scoped_observation_.Observe(enterprise_proxy_service_);
  OnDynamicProxyConfigsStatusChanged();
}

ProxyServiceController::~ProxyServiceController() = default;

bool ProxyServiceController::MaybeHandleProxyAuthChallenge(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    NSURLResponse* failure_response,
    web::WebStateDelegate::ProxyAuthCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(source);

  // Null once `Shutdown()` has run.
  if (!enterprise_proxy_error_service_) {
    return false;
  }

  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateProxyAuthHeadersFromNSURLResponse(failure_response);
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(protection_space,
                                                 response_headers);
  if (!auth_info.has_value()) {
    return false;
  }

  const GURL destination_url = net::GURLWithNSURL(failure_response.URL);
  const int64_t navigation_id =
      GetNavigationIdForChallenge(source, destination_url);
  std::optional<
      enterprise_net::EnterpriseProxyErrorService::PendingInterception>
      interception =
          enterprise_proxy_error_service_->EvaluateProxyAuthChallenge(
              *auth_info, destination_url, response_headers, navigation_id);
  if (!interception.has_value()) {
    return false;
  }

  // `enterprise_proxy_error_service_` is the one running the callback, so it
  // is alive whenever the callback runs.
  enterprise_proxy_error_service_->ResolveProxyAuthChallenge(
      *std::move(interception),
      base::BindOnce(&RunProxyAuthCallback,
                     base::Unretained(enterprise_proxy_error_service_.get()),
                     source->GetWeakPtr(), navigation_id, std::move(callback)));
  return true;
}

void ProxyServiceController::Shutdown() {
  scoped_observation_.Reset();
  enterprise_proxy_service_ = nullptr;
  enterprise_proxy_error_service_ = nullptr;
  proxy_configuration_provider_ = nullptr;
}

void ProxyServiceController::OnDynamicProxyConfigsStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(enterprise_proxy_service_);
  CHECK(proxy_configuration_provider_);
  proxy_configuration_provider_->UpdateProxyConfiguration(
      web::TranslateProvisioningDomainRoutingConfig(
          enterprise_proxy_service_->GetDynamicRoutingConfig()));
}
