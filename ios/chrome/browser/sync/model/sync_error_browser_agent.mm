// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent_app_state_observer.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"

BROWSER_USER_DATA_KEY_IMPL(SyncErrorBrowserAgent)

SyncErrorBrowserAgent::SyncErrorBrowserAgent(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
  SyncErrorBrowserAgentAppStateObserver* observer =
      [[SyncErrorBrowserAgentAppStateObserver alloc]
          initWithSyncErrorBrowserAgent:this];
  [browser_->GetSceneState().appState addObserver:observer];
  app_state_observer_ = observer;
}

SyncErrorBrowserAgent::~SyncErrorBrowserAgent() {
  DCHECK(!browser_);
}

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

void SyncErrorBrowserAgent::AppStateDidUpdateToFinalStage() {
  TriggerInfobarOnAllWebStatesIfNeeded();
}

#pragma mark - BrowserObserver

void SyncErrorBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  [browser_->GetSceneState().appState removeObserver:app_state_observer_];
  [app_state_observer_ disconnect];
  app_state_observer_ = nil;
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  browser_ = nullptr;
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
      }
      CreateReSignInInfoBarDelegate(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      CreateReSignInInfoBarDelegate(insert_change.inserted_web_state());
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
}

void SyncErrorBrowserAgent::WebStateRealized(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  CreateReSignInInfoBarDelegate(web_state);
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

  ProfileIOS* profile = browser_->GetProfile();
  AppState* app_state = browser_->GetSceneState().appState;

  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::Create(
          AuthenticationServiceFactory::GetForProfile(profile),
          IdentityManagerFactory::GetForProfile(profile), app_state,
          signin_presenter_provider_);
  if (delegate) {
    InfoBarManagerImpl::FromWebState(web_state)->AddInfoBar(
        CreateConfirmInfoBar(std::move(delegate)));
    return;
  }
  DisplaySyncErrors(profile, web_state, sync_presenter_provider_);
}
