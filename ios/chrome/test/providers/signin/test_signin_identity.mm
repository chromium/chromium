// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

bool IsSigninSupported() {
  return true;
}

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager(
    id<SingleSignOnService> sso_service) {
  return std::make_unique<FakeSystemIdentityManager>();
}

}  // namespace provider
}  // namespace ios
