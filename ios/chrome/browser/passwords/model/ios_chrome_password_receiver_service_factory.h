// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_RECEIVER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_RECEIVER_SERVICE_FACTORY_H_

#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Creates instances of PasswordReceiverService per profile.
class IOSChromePasswordReceiverServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromePasswordReceiverServiceFactory,
          password_manager::PasswordReceiverService> {
 public:
  explicit IOSChromePasswordReceiverServiceFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_RECEIVER_SERVICE_FACTORY_H_
