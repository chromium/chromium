// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_

#include <memory>

#include "components/sync_user_events/user_event_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that associates UserEventServices to ProfileIOS.
class IOSUserEventServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<IOSUserEventServiceFactory,
                                                syncer::UserEventService> {
 public:
  explicit IOSUserEventServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_
