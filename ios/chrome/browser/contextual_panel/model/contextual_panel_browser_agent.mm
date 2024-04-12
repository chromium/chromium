// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

BROWSER_USER_DATA_KEY_IMPL(ContextualPanelBrowserAgent)

ContextualPanelBrowserAgent::ContextualPanelBrowserAgent(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_.get());
  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

ContextualPanelBrowserAgent::~ContextualPanelBrowserAgent() {
  web_state_list_observation_.Reset();
  contextual_panel_tab_helper_observation_.Reset();
  browser_ = nullptr;
}

bool ContextualPanelBrowserAgent::
    IsEntrypointConfigurationAvailableForCurrentTab() {
  if (!contextual_panel_tab_helper_observation_.IsObserving()) {
    return false;
  }

  return contextual_panel_tab_helper_observation_.GetSource()
      ->HasCachedConfigsAvailable();
}

base::WeakPtr<ContextualPanelItemConfiguration>
ContextualPanelBrowserAgent::GetEntrypointConfigurationForCurrentTab() {
  if (!contextual_panel_tab_helper_observation_.IsObserving()) {
    return nullptr;
  }

  return contextual_panel_tab_helper_observation_.GetSource()
      ->GetFirstCachedConfig();
}

bool ContextualPanelBrowserAgent::WasLargeEntrypointShownForCurrentTab() {
  if (!contextual_panel_tab_helper_observation_.IsObserving()) {
    return true;
  }

  return contextual_panel_tab_helper_observation_.GetSource()
      ->WasLargeEntrypointShown();
}

void ContextualPanelBrowserAgent::SetLargeEntrypointShownForCurrentTab(
    bool shown) {
  if (!contextual_panel_tab_helper_observation_.IsObserving()) {
    return;
  }

  contextual_panel_tab_helper_observation_.GetSource()->SetLargeEntrypointShown(
      shown);
}

#pragma mark - WebStateListObserver

void ContextualPanelBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  // Return early if the active web state is the same as before the change.
  if (!status.active_web_state_change()) {
    return;
  }

  contextual_panel_tab_helper_observation_.Reset();

  // Return early if no new webstates are active.
  if (!status.new_active_web_state) {
    return;
  }

  contextual_panel_tab_helper_observation_.Observe(
      ContextualPanelTabHelper::FromWebState(status.new_active_web_state));

  id<ContextualPanelEntrypointCommands> contextual_panel_entrypoint_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(),
                         ContextualPanelEntrypointCommands);

  [contextual_panel_entrypoint_handler
      updateContextualPanelEntrypointForNewModelData];
}

void ContextualPanelBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  web_state_list_observation_.Reset();
  contextual_panel_tab_helper_observation_.Reset();
  browser_ = nullptr;
}

#pragma mark - ContextualPanelTabHelperObserver

void ContextualPanelBrowserAgent::ContextualPanelHasNewData(
    ContextualPanelTabHelper* tab_helper,
    std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
        item_configurations) {
  id<ContextualPanelEntrypointCommands> contextual_panel_entrypoint_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(),
                         ContextualPanelEntrypointCommands);

  [contextual_panel_entrypoint_handler
      updateContextualPanelEntrypointForNewModelData];
}

void ContextualPanelBrowserAgent::ContextualPanelTabHelperDestroyed(
    ContextualPanelTabHelper* tab_helper) {
  contextual_panel_tab_helper_observation_.Reset();

  id<ContextualPanelEntrypointCommands> contextual_panel_entrypoint_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(),
                         ContextualPanelEntrypointCommands);

  [contextual_panel_entrypoint_handler
      updateContextualPanelEntrypointForNewModelData];
}
