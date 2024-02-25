// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"

namespace ios {
namespace provider {

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager(
    id<SingleSignOnService> sso_service) {
  return std::make_unique<FakeSystemIdentityManager>();
}

}  // namespace provider
}  // namespace ios
