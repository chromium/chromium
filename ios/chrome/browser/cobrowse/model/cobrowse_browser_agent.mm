// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"

#import "components/search_engines/util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"

CobrowseBrowserAgent::CobrowseBrowserAgent(Browser* browser)
    : BrowserUserData<CobrowseBrowserAgent>(browser) {
  CHECK(IsAimCobrowseEnabled());
  StartObserving(browser);
}

CobrowseBrowserAgent::~CobrowseBrowserAgent() {
  StopObserving();
}

#pragma mark - CobrowseTabHelper::Delegate

bool CobrowseBrowserAgent::CanShowAssistantForWebState(
    web::WebState* web_state) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  const int index = web_state_list->GetIndexOfWebState(web_state);
  CHECK_NE(index, WebStateList::kInvalidIndex);

  web::WebState* opener = web_state_list->GetOpenerOfWebStateAt(index).opener;
  return opener && IsAimURL(opener->GetLastCommittedURL());
}

#pragma mark - TabsDependencyInstaller

void CobrowseBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);
  tab_helper->SetDelegate(this);
  tab_helper->SetSceneCommandsHandler(
      HandlerForProtocol(browser_->GetCommandDispatcher(), SceneCommands));
}

void CobrowseBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);
  tab_helper->SetDelegate(nullptr);
  tab_helper->SetSceneCommandsHandler(nil);
}

void CobrowseBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // Nothing to do.
}

void CobrowseBrowserAgent::OnActiveWebStateChanged(web::WebState* old_active,
                                                   web::WebState* new_active) {
  // Nothing to do.
}
