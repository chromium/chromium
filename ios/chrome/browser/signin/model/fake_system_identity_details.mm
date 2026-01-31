// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_details.h"

#import "base/check.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

@implementation FakeSystemIdentityDetails {
  AccountCapabilities _pendingCapabilities;
  AccountCapabilities _visibleCapabilities;
  std::unique_ptr<AccountCapabilitiesTestMutator> _pendingCapabilitiesMutator;
}

- (instancetype)initWithFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  if ((self = [super init])) {
    _pendingCapabilitiesMutator =
        std::make_unique<AccountCapabilitiesTestMutator>(&_pendingCapabilities);
    _fakeIdentity = fakeIdentity;
    _cachedAvatar = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(32, 32), UIColor.whiteColor);
    DCHECK(_fakeIdentity);
  }
  return self;
}

#pragma mark - Properties

- (void)updateVisibleCapabilities {
  _visibleCapabilities.UpdateWith(_pendingCapabilities);
}

- (const FakeSystemIdentityCapabilitiesMap&)visibleCapabilities {
  return _visibleCapabilities.ConvertToAccountCapabilitiesIOS();
}

- (AccountCapabilitiesTestMutator*)pendingCapabilitiesMutator {
  return _pendingCapabilitiesMutator.get();
}

- (void)setCachedAvatar:(UIImage*)cachedAvatar {
  _cachedAvatar = cachedAvatar;
  _avatarUpdatedFromLastFetch = YES;
}

@end
