// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_PASSKEY_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_PASSKEY_MODEL_FACTORY_H_

#import <memory>

#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that associates PasskeyModel to Profiles.
class IOSPasskeyModelFactory final
    : public TypedProfileKeyedServiceFactoryIOS<IOSPasskeyModelFactory,
                                                webauthn::PasskeyModel> {
 public:
  explicit IOSPasskeyModelFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_PASSKEY_MODEL_FACTORY_H_
