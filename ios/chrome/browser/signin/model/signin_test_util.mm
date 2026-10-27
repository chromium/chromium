// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_test_util.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

TestProfileIOS* AddManagedProfileWithBuilder(
    TestProfileManagerIOS& profile_manager,
    id<SystemIdentity> managed_identity,
    TestProfileIOS::Builder builder) {
  CHECK(managed_identity);
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  if (!system_identity_manager->ContainsIdentity(managed_identity)) {
    system_identity_manager->AddIdentity(managed_identity);
  }
  return profile_manager.AddManagedProfileForGaiaIDWithBuilder(
      managed_identity.gaiaId, std::move(builder));
}
