// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"
#import "ios/web/public/proxy/pvd_route_translator.h"

ProxyServiceController::ProxyServiceController(
    enterprise_net::EnterpriseProxyService* enterprise_proxy_service,
    web::ProxyConfigurationProvider* proxy_configuration_provider)
    : enterprise_proxy_service_(enterprise_proxy_service),
      proxy_configuration_provider_(proxy_configuration_provider) {
  CHECK(enterprise_proxy_service_);
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
  // TODO(crbug.com/543371754): Complete enterprise proxy auth challenge
  // handling.
  return false;
}

void ProxyServiceController::Shutdown() {
  scoped_observation_.Reset();
  enterprise_proxy_service_ = nullptr;
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
