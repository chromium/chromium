// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_trusted_vault_provider.h"

@implementation ShellTrustedVaultProvider

- (instancetype)initWithAuthService:(ShellAuthService*)authService {
  return [super init];
}

- (void)showFetchKeysFlowForIdentity:(CWVIdentity*)identity
                  fromViewController:(UIViewController*)viewController {
  // No op.
}

- (void)showFixDegradedRecoverabilityFlowForIdentity:(CWVIdentity*)identity
                                  fromViewController:
                                      (UIViewController*)viewController {
  // No op.
}

#pragma mark - CWVTrustedVaultProvider

- (void)addTrustedVaultObserver:(CWVTrustedVaultObserver*)observer {
  // No op.
}

- (void)removeTrustedVaultObserver:(CWVTrustedVaultObserver*)observer {
  // No op.
}

- (void)fetchKeysForIdentity:(CWVIdentity*)identity
                  completion:(void (^)(NSArray<NSData*>* _Nullable,
                                       NSError* _Nullable))completion {
  NSError* error = [NSError errorWithDomain:@"org.chromium.ios-web-view-shell"
                                       code:-1
                                   userInfo:nil];
  completion(nil, error);
}

- (void)markLocalKeysAsStaleForIdentity:(CWVIdentity*)identity
                             completion:
                                 (void (^)(NSError* _Nullable))completion {
  NSError* error = [NSError errorWithDomain:@"org.chromium.ios-web-view-shell"
                                       code:-1
                                   userInfo:nil];
  completion(error);
}

- (void)isRecoverabilityDegradedForIdentity:(CWVIdentity*)identity
                                 completion:(void (^)(BOOL, NSError* _Nullable))
                                                completion {
  NSError* error = [NSError errorWithDomain:@"org.chromium.ios-web-view-shell"
                                       code:-1
                                   userInfo:nil];
  completion(NO, error);
}

- (void)clearLocalDataForForIdentity:(CWVIdentity*)identity {
  // No op.
}

@end
