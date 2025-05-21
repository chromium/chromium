// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/model/glic_service_factory.h"

#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/gemini/model/glic_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/glic/glic_api.h"

class GlicService;

namespace {

std::unique_ptr<KeyedService> BuildGlicService(web::BrowserState* context) {
  if (!IsPageActionMenuEnabled()) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<GlicService>(
      AuthenticationServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
GlicService* GlicServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<GlicService>(profile,
                                                            /*create=*/true);
}

// static
GlicServiceFactory* GlicServiceFactory::GetInstance() {
  static base::NoDestructor<GlicServiceFactory> instance;
  return instance.get();
}

GlicServiceFactory::GlicServiceFactory()
    : ProfileKeyedServiceFactoryIOS("GlicService") {
  DependsOn(AuthenticationServiceFactory::GetInstance());
}

GlicServiceFactory::~GlicServiceFactory() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
GlicServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildGlicService);
}

std::unique_ptr<KeyedService> GlicServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildGlicService(context);
}
