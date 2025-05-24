// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ShareExtensionService* ShareExtensionServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ShareExtensionService>(
      profile, /*create=*/true);
}

// static
ShareExtensionServiceFactory* ShareExtensionServiceFactory::GetInstance() {
  static base::NoDestructor<ShareExtensionServiceFactory> instance;
  return instance.get();
}

ShareExtensionServiceFactory::ShareExtensionServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ShareExtensionService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

ShareExtensionServiceFactory::~ShareExtensionServiceFactory() {}

std::unique_ptr<KeyedService>
ShareExtensionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  return std::make_unique<ShareExtensionService>(
      ios::BookmarkModelFactory::GetForProfile(profile),
      ReadingListModelFactory::GetForProfile(profile));
}
