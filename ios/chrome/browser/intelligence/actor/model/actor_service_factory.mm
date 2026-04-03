// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace actor {

// static
ActorService* ActorServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ActorService>(profile,
                                                             /*create=*/true);
}

// static
ActorServiceFactory* ActorServiceFactory::GetInstance() {
  static base::NoDestructor<ActorServiceFactory> instance;
  return instance.get();
}

ActorServiceFactory::ActorServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ActorService",
                                    ProfileSelection::kNoInstanceInIncognito) {}

ActorServiceFactory::~ActorServiceFactory() {}

std::unique_ptr<KeyedService> ActorServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!IsActorEnabled()) {
    return nullptr;
  }
  return std::make_unique<ActorService>(profile);
}

}  // namespace actor
