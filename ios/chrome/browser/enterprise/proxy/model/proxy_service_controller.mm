// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"

#import <Foundation/Foundation.h>

ProxyServiceController::ProxyServiceController() = default;

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
