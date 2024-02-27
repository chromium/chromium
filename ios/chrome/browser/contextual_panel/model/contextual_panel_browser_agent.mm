// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_browser_agent.h"

#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_commands.h"

BROWSER_USER_DATA_KEY_IMPL(ContextualPanelBrowserAgent)

ContextualPanelBrowserAgent::ContextualPanelBrowserAgent(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_.get());
  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

ContextualPanelBrowserAgent::~ContextualPanelBrowserAgent() {
  web_state_list_observation_.Reset();
}

#pragma mark - WebStateListObserver

void ContextualPanelBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  // Update the entrypoint's badge status.
}

void ContextualPanelBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  web_state_list_observation_.Reset();
  browser_ = nullptr;
}
