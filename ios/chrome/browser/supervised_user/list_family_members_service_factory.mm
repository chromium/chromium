// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/list_family_members_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/supervised_user/core/browser/list_family_members_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"

// static
supervised_user::ListFamilyMembersService*
ListFamilyMembersServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<supervised_user::ListFamilyMembersService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
ListFamilyMembersServiceFactory*
ListFamilyMembersServiceFactory::GetInstance() {
  static base::NoDestructor<ListFamilyMembersServiceFactory> instance;
  return instance.get();
}

ListFamilyMembersServiceFactory::ListFamilyMembersServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ListFamilyMembersService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ListFamilyMembersServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  return std::make_unique<supervised_user::ListFamilyMembersService>(
      IdentityManagerFactory::GetForBrowserState(browser_state),
      browser_state->GetSharedURLLoaderFactory());
}
