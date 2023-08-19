// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/fake_system_identity_details.h"

#import "base/check.h"
#import "components/signin/public/identity_manager/account_capabilities.h"

@implementation FakeSystemIdentityDetails {
  AccountCapabilities _nativeCapabilities;
  std::unique_ptr<AccountCapabilitiesTestMutator> _capabilitiesMutator;
}

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity {
  if ((self = [super init])) {
    _capabilitiesMutator =
        std::make_unique<AccountCapabilitiesTestMutator>(&_nativeCapabilities);
    _identity = identity;
    DCHECK(_identity);
  }
  return self;
}

#pragma mark - Properties

- (const FakeSystemIdentityCapabilitiesMap&)capabilities {
  return _nativeCapabilities.ConvertToAccountCapabilitiesIOS();
}

- (AccountCapabilitiesTestMutator*)capabilitiesMutator {
  return _capabilitiesMutator.get();
}

@end
