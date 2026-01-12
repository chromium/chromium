// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client_app_interface.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/webauthn/model/scoped_passkey_keychain_provider_override.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"

namespace {

// Returns a test key.
std::vector<uint8_t> GetTestKey() {
  // Random test key.
  const std::vector<char> key_values = {
      '\x1f', '\xfa', '\x97', '\x98', '\xdf', '\n',   '\xc7', '\xe4',
      '\xf6', 'G',    '\xd5', 'm',    'C',    '\xa2', 'P',    '\xe0',
      '\xa2', 'E',    '\x90', '\xb2', '\x86', '\xbf', '\xfc', 'E',
      '\e',   'N',    '\x15', '\xea', 'G',    '\x9b', '\x9b', '\xc8'};
  return std::vector<uint8_t>(key_values.begin(), key_values.end());
}

class FakePasskeyKeychainProvider : public PasskeyKeychainProvider {
 public:
  FakePasskeyKeychainProvider()
      : PasskeyKeychainProvider(/*metrics_reporting_enabled=*/false) {}
  ~FakePasskeyKeychainProvider() override = default;

  void FetchKeys(NSString* gaia,
                 webauthn::ReauthenticatePurpose purpose,
                 webauthn::KeysFetchedCallback callback) override {
    DCHECK(!callback.is_null());
    std::move(callback).Run({GetTestKey()});
  }
};

}  // namespace

@interface IOSChromePasskeyClientAppInterface () {
  std::unique_ptr<ScopedPasskeyKeychainProviderOverride>
      _scopedPasskeyKeychainProviderOverride;
}

@end

@implementation IOSChromePasskeyClientAppInterface

+ (instancetype)sharedInstance {
  static IOSChromePasskeyClientAppInterface* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[IOSChromePasskeyClientAppInterface alloc] init];
  });
  return instance;
}

+ (std::unique_ptr<ScopedPasskeyKeychainProviderOverride>&)
    scopedPasskeyKeychainProviderOverride {
  return [IOSChromePasskeyClientAppInterface sharedInstance]
      ->_scopedPasskeyKeychainProviderOverride;
}

+ (void)setUpFakePasskeyKeychainProvider {
  [IOSChromePasskeyClientAppInterface scopedPasskeyKeychainProviderOverride] =
      ScopedPasskeyKeychainProviderOverride::MakeAndArmForTesting(
          std::make_unique<FakePasskeyKeychainProvider>());
}

@end
