// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_H_

#import "components/password_manager/core/browser/password_form_cache.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

class Browser;
@protocol ReSigninPresenter;
@class SyncErrorBrowserAgentProfileStateObserver;
@protocol SyncPresenter;

namespace password_manager {
class PasswordFormManager;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

// Browser agent that is responsible for displaying sync errors.
class SyncErrorBrowserAgent
    : public BrowserUserData<SyncErrorBrowserAgent>,
      public password_manager::PasswordFormManagerObserver,
      public TabsDependencyInstaller {
 public:
  SyncErrorBrowserAgent(const SyncErrorBrowserAgent&) = delete;
  SyncErrorBrowserAgent& operator=(const SyncErrorBrowserAgent&) = delete;

  ~SyncErrorBrowserAgent() override;

  // Sets the UI providers to present sign in and sync UI when needed.
  void SetUIProviders(id<ReSigninPresenter> signin_presenter_provider,
                      id<SyncPresenter> sync_presenter_provider);

  // Clears the UI providers.
  void ClearUIProviders();

  // Called when the profile state was updated to final stage.
  void ProfileStateDidUpdateToFinalStage();

  // TabsDependencyInstaller implementation:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<SyncErrorBrowserAgent>;

  explicit SyncErrorBrowserAgent(Browser* browser);

  // password_manager::PasswordFormManagerObserver methods
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  // Helper method.
  void CreateReSignInInfoBarDelegate(web::WebState* web_state);

  // Triggers Infobar on all web states, if needed.
  void TriggerInfobarOnAllWebStatesIfNeeded();

  // Provider to a SignIn presenter
  __weak id<ReSigninPresenter> resignin_presenter_provider_;
  // Provider to a Sync presenter
  __weak id<SyncPresenter> sync_presenter_provider_;
  // Used to observe the ProfileState.
  __strong SyncErrorBrowserAgentProfileStateObserver* profile_state_observer_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_H_
