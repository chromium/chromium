// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_fake.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AuthenticationServiceFake::AuthenticationServiceFake(
    PrefService* pref_service,
    SyncSetupService* sync_setup_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : AuthenticationService(pref_service,
                            sync_setup_service,
                            identity_manager,
                            sync_service),
      have_accounts_changed_while_in_background_(false) {}

AuthenticationServiceFake::~AuthenticationServiceFake() {}

void AuthenticationServiceFake::SignIn(ChromeIdentity* identity) {
  // Needs to call PrepareForFirstSyncSetup to behave like
  // AuthenticationService.
  sync_setup_service_->PrepareForFirstSyncSetup();
  authenticated_identity_ = identity;
}

void AuthenticationServiceFake::SignOut(
    signin_metrics::ProfileSignout signout_source,
    ProceduralBlock completion) {
  authenticated_identity_ = nil;
  if (completion)
    completion();
}

void AuthenticationServiceFake::SetHaveAccountsChangedWhileInBackground(
    bool changed) {
  have_accounts_changed_while_in_background_ = changed;
}

bool AuthenticationServiceFake::HaveAccountsChangedWhileInBackground() const {
  return have_accounts_changed_while_in_background_;
}

bool AuthenticationServiceFake::IsAuthenticated() const {
  return authenticated_identity_ != nil;
}

ChromeIdentity* AuthenticationServiceFake::GetAuthenticatedIdentity() const {
  return authenticated_identity_;
}

std::unique_ptr<KeyedService>
AuthenticationServiceFake::CreateAuthenticationService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  auto service = base::WrapUnique(new AuthenticationServiceFake(
      browser_state->GetPrefs(),
      SyncSetupServiceFactory::GetForBrowserState(browser_state),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      ProfileSyncServiceFactory::GetForBrowserState(browser_state)));
  service->Initialize(std::make_unique<AuthenticationServiceDelegateFake>());
  return service;
}
