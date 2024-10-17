// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_provider_bridge.h"

#import "base/functional/callback.h"

namespace {

// Returns the security domain secret from the vault keys.
NSData* GetSecurityDomainSecret(
    const PasskeyKeychainProvider::SharedKeyList& keyList) {
  if (keyList.empty()) {
    return nil;
  }
  // TODO(crbug.com/355041765): Do we need to handle multiple keys?
  return [NSData dataWithBytes:keyList[0].data() length:keyList[0].size()];
}

}  // namespace

@implementation PasskeyKeychainProviderBridge {
  std::unique_ptr<PasskeyKeychainProvider> _passkeyKeychainProvider;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _passkeyKeychainProvider = std::make_unique<PasskeyKeychainProvider>();
  }
  return self;
}

- (void)dealloc {
  if (_passkeyKeychainProvider) {
    _passkeyKeychainProvider.reset();
  }
}

- (void)fetchSecurityDomainSecretForGaia:(NSString*)gaia
                    navigationController:
                        (UINavigationController*)navigationController
                                 purpose:(PasskeyKeychainProvider::
                                              ReauthenticatePurpose)purpose
                           enableLogging:(BOOL)enableLogging
                              completion:(FetchKeyCompletionBlock)completion {
  _passkeyKeychainProvider->FetchKeys(
      gaia, navigationController, purpose,
      base::BindOnce(^(const PasskeyKeychainProvider::SharedKeyList& keyList) {
        completion(GetSecurityDomainSecret(keyList));
      }));
}

- (void)markKeysAsStaleForGaia:(NSString*)gaia
                    completion:(ProceduralBlock)completion {
  _passkeyKeychainProvider->MarkKeysAsStale(gaia, base::BindOnce(^() {
                                              completion();
                                            }));
}

@end
