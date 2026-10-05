// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"

#import "components/dom_distiller/core/distiller.h"
#import "components/dom_distiller/core/distiller_options.h"
#import "ios/chrome/browser/dom_distiller/model/constants.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
DistillerService* DistillerServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DistillerService>(
      profile, /*create=*/true);
}

// static
DistillerServiceFactory* DistillerServiceFactory::GetInstance() {
  static base::NoDestructor<DistillerServiceFactory> instance;
  return instance.get();
}

DistillerServiceFactory::DistillerServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DistillerService",
                                    ProfileSelection::kRedirectedInIncognito) {}

DistillerServiceFactory::~DistillerServiceFactory() {}

std::unique_ptr<KeyedService> DistillerServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  dom_distiller::DistillerOptions options;
  options.readability.allowed_video_regex = kReadabilityAllowedVideoRegex;
  return std::make_unique<DistillerService>(
      std::make_unique<dom_distiller::DistillerFactoryImpl>(std::move(options)),
      profile->GetPrefs());
}
