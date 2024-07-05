// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_details.h"

#import "base/check.h"
#import "components/signin/public/identity_manager/account_capabilities.h"

@implementation FakeSystemIdentityDetails {
  AccountCapabilities _pendingCapabilities;
  AccountCapabilities _visibleCapabilities;
  std::unique_ptr<AccountCapabilitiesTestMutator> _pendingCapabilitiesMutator;
}

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity {
  if ((self = [super init])) {
    _pendingCapabilitiesMutator =
        std::make_unique<AccountCapabilitiesTestMutator>(&_pendingCapabilities);
    _identity = identity;
    DCHECK(_identity);
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

@end
