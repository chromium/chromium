// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_trusted_vault_observer_internal.h"

#import "ios/web_view/public/cwv_trusted_vault_provider.h"

@implementation CWVTrustedVaultObserver

- (instancetype)initWithTrustedVaultObserver:
    (trusted_vault::TrustedVaultClient::Observer*)observer {
  self = [super init];
  if (self) {
    _observer = observer;
  }
  return self;
}

#pragma mark - Public Methods

- (void)trustedVaultProviderDidChangeKeys:
    (id<CWVTrustedVaultProvider>)provider {
  _observer->OnTrustedVaultKeysChanged();
}

- (void)trustedVaultProviderDidChangeRecoverability:
    (id<CWVTrustedVaultProvider>)provider {
  _observer->OnTrustedVaultRecoverabilityChanged();
}

#pragma mark - NSObject

- (BOOL)isEqual:(CWVTrustedVaultObserver*)otherObserver {
  if (self == otherObserver) {
    return YES;
  }
  if (![otherObserver isKindOfClass:[CWVTrustedVaultObserver class]]) {
    return NO;
  }

  return self.observer == otherObserver.observer;
}

- (NSUInteger)hash {
  return (NSUInteger)self.observer;
}

@end
