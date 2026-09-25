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
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_auth_challenge_util.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"
#import "ios/web/public/proxy/pvd_route_translator.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/auth.h"
#import "net/http/http_response_headers.h"
#import "url/gurl.h"

namespace {

// Navigation ID reported while proxy challenges cannot be correlated with the
// navigation that triggered them.
// TODO(crbug.com/543371697): Plumb the real navigation ID.
constexpr int64_t kUnknownNavigationId = 0;

// Adapts the credentials resolved by `EnterpriseProxyErrorService` into the
// `(username, password, error)` triple that
// `web::WebStateDelegate::ProxyAuthCallback` expects.
//
// TODO(crbug.com/543371697): Fail the navigation once the navigation ID is
// plumbed.
void RunProxyAuthCallback(
    web::WebStateDelegate::ProxyAuthCallback callback,
    const std::optional<net::AuthCredentials>& credentials) {
  if (!credentials.has_value()) {
    std::move(callback).Run(/*username=*/nil, /*password=*/nil, /*error=*/nil);
    return;
  }
  std::move(callback).Run(base::SysUTF16ToNSString(credentials->username()),
                          base::SysUTF16ToNSString(credentials->password()),
                          /*error=*/nil);
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

  std::optional<
      enterprise_net::EnterpriseProxyErrorService::PendingInterception>
      interception =
          enterprise_proxy_error_service_->EvaluateProxyAuthChallenge(
              *auth_info, net::GURLWithNSURL(failure_response.URL),
              response_headers, kUnknownNavigationId);
  if (!interception.has_value()) {
    return false;
  }

  enterprise_proxy_error_service_->ResolveProxyAuthChallenge(
      *std::move(interception),
      base::BindOnce(&RunProxyAuthCallback, std::move(callback)));
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
