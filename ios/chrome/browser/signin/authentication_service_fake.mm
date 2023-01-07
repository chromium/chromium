// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_fake.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AuthenticationServiceFake::AuthenticationServiceFake(
    PrefService* pref_service,
    SyncSetupService* sync_setup_service,
    ChromeAccountManagerService* account_manager_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : AuthenticationService(pref_service,
                            sync_setup_service,
                            account_manager_service,
                            identity_manager,
                            sync_service) {}

AuthenticationServiceFake::~AuthenticationServiceFake() {}

void AuthenticationServiceFake::SignIn(id<SystemIdentity> identity) {
  // Needs to call PrepareForFirstSyncSetup to behave like
  // AuthenticationService.
  DCHECK(identity);
  sync_setup_service_->PrepareForFirstSyncSetup();
  primary_identity_ = identity;
  consent_level_ = signin::ConsentLevel::kSignin;
}

void AuthenticationServiceFake::GrantSyncConsent(id<SystemIdentity> identity) {
  consent_level_ = signin::ConsentLevel::kSync;
}

void AuthenticationServiceFake::SignOut(
    signin_metrics::ProfileSignout signout_source,
    bool force_clear_browsing_data,
    ProceduralBlock completion) {
  if (force_clear_browsing_data ||
      HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AuthenticationServiceFake::SignOutInternal,
                                  weak_factory_.GetWeakPtr(), completion));
  } else {
    SignOutInternal(completion);
  }
}

void AuthenticationServiceFake::SignOutInternal(ProceduralBlock completion) {
  primary_identity_ = nil;
  consent_level_ = signin::ConsentLevel::kSignin;
  if (completion)
    completion();
}

ChromeIdentity* AuthenticationServiceFake::GetPrimaryIdentity(
    signin::ConsentLevel consent_level) const {
  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      return base::mac::ObjCCastStrict<ChromeIdentity>(primary_identity_);
    case signin::ConsentLevel::kSync:
      return (consent_level_ == signin::ConsentLevel::kSync) ? primary_identity_
                                                             : nil;
  }
  return nil;
}

bool AuthenticationServiceFake::HasPrimaryIdentityManaged(
    signin::ConsentLevel consent_level) const {
  if (!GetPrimaryIdentity(consent_level)) {
    return false;
  }
  return [ios::GetManagedEmailSuffixes()
             indexOfObjectPassingTest:^BOOL(NSString* suffix, NSUInteger idx,
                                            BOOL* stop) {
               return [primary_identity_.userEmail hasSuffix:suffix];
             }] != NSNotFound;
}

std::unique_ptr<KeyedService>
AuthenticationServiceFake::CreateAuthenticationService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  auto service = base::WrapUnique(new AuthenticationServiceFake(
      browser_state->GetPrefs(),
      SyncSetupServiceFactory::GetForBrowserState(browser_state),
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      SyncServiceFactory::GetForBrowserState(browser_state)));
  service->Initialize(std::make_unique<AuthenticationServiceDelegateFake>());
  return service;
}
