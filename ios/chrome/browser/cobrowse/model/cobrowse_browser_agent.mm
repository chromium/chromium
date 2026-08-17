// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
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
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
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

bool CobrowseBrowserAgent::ShouldAcceptContextUpdate(
    CobrowseContext* context) const {
  // Nil/invalid contexts are always accepted (wipes the session).
  if (!context || !context.url.is_valid()) {
    return true;
  }

  // Reject contexts that are not AIM URLs.
  if (!IsAimURL(context.url) && !IsAimZeroStateURL(context.url)) {
    return false;
  }

  // Allow contexts that have an explicit text query, or lack a query entirely.
  // Only reject if the query is explicitly empty (e.g. `q=&`).
  if (!context.searchQuery || context.searchQuery.length > 0) {
    return true;
  }

  // Allow contexts that have attached items in memory.
  if (context.attachedItems.count > 0) {
    return true;
  }

  // Reject buggy fallback navigations (e.g., tapping a chip incorrectly resets
  // the URL to an empty query). This bug is identified if the new context has
  // an empty query and no attachments, but the current context had a valid
  // query.
  if ((!context.searchQuery || context.searchQuery.length == 0) &&
      context.attachedItems.count == 0 && context_ &&
      context_.searchQuery.length > 0) {
    return false;
  }

  // Allow contexts that contain valid server session tokens.
  if (context.hasServerSessionTokens) {
    return true;
  }

  // Reject all other empty queries without attachments or tokens.
  return false;
}

void CobrowseBrowserAgent::SetCobrowseContext(CobrowseContext* context) {
  if (!ShouldAcceptContextUpdate(context)) {
    return;
  }

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
  // displayed. Also check if the Start Surface is visible.
  if (ShouldHideAssistantForWebState(web_state)) {
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
    GURL opener_url = opener->GetLastCommittedURL();
    if (IsAimURL(opener_url) || IsAimZeroStateURL(opener_url)) {
      CobrowseContext* new_context =
          [[CobrowseContext alloc] initWithURL:opener_url];

      // If the opener's current URL is empty (e.g., q=& without attachments),
      // it will be rejected. This happens due to a bug when opening a chip with
      // multiple links: the URL is incorrectly reset to an empty one. In this
      // case, it is filtered out and traverse the history to keep the parent's
      // actual contextual URL.
      if (!ShouldAcceptContextUpdate(new_context)) {
        web::NavigationManager* nav_manager = opener->GetNavigationManager();
        int last_index = nav_manager->GetLastCommittedItemIndex();
        if (last_index > 0) {
          web::NavigationItem* prev_item =
              nav_manager->GetItemAtIndex(last_index - 1);
          if (prev_item) {
            CobrowseContext* prev_context =
                [[CobrowseContext alloc] initWithURL:prev_item->GetURL()];
            if (ShouldAcceptContextUpdate(prev_context)) {
              new_context = prev_context;
            }
          }
        }
      }

      SetCobrowseContext(new_context);
    }
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

void CobrowseBrowserAgent::TerminateSession() {
  if (is_session_active_) {
    id<SceneCommands> scene_commands_handler =
        HandlerForProtocol(browser_->GetCommandDispatcher(), SceneCommands);
    [scene_commands_handler hideAssistant];
    SetSessionActive(false);
  }
}

void CobrowseBrowserAgent::OnEligibilityChanged() {
  if (!IsAimCobrowseEligible(browser_->GetProfile())) {
    TerminateSession();
  }
}

bool CobrowseBrowserAgent::ShouldHideAssistantForWebState(
    web::WebState* web_state) {
  if (ui_state_provider_ &&
      ui_state_provider_->IsAssistantHiddenByUIState(web_state)) {
    return true;
  }
  return false;
}

bool CobrowseBrowserAgent::IsWebStateActive(web::WebState* web_state) {
  if (!browser_ || !browser_->GetWebStateList()) {
    return false;
  }
  return browser_->GetWebStateList()->GetActiveWebState() == web_state;
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
