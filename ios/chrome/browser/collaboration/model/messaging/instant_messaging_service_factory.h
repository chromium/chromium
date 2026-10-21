// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_FACTORY_H_

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace collaboration::messaging {

// Factory for the instant messaging service.
class InstantMessagingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<InstantMessagingServiceFactory,
                                                InstantMessagingService> {
 public:
  explicit InstantMessagingServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace collaboration::messaging

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_FACTORY_H_
