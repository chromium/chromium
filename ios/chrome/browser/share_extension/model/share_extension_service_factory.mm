// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ShareExtensionService* ShareExtensionServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<ShareExtensionService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
ShareExtensionService* ShareExtensionServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<ShareExtensionService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
ShareExtensionServiceFactory* ShareExtensionServiceFactory::GetInstance() {
  static base::NoDestructor<ShareExtensionServiceFactory> instance;
  return instance.get();
}

ShareExtensionServiceFactory::ShareExtensionServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ShareExtensionService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

ShareExtensionServiceFactory::~ShareExtensionServiceFactory() {}

std::unique_ptr<KeyedService>
ShareExtensionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForProfile(profile);

  return std::make_unique<ShareExtensionService>(
      bookmark_model, ReadingListModelFactory::GetForProfile(profile));
}

web::BrowserState* ShareExtensionServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
