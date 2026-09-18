// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/enterprise/net/core/enterprise_proxy_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/web/public/web_state_delegate.h"

@class NSURLProtectionSpace;
@class NSURLCredential;
@class NSURLResponse;

namespace web {
class WebState;
}  // namespace web

namespace web {
class ProxyConfigurationProvider;
}  // namespace web

// Profile-scoped `KeyedService` responsible for managing web-layer proxy
// configuration and observing enterprise proxy route updates.
class ProxyServiceController
    : public KeyedService,
      public enterprise_net::EnterpriseProxyService::Observer {
 public:
  // Initializes the controller with `enterprise_proxy_service` and
  // `proxy_configuration_provider`. Both parameters must be non-null. Starts
  // observing route updates and performs initial route configuration.
  ProxyServiceController(
      enterprise_net::EnterpriseProxyService* enterprise_proxy_service,
      web::ProxyConfigurationProvider* proxy_configuration_provider);
  ~ProxyServiceController() override;

  ProxyServiceController(const ProxyServiceController&) = delete;
  ProxyServiceController& operator=(const ProxyServiceController&) = delete;
  ProxyServiceController(ProxyServiceController&&) = delete;
  ProxyServiceController& operator=(ProxyServiceController&&) = delete;

  // Evaluates a 407 Proxy Authentication challenge against enterprise proxy
  // policies. Returns true if the challenge is handled by enterprise proxy
  // logic, or false if the caller should fall back to standard auth handling.
  virtual bool MaybeHandleProxyAuthChallenge(
      web::WebState* source,
      NSURLProtectionSpace* protection_space,
      NSURLCredential* proposed_credential,
      NSURLResponse* failure_response,
      web::WebStateDelegate::ProxyAuthCallback callback);

  // KeyedService:
  // Stops observing `enterprise_proxy_service_` and clears references.
  void Shutdown() override;

  // EnterpriseProxyService::Observer:
  // Translates dynamic routing configs from `enterprise_proxy_service_` and
  // forwards them to `proxy_configuration_provider_`.
  void OnDynamicProxyConfigsStatusChanged() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Observed and cleared on destruction or `Shutdown()`.
  raw_ptr<enterprise_net::EnterpriseProxyService> enterprise_proxy_service_ =
      nullptr;
  // Owned by `BrowserState`, which outlives `ProfileIOS` keyed services.
  raw_ptr<web::ProxyConfigurationProvider> proxy_configuration_provider_ =
      nullptr;

  base::ScopedObservation<enterprise_net::EnterpriseProxyService,
                          enterprise_net::EnterpriseProxyService::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_
