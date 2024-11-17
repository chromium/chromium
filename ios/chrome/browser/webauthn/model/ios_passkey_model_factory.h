// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_PASSKEY_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_PASSKEY_MODEL_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Singleton that associates PasskeyModel to Profiles.
class IOSPasskeyModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  static webauthn::PasskeyModel* GetForProfile(ProfileIOS* profile);

  static IOSPasskeyModelFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSPasskeyModelFactory>;

  IOSPasskeyModelFactory();
  ~IOSPasskeyModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_PASSKEY_MODEL_FACTORY_H_
