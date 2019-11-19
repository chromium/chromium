// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/sessions/core/session_types.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabRestoreServiceDelegateImplIOS::TabRestoreServiceDelegateImplIOS(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state), session_id_(SessionID::NewUnique()) {}

TabRestoreServiceDelegateImplIOS::~TabRestoreServiceDelegateImplIOS() {}

WebStateList* TabRestoreServiceDelegateImplIOS::GetWebStateList() const {
  TabModel* tab_model =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state_);
  DCHECK([tab_model webStateList]);
  return [tab_model webStateList];
}

void TabRestoreServiceDelegateImplIOS::ShowBrowserWindow() {
  // No need to do anything here, as the singleton browser "window" is already
  // shown.
}

SessionID TabRestoreServiceDelegateImplIOS::GetSessionID() const {
  return session_id_;
}

int TabRestoreServiceDelegateImplIOS::GetTabCount() const {
  return GetWebStateList()->count();
}

int TabRestoreServiceDelegateImplIOS::GetSelectedIndex() const {
  return GetWebStateList()->active_index();
}

std::string TabRestoreServiceDelegateImplIOS::GetAppName() const {
  return std::string();
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::GetLiveTabAt(
    int index) const {
  return nullptr;
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::GetActiveLiveTab() const {
  return nullptr;
}

bool TabRestoreServiceDelegateImplIOS::IsTabPinned(int index) const {
  // Not supported by iOS.
  return false;
}

base::Optional<base::Token> TabRestoreServiceDelegateImplIOS::GetTabGroupForTab(
    int index) const {
  // Not supported by iOS.
  return base::nullopt;
}

TabRestoreServiceDelegateImplIOS::TabGroupMetadata
TabRestoreServiceDelegateImplIOS::GetTabGroupMetadata(base::Token group) const {
  // Since we never return a group from GetTabGroupForTab(), this should never
  // be called.
  NOTREACHED();
  return TabGroupMetadata();
}

const gfx::Rect TabRestoreServiceDelegateImplIOS::GetRestoredBounds() const {
  // Not supported by iOS.
  return gfx::Rect();
}

ui::WindowShowState TabRestoreServiceDelegateImplIOS::GetRestoredState() const {
  // Not supported by iOS.
  return ui::SHOW_STATE_NORMAL;
}

std::string TabRestoreServiceDelegateImplIOS::GetWorkspace() const {
  // Not supported by iOS.
  return std::string();
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::AddRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    base::Optional<base::Token> group,
    bool select,
    bool pin,
    bool from_last_session,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const std::string& user_agent_override) {
  // TODO(crbug.com/661636): Handle tab-switch animation somehow...
  WebStateList* web_state_list = GetWebStateList();
  web_state_list->InsertWebState(
      tab_index,
      session_util::CreateWebStateWithNavigationEntries(
          browser_state_, selected_navigation, navigations),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  return nullptr;
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::ReplaceRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    base::Optional<base::Token> group,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const std::string& user_agent_override) {
  WebStateList* web_state_list = GetWebStateList();
  web_state_list->ReplaceWebStateAt(
      web_state_list->active_index(),
      session_util::CreateWebStateWithNavigationEntries(
          browser_state_, selected_navigation, navigations));

  return nullptr;
}

void TabRestoreServiceDelegateImplIOS::CloseTab() {
  WebStateList* web_state_list = GetWebStateList();
  web_state_list->CloseWebStateAt(web_state_list->active_index(),
                                  WebStateList::CLOSE_USER_ACTION);
}

void TabRestoreServiceDelegateImplIOS::SetTabGroupMetadata(
    base::Token group,
    TabGroupMetadata group_metadata) {
  // Not supported on iOS.
}
