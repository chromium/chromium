// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/fake_reauthentication_service_util.h"

#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"

std::unique_ptr<KeyedService> CreateFakeReauthService(ProfileIOS* profile) {
  id<ReauthenticationProtocol> fake_reauth_module =
      [[MockReauthenticationModule alloc] init];
  return std::make_unique<ReauthenticationService>(fake_reauth_module);
}
