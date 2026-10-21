// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_

#import "components/password_manager/core/browser/sharing/password_sender_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Creates instances of PasswordSenderService per profile.
class IOSChromePasswordSenderServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromePasswordSenderServiceFactory,
          password_manager::PasswordSenderService> {
 public:
  explicit IOSChromePasswordSenderServiceFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
