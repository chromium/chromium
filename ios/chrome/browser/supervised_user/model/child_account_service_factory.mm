// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"

#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/supervised_user/core/browser/permission_request_creator_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

namespace {

// Produces a new instance of a PermissionRequestCreator, which
// is used to allow remote approvals through ChildAccountService.
std::unique_ptr<supervised_user::PermissionRequestCreator>
CreatePermissionCreator(ChromeBrowserState* browser_state) {
  std::unique_ptr<supervised_user::PermissionRequestCreator>
      permission_creator =
          std::make_unique<supervised_user::PermissionRequestCreatorImpl>(
              IdentityManagerFactory::GetForBrowserState(browser_state),
              browser_state->GetSharedURLLoaderFactory());
  return permission_creator;
}

}  // namespace

// static
supervised_user::ChildAccountService*
ChildAccountServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<supervised_user::ChildAccountService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
ChildAccountServiceFactory* ChildAccountServiceFactory::GetInstance() {
  static base::NoDestructor<ChildAccountServiceFactory> instance;
  return instance.get();
}

ChildAccountServiceFactory::ChildAccountServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ChildAccountService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SupervisedUserServiceFactory::GetInstance());
  DependsOn(ListFamilyMembersServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ChildAccountServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserState(browser_state);
  CHECK(supervised_user_service);
  PrefService* user_prefs = browser_state->GetPrefs();
  CHECK(user_prefs);
  supervised_user::ListFamilyMembersService* list_family_members_service =
      ListFamilyMembersServiceFactory::GetForBrowserState(browser_state);
  CHECK(list_family_members_service);

  return std::make_unique<supervised_user::ChildAccountService>(
      *user_prefs, *supervised_user_service,
      IdentityManagerFactory::GetForBrowserState(browser_state),
      browser_state->GetSharedURLLoaderFactory(),
      base::BindRepeating(&CreatePermissionCreator, browser_state),
      // Callback relevant only for Chrome OS.
      /*check_user_child_status_callback=*/base::DoNothing(),
      *list_family_members_service);
}
