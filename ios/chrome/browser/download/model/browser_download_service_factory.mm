// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/browser_download_service_factory.h"

#import "ios/chrome/browser/download/model/browser_download_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/download/download_controller.h"

// static
BrowserDownloadService* BrowserDownloadServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<BrowserDownloadService>(
      profile, /*create=*/true);
}

// static
BrowserDownloadServiceFactory* BrowserDownloadServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserDownloadServiceFactory> instance;
  return instance.get();
}

BrowserDownloadServiceFactory::BrowserDownloadServiceFactory()
    : ProfileKeyedServiceFactoryIOS("BrowserDownloadService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile) {}

BrowserDownloadServiceFactory::~BrowserDownloadServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserDownloadServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  web::DownloadController* download_controller =
      web::DownloadController::FromBrowserState(browser_state);
  return std::make_unique<BrowserDownloadService>(download_controller);
}
