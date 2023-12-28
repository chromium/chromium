// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_configuration.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/public/provider/chrome/browser/drive/drive_api.h"

namespace drive {

// static
DriveService* DriveServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<DriveService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
DriveServiceFactory* DriveServiceFactory::GetInstance() {
  static base::NoDestructor<DriveServiceFactory> instance;
  return instance.get();
}

DriveServiceFactory::DriveServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DriveService",
          BrowserStateDependencyManager::GetInstance()) {}

DriveServiceFactory::~DriveServiceFactory() = default;

std::unique_ptr<KeyedService> DriveServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  drive::DriveServiceConfiguration configuration{};
  return ios::provider::CreateDriveService(configuration);
}

web::BrowserState* DriveServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace drive
