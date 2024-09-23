// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

namespace {

std::string GetManagedBookmarksDomain(ProfileIOS* profile) {
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!auth_service) {
    return std::string();
  }

  id<SystemIdentity> identity =
      auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity) {
    return std::string();
  }

  return base::SysNSStringToUTF8(
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->GetCachedHostedDomainForIdentity(identity));
}

std::unique_ptr<KeyedService> BuildManagedBookmarkModel(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  // base::Unretained is safe because ManagedBookmarkService will
  // be destroyed before the profile it is attached to.
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      profile->GetPrefs(), base::BindRepeating(&GetManagedBookmarksDomain,
                                               base::Unretained(profile)));
}

}  // namespace

// static
bookmarks::ManagedBookmarkService*
ManagedBookmarkServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
bookmarks::ManagedBookmarkService* ManagedBookmarkServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<bookmarks::ManagedBookmarkService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
ManagedBookmarkServiceFactory* ManagedBookmarkServiceFactory::GetInstance() {
  static base::NoDestructor<ManagedBookmarkServiceFactory> instance;
  return instance.get();
}

// static
ManagedBookmarkServiceFactory::TestingFactory
ManagedBookmarkServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildManagedBookmarkModel);
}

ManagedBookmarkServiceFactory::ManagedBookmarkServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ManagedBookmarkService",
          BrowserStateDependencyManager::GetInstance()) {}

ManagedBookmarkServiceFactory::~ManagedBookmarkServiceFactory() {}

std::unique_ptr<KeyedService>
ManagedBookmarkServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildManagedBookmarkModel(context);
}

bool ManagedBookmarkServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
