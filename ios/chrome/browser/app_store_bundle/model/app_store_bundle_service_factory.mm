// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/app_store_bundle/app_store_bundle_api.h"

// static
AppStoreBundleService* AppStoreBundleServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<AppStoreBundleService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
AppStoreBundleServiceFactory* AppStoreBundleServiceFactory::GetInstance() {
  static base::NoDestructor<AppStoreBundleServiceFactory> instance;
  return instance.get();
}

AppStoreBundleServiceFactory::AppStoreBundleServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AppStoreBundleService",
                                    ProfileSelection::kRedirectedInIncognito) {}

AppStoreBundleServiceFactory::~AppStoreBundleServiceFactory() = default;

std::unique_ptr<KeyedService>
AppStoreBundleServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  // TODO(crbug.com/440148910): return
  // `ios::provider::CreateAppStoreBundleService()`.
  return nullptr;
}
