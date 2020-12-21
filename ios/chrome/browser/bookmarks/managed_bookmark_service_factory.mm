// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"

#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::string GetManagedBookmarksDomain(ChromeBrowserState* browser_state) {
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  if (!auth_service)
    return std::string();

  ChromeIdentity* identity = auth_service->GetAuthenticatedIdentity();
  if (!identity)
    return std::string();

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  if (!identity_service)
    return std::string();

  return base::SysNSStringToUTF8(
      identity_service->GetCachedHostedDomainForIdentity(identity));
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

ManagedBookmarkServiceFactory::ManagedBookmarkServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ManagedBookmarkService",
          BrowserStateDependencyManager::GetInstance()) {}

ManagedBookmarkServiceFactory::~ManagedBookmarkServiceFactory() {}

std::unique_ptr<KeyedService>
ManagedBookmarkServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  // base::Unretained is safe because ManagedBookmarkService will
  // be destroyed before the browser_state it is attached to.
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      browser_state->GetPrefs(),
      base::BindRepeating(&GetManagedBookmarksDomain,
                          base::Unretained(browser_state)));
}

bool ManagedBookmarkServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
