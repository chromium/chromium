// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"

#import "components/password_manager/core/browser/password_form_manager.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent_profile_state_observer.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {

password_manager::PasswordFormCache* GetPasswordFormCacheFromWebState(
    web::WebState* web_state) {
  PasswordTabHelper* helper = PasswordTabHelper::FromWebState(web_state);
  if (!helper) {
    return nullptr;
  }

  password_manager::PasswordManager* password_manager =
      helper->GetPasswordManager();
  if (!password_manager) {
    return nullptr;
  }

  return password_manager->GetPasswordFormCache();
}

// Returns whether a user action is required to resolve a password sync error.
// TODO(crbug.com/408165259): Create a generic data type check in sync service
// instead and apply to similar function in recent_tabs_mediator.mm as well.
bool UserActionRequiredToFixPasswordSyncError(ProfileIOS* profile) {
  if (!profile) {
    return false;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }

  // No action should be required if the user (or enterprise policy) has
  // disabled password sync.
  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPasswords)) {
    return false;
  }

  switch (sync_service->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return true;
    case syncer::SyncService::UserActionableError::kNone:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return false;
  }

  NOTREACHED();
}

}  // namespace

SyncErrorBrowserAgent::SyncErrorBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
  profile_state_observer_ = [[SyncErrorBrowserAgentProfileStateObserver alloc]
       initWithProfileState:browser_->GetSceneState().profileState
      syncErrorBrowserAgent:this];
  [profile_state_observer_ start];
}

SyncErrorBrowserAgent::~SyncErrorBrowserAgent() = default;

void SyncErrorBrowserAgent::SetUIProviders(
    id<SigninPresenter> signin_presenter_provider,
    id<SyncPresenter> sync_presenter_provider) {
  DCHECK(signin_presenter_provider);
  DCHECK(sync_presenter_provider);
  signin_presenter_provider_ = signin_presenter_provider;
  sync_presenter_provider_ = sync_presenter_provider;

  // Re-evaluate all web states.
  TriggerInfobarOnAllWebStatesIfNeeded();
}

void SyncErrorBrowserAgent::ClearUIProviders() {
  signin_presenter_provider_ = nil;
  sync_presenter_provider_ = nil;
}

void SyncErrorBrowserAgent::ProfileStateDidUpdateToFinalStage() {
  TriggerInfobarOnAllWebStatesIfNeeded();
  [profile_state_observer_ disconnect];
  profile_state_observer_ = nil;
}

#pragma mark - BrowserObserver

void SyncErrorBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  [profile_state_observer_ disconnect];
  profile_state_observer_ = nil;
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

#pragma mark - WebStateListObserver

void SyncErrorBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web::WebState* detached_web_state = detach_change.detached_web_state();
      if (!detached_web_state->IsRealized()) {
        web_state_observations_.RemoveObservation(detached_web_state);
        RemovePasswordFormManagerObserver(detached_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web::WebState* replaced_web_state = replace_change.replaced_web_state();
      if (!replaced_web_state->IsRealized()) {
        web_state_observations_.RemoveObservation(replaced_web_state);
        RemovePasswordFormManagerObserver(replaced_web_state);
      }
      CreateReSignInInfoBarDelegate(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      web::WebState* inserted_web_state =
          change.As<WebStateListChangeInsert>().inserted_web_state();
      CreateReSignInInfoBarDelegate(inserted_web_state);
      AddPasswordFormManagerObserver(inserted_web_state);
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

void SyncErrorBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  RemovePasswordFormManagerObserver(web_state);
}

void SyncErrorBrowserAgent::WebStateRealized(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  RemovePasswordFormManagerObserver(web_state);
  CreateReSignInInfoBarDelegate(web_state);
}

void SyncErrorBrowserAgent::OnPasswordFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  ProfileIOS* profile = browser_->GetProfile();
  if (active_web_state && active_web_state->IsRealized() &&
      UserActionRequiredToFixPasswordSyncError(profile) &&
      base::FeatureList::IsEnabled(
          syncer::kSyncTrustedVaultInfobarImprovements)) {
    DisplaySyncErrors(profile, active_web_state, sync_presenter_provider_,
                      SyncErrorInfoBarTrigger::kPasswordFormParsed);
  }
}

void SyncErrorBrowserAgent::TriggerInfobarOnAllWebStatesIfNeeded() {
  web_state_observations_.RemoveAllObservations();
  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    CreateReSignInInfoBarDelegate(web_state);
  }
}

void SyncErrorBrowserAgent::CreateReSignInInfoBarDelegate(
    web::WebState* web_state) {
  if (!web_state->IsRealized()) {
    web_state_observations_.AddObservation(web_state);
    return;
  }

  if (!signin_presenter_provider_ || !sync_presenter_provider_) {
    return;
  }

  ProfileState* profile_state = browser_->GetSceneState().profileState;
  if (profile_state.initStage != ProfileInitStage::kFinal) {
    return;
  }

  ProfileIOS* profile = browser_->GetProfile();
  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::Create(
          AuthenticationServiceFactory::GetForProfile(profile),
          IdentityManagerFactory::GetForProfile(profile),
          signin_presenter_provider_);
  if (delegate) {
    InfoBarManagerImpl::FromWebState(web_state)->AddInfoBar(
        CreateConfirmInfoBar(std::move(delegate)));
    return;
  }
  DisplaySyncErrors(profile, web_state, sync_presenter_provider_,
                    SyncErrorInfoBarTrigger::kNewTabOpened);
}

void SyncErrorBrowserAgent::AddPasswordFormManagerObserver(
    web::WebState* web_state) {
  CHECK(web_state);

  password_manager::PasswordFormCache* password_form_cache =
      GetPasswordFormCacheFromWebState(web_state);
  if (password_form_cache) {
    password_form_cache->SetObserver(weak_ptr_factory_.GetWeakPtr());
  }
}

void SyncErrorBrowserAgent::RemovePasswordFormManagerObserver(
    web::WebState* web_state) {
  CHECK(web_state);

  password_manager::PasswordFormCache* password_form_cache =
      GetPasswordFormCacheFromWebState(web_state);
  if (password_form_cache) {
    password_form_cache->ResetObserver();
  }
}
