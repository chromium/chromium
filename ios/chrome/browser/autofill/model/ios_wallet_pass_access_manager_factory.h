// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_WALLET_PASS_ACCESS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_WALLET_PASS_ACCESS_MANAGER_FACTORY_H_

#import <memory>

#import "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all WalletPassAccessManagers and associates them with
// ProfileIOS.
class IOSWalletPassAccessManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSWalletPassAccessManagerFactory,
          autofill::WalletPassAccessManager> {
 public:
  explicit IOSWalletPassAccessManagerFactory(PassKey key);

 private:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_WALLET_PASS_ACCESS_MANAGER_FACTORY_H_
