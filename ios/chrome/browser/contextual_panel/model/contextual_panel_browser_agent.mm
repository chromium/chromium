// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_commands.h"
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
}

ContextualPanelItemConfiguration
ContextualPanelBrowserAgent::GetEntrypointConfiguration() {
  CHECK(IsContextualPanelForceShowEntrypointEnabled());

  // Only pass a test config when force showing the entrypoint.
  // TODO(crbug.com/327181130) cleanup when appropriate to do so.
  ContextualPanelItemConfiguration config;
  config.entrypoint_image_name = base::SysNSStringToUTF8(kMapSymbol);
  config.image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
  return config;
}

#pragma mark - WebStateListObserver

void ContextualPanelBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  // Return early if the active web state is the same as before the change, or
  // if there is no new webstate (last tab closed).
  if (!status.active_web_state_change() || !status.new_active_web_state) {
    return;
  }

  if (IsContextualPanelEnabled() &&
      IsContextualPanelForceShowEntrypointEnabled()) {
    id<ContextualPanelCommands> contextual_panel_handler = HandlerForProtocol(
        browser_->GetCommandDispatcher(), ContextualPanelCommands);
    [contextual_panel_handler showContextualPanelEntrypoint];
  }
}

void ContextualPanelBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  web_state_list_observation_.Reset();
  browser_ = nullptr;
}
