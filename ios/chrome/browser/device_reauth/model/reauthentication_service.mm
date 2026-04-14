// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"

#import "base/check.h"

ReauthenticationService::ReauthenticationService(
    id<ReauthenticationProtocol> reauth_module)
    : reauth_module_(reauth_module) {
  CHECK(reauth_module_);
}

ReauthenticationService::~ReauthenticationService() = default;

void ReauthenticationService::Shutdown() {
  reauth_module_ = nil;
}

id<ReauthenticationProtocol> ReauthenticationService::GetReauthModule() {
  return reauth_module_;
}
