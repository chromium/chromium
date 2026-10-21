// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/browser_download_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/download/model/browser_download_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/download/download_controller.h"

namespace {

// Default factory.
std::unique_ptr<KeyedService> BuildBrowserDownloadService(ProfileIOS* profile) {
  return std::make_unique<BrowserDownloadService>(
      web::DownloadController::FromBrowserState(profile));
}

}  // namespace

// static
BrowserDownloadServiceFactory::TestingFactory
BrowserDownloadServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildBrowserDownloadService);
}

BrowserDownloadServiceFactory::BrowserDownloadServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "BrowserDownloadService",
          ProfileSelection::kOwnInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService>
BrowserDownloadServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildBrowserDownloadService(profile);
}
