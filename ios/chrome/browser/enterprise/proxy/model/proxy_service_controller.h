// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_

#import "components/keyed_service/core/keyed_service.h"
#import "ios/web/public/web_state_delegate.h"

@class NSURLProtectionSpace;
@class NSURLCredential;
@class NSURLResponse;

namespace web {
class WebState;
}  // namespace web

// Profile-scoped `KeyedService` responsible for managing web-layer proxy
// configuration and observing enterprise proxy route updates.
class ProxyServiceController : public KeyedService {
 public:
  ProxyServiceController(const ProxyServiceController&) = delete;
  ProxyServiceController& operator=(const ProxyServiceController&) = delete;
  ProxyServiceController(ProxyServiceController&&) = delete;
  ProxyServiceController& operator=(ProxyServiceController&&) = delete;

  ProxyServiceController();
  ~ProxyServiceController() override;

  // Evaluates a 407 Proxy Authentication challenge against enterprise proxy
  // policies. Returns true if the challenge is handled by enterprise proxy
  // logic, or false if the caller should fall back to standard auth handling.
  virtual bool MaybeHandleProxyAuthChallenge(
      web::WebState* source,
      NSURLProtectionSpace* protection_space,
      NSURLCredential* proposed_credential,
      NSURLResponse* failure_response,
      web::WebStateDelegate::ProxyAuthCallback callback);
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_
