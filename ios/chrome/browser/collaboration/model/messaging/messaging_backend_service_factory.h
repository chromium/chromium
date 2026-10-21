// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_

#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace collaboration::messaging {

// Factory for the messaging backend service.
class MessagingBackendServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<MessagingBackendServiceFactory,
                                                MessagingBackendService> {
 public:
  explicit MessagingBackendServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace collaboration::messaging

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_
