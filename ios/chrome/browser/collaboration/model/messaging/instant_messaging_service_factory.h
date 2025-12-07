// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace collaboration::messaging {

class InstantMessagingService;

// Factory for the instant messaging service.
class InstantMessagingServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static InstantMessagingService* GetForProfile(ProfileIOS* profile);
  static InstantMessagingServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<InstantMessagingServiceFactory>;

  InstantMessagingServiceFactory();
  ~InstantMessagingServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace collaboration::messaging

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_FACTORY_H_
