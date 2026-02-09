// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/test/ios_chrome_passkey_client_app_interface.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/webauthn/public/scoped_passkey_keychain_provider_override.h"
#import "ios/chrome/browser/webauthn/public/scoped_passkey_reauth_module_override.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"

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
    CHECK(!callback.is_null());
    std::move(callback).Run({GetTestKey()}, /*error=*/nil);
  }

  void CheckEnrolled(NSString* gaia, CheckEnrolledCallback callback) override {
    CHECK(!callback.is_null());
    std::move(callback).Run(YES, /*error=*/nil);
  }

  void CheckDegradedRecoverability(
      NSString* gaia,
      CheckDegradedRecoverabilityCallback callback) override {
    CHECK(!callback.is_null());
    std::move(callback).Run(NO, /*error=*/nil);
  }
};

}  // namespace

@interface IOSChromePasskeyClientAppInterface () {
  std::unique_ptr<ScopedPasskeyKeychainProviderBridgeOverride>
      _scopedPasskeyKeychainProviderBridgeOverride;
  std::unique_ptr<ScopedPasskeyReauthModuleOverride>
      _scopedPasskeyReauthModuleOverride;
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

+ (std::unique_ptr<ScopedPasskeyKeychainProviderBridgeOverride>&)
    scopedPasskeyKeychainProviderBridgeOverride {
  return [IOSChromePasskeyClientAppInterface sharedInstance]
      ->_scopedPasskeyKeychainProviderBridgeOverride;
}

+ (std::unique_ptr<ScopedPasskeyReauthModuleOverride>&)
    scopedPasskeyReauthModuleOverride {
  return [IOSChromePasskeyClientAppInterface sharedInstance]
      ->_scopedPasskeyReauthModuleOverride;
}

+ (void)setUpFakePasskeyKeychainProviderBridge {
  PasskeyKeychainProviderBridge* bridge = [[PasskeyKeychainProviderBridge alloc]
      initWithPasskeyKeychainProvider:std::make_unique<
                                          FakePasskeyKeychainProvider>()];
  [IOSChromePasskeyClientAppInterface
      scopedPasskeyKeychainProviderBridgeOverride] =
      ScopedPasskeyKeychainProviderBridgeOverride::MakeAndArmForTesting(bridge);
}

+ (void)setUpMockReauthenticationModule {
  MockReauthenticationModule* mockModule =
      [[MockReauthenticationModule alloc] init];
  [mockModule setExpectedResult:ReauthenticationResult::kSuccess];
  [IOSChromePasskeyClientAppInterface scopedPasskeyReauthModuleOverride] =
      ScopedPasskeyReauthModuleOverride::MakeAndArmForTesting(mockModule);
}

+ (void)removeMockReauthenticationModule {
  [IOSChromePasskeyClientAppInterface scopedPasskeyReauthModuleOverride] =
      nullptr;
}

+ (void)setMockReauthenticationResult:(ReauthenticationResult)result {
  MockReauthenticationModule* module =
      base::apple::ObjCCastStrict<MockReauthenticationModule>(
          [IOSChromePasskeyClientAppInterface scopedPasskeyReauthModuleOverride]
              ->Get());
  [module setExpectedResult:result];
}

@end
