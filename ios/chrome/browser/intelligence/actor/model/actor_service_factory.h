// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_FACTORY_H_

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace actor {

// Singleton that owns all ActorServices and associates them with
// ProfileIOS.
class ActorServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ActorServiceFactory,
                                                ActorService> {
 public:
  explicit ActorServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_FACTORY_H_
