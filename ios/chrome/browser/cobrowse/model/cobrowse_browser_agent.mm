// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"

CobrowseBrowserAgent::CobrowseBrowserAgent(Browser* browser)
    : BrowserUserData<CobrowseBrowserAgent>(browser) {
  CHECK(IsAimCobrowseEnabled());
  StartObserving(browser);

  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(
          browser_->GetProfile());
  if (aim_eligibility_service) {
    eligibility_subscription_ =
        aim_eligibility_service->RegisterEligibilityChangedCallback(
            base::BindRepeating(&CobrowseBrowserAgent::OnEligibilityChanged,
                                base::Unretained(this)));
  }

  SceneState* scene_state = browser_->GetSceneState();
  if (scene_state && !scene_state.sceneSessionID.empty()) {
    const auto& map = browser_->GetProfile()->GetPrefs()->GetDict(
        prefs::kCobrowseSessionActiveMap);
    is_session_active_ = map.FindString(scene_state.sceneSessionID) != nullptr;
    if (is_session_active_ && !IsAimCobrowseEligible(browser_->GetProfile())) {
      SetSessionActive(false);
    }
  }
}

CobrowseBrowserAgent::~CobrowseBrowserAgent() {
  StopObserving();
}

CobrowseContext* CobrowseBrowserAgent::GetCobrowseContext() {
  return context_;
}

void CobrowseBrowserAgent::SetCobrowseContext(CobrowseContext* context) {
  context_ = context;
  if (is_session_active_) {
    SceneState* scene_state = browser_->GetSceneState();
    if (scene_state && !scene_state.sceneSessionID.empty()) {
      ScopedDictPrefUpdate update(browser_->GetProfile()->GetPrefs(),
                                  prefs::kCobrowseSessionActiveMap);
      std::string server_id = "";
      if (context_) {
        server_id = base::SysNSStringToUTF8(context_.serverID);
      }
      update->Set(scene_state.sceneSessionID, server_id);
      browser_->GetProfile()->GetPrefs()->CommitPendingWrite();
    }
  }
}

void CobrowseBrowserAgent::SetUIStateProvider(UIStateProvider* provider) {
  ui_state_provider_ = provider;
}

#pragma mark - CobrowseTabHelper::Delegate

bool CobrowseBrowserAgent::CanShowAssistantForWebState(
    web::WebState* web_state) {
  if (!IsAimCobrowseEligible(browser_->GetProfile())) {
    return false;
  }
  // A WebState is loaded when it becomes the active WebState while the Tab
  // Grid is visible, which triggers DidStartNavigation. To avoid UI conflicts
  // or crashes, do not show the assistant if the Tab Grid is currently
  // displayed.
  if (ui_state_provider_ && ui_state_provider_->IsTabGridVisible()) {
    return false;
  }

  if (IsSessionActive()) {
    return true;
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int index = web_state_list->GetIndexOfWebState(web_state);
  CHECK_NE(index, WebStateList::kInvalidIndex);

  web::WebState* opener = web_state_list->GetOpenerOfWebStateAt(index).opener;
  return opener && opener->IsRealized() &&
         IsAimURL(opener->GetLastCommittedURL());
}

void CobrowseBrowserAgent::ConfigureAssistantContextForWebState(
    web::WebState* web_state) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  const int index = web_state_list->GetIndexOfWebState(web_state);
  if (index == WebStateList::kInvalidIndex) {
    return;
  }
  web::WebState* opener = web_state_list->GetOpenerOfWebStateAt(index).opener;
  if (opener) {
    SetCobrowseContext(
        [[CobrowseContext alloc] initWithURL:opener->GetLastCommittedURL()]);
  }
}

bool CobrowseBrowserAgent::IsSessionActive() {
  return is_session_active_;
}

void CobrowseBrowserAgent::SetSessionActive(bool active) {
  is_session_active_ = active;
  SceneState* scene_state = browser_->GetSceneState();
  if (scene_state && !scene_state.sceneSessionID.empty()) {
    ScopedDictPrefUpdate update(browser_->GetProfile()->GetPrefs(),
                                prefs::kCobrowseSessionActiveMap);
    if (active) {
      std::string server_id = "";
      if (context_) {
        server_id = base::SysNSStringToUTF8(context_.serverID);
      }
      update->Set(scene_state.sceneSessionID, server_id);
    } else {
      update->Remove(scene_state.sceneSessionID);
    }
    browser_->GetProfile()->GetPrefs()->CommitPendingWrite();
  }
}

void CobrowseBrowserAgent::OnEligibilityChanged() {
  if (!IsAimCobrowseEligible(browser_->GetProfile())) {
    if (is_session_active_) {
      id<SceneCommands> scene_commands_handler =
          HandlerForProtocol(browser_->GetCommandDispatcher(), SceneCommands);
      [scene_commands_handler hideAssistant];
      SetSessionActive(false);
    }
  }
}

bool CobrowseBrowserAgent::IsTabGridVisible() {
  return ui_state_provider_ && ui_state_provider_->IsTabGridVisible();
}

#pragma mark - TabsDependencyInstaller

void CobrowseBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);
  if (tab_helper) {
    tab_helper->SetDelegate(this);
    tab_helper->SetSceneCommandsHandler(
        HandlerForProtocol(browser_->GetCommandDispatcher(), SceneCommands));
  }
}

void CobrowseBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);
  if (tab_helper) {
    tab_helper->SetDelegate(nullptr);
    tab_helper->SetSceneCommandsHandler(nil);
  }
}

void CobrowseBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // Nothing to do.
}

void CobrowseBrowserAgent::OnActiveWebStateChanged(web::WebState* old_active,
                                                   web::WebState* new_active) {
  // Nothing to do.
}
