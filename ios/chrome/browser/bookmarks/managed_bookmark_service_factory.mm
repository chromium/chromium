// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"

namespace {

std::string GetManagedBookmarksDomain(ChromeBrowserState* browser_state) {
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  if (!auth_service)
    return std::string();

  id<SystemIdentity> identity =
      auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity)
    return std::string();

  return base::SysNSStringToUTF8(
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->GetCachedHostedDomainForIdentity(identity));
}

std::unique_ptr<KeyedService> BuildManagedBookmarkModel(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  // base::Unretained is safe because ManagedBookmarkService will
  // be destroyed before the browser_state it is attached to.
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      browser_state->GetPrefs(),
      base::BindRepeating(&GetManagedBookmarksDomain,
                          base::Unretained(browser_state)));
}

}  // namespace

// static
bookmarks::ManagedBookmarkService*
ManagedBookmarkServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::ManagedBookmarkService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
