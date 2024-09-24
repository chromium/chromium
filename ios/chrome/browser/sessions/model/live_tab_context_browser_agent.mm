// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/live_tab_context_browser_agent.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/sessions/core/session_types.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"
#import "ui/base/mojom/window_show_state.mojom.h"

BROWSER_USER_DATA_KEY_IMPL(LiveTabContextBrowserAgent)

LiveTabContextBrowserAgent::LiveTabContextBrowserAgent(Browser* browser)
    : profile_(browser->GetProfile()),
      web_state_list_(browser->GetWebStateList()),
      session_id_(SessionID::NewUnique()) {}

LiveTabContextBrowserAgent::~LiveTabContextBrowserAgent() {}

void LiveTabContextBrowserAgent::ShowBrowserWindow() {
  // No need to do anything here, as the singleton browser "window" is already
  // shown.
}

SessionID LiveTabContextBrowserAgent::GetSessionID() const {
  return session_id_;
}

sessions::SessionWindow::WindowType LiveTabContextBrowserAgent::GetWindowType()
    const {
  // Not supported by iOS.
  return sessions::SessionWindow::TYPE_NORMAL;
}

int LiveTabContextBrowserAgent::GetTabCount() const {
  return web_state_list_->count();
}

int LiveTabContextBrowserAgent::GetSelectedIndex() const {
  return web_state_list_->active_index();
}

std::string LiveTabContextBrowserAgent::GetAppName() const {
  return std::string();
}

std::string LiveTabContextBrowserAgent::GetUserTitle() const {
  return std::string();
}

sessions::LiveTab* LiveTabContextBrowserAgent::GetLiveTabAt(int index) const {
  return nullptr;
}

sessions::LiveTab* LiveTabContextBrowserAgent::GetActiveLiveTab() const {
  return nullptr;
}

std::map<std::string, std::string>
LiveTabContextBrowserAgent::GetExtraDataForTab(int index) const {
  return std::map<std::string, std::string>();
}

std::map<std::string, std::string>
LiveTabContextBrowserAgent::GetExtraDataForWindow() const {
  return std::map<std::string, std::string>();
}

std::optional<tab_groups::TabGroupId>
LiveTabContextBrowserAgent::GetTabGroupForTab(int index) const {
  // Not supported by iOS.
  return std::nullopt;
}

const tab_groups::TabGroupVisualData*
LiveTabContextBrowserAgent::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group) const {
  // Since we never return a group from GetTabGroupForTab(), this should never
  // be called.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool LiveTabContextBrowserAgent::IsTabPinned(int index) const {
  // Not supported by iOS.
  return false;
}

const std::optional<base::Uuid>
LiveTabContextBrowserAgent::GetSavedTabGroupIdForGroup(
    const tab_groups::TabGroupId& group) const {
  // Not supported by iOS... yet.
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

void LiveTabContextBrowserAgent::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  // Not supported on iOS.
}

const gfx::Rect LiveTabContextBrowserAgent::GetRestoredBounds() const {
  // Not supported by iOS.
  return gfx::Rect();
}

ui::mojom::WindowShowState LiveTabContextBrowserAgent::GetRestoredState()
    const {
  // Not supported by iOS.
  return ui::mojom::WindowShowState::kNormal;
}

std::string LiveTabContextBrowserAgent::GetWorkspace() const {
  // Not supported by iOS.
  return std::string();
}

sessions::LiveTab* LiveTabContextBrowserAgent::AddRestoredTab(
    const sessions::tab_restore::Tab& tab,
    int tab_index,
    bool select,
    sessions::tab_restore::Type original_session_type) {
  // TODO(crbug.com/40491734): Handle tab-switch animation somehow...
  web_state_list_->InsertWebState(
      session_util::CreateWebStateWithNavigationEntries(
          profile_, tab.normalized_navigation_index(), tab.navigations),
      WebStateList::InsertionParams::AtIndex(tab_index).Activate());
  return nullptr;
}

sessions::LiveTab* LiveTabContextBrowserAgent::ReplaceRestoredTab(
    const sessions::tab_restore::Tab& tab) {
  web_state_list_->ReplaceWebStateAt(
      web_state_list_->active_index(),
      session_util::CreateWebStateWithNavigationEntries(
          profile_, tab.normalized_navigation_index(), tab.navigations));

  return nullptr;
}

void LiveTabContextBrowserAgent::CloseTab() {
  web_state_list_->CloseWebStateAt(web_state_list_->active_index(),
                                   WebStateList::CLOSE_USER_ACTION);
}
