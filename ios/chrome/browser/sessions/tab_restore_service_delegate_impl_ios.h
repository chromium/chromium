// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_IMPL_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_IMPL_IOS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/live_tab_context.h"

class WebStateList;

namespace ios {
class ChromeBrowserState;
}

// Implementation of sessions::LiveTabContext which uses an instance
// of TabModel in order to fulfil its duties.
class TabRestoreServiceDelegateImplIOS : public sessions::LiveTabContext,
                                         public KeyedService {
 public:
  explicit TabRestoreServiceDelegateImplIOS(
      ios::ChromeBrowserState* browser_state);
  ~TabRestoreServiceDelegateImplIOS() override;

  // Overridden from KeyedService:
  void Shutdown() override {}

  // Overridden from sessions::LiveTabContext:
  void ShowBrowserWindow() override;
  SessionID GetSessionID() const override;
  int GetTabCount() const override;
  int GetSelectedIndex() const override;
  std::string GetAppName() const override;
  sessions::LiveTab* GetLiveTabAt(int index) const override;
  sessions::LiveTab* GetActiveLiveTab() const override;
  bool IsTabPinned(int index) const override;
  base::Optional<base::Token> GetTabGroupForTab(int index) const override;
  TabGroupMetadata GetTabGroupMetadata(base::Token group) const override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      base::Optional<base::Token> group,
      bool select,
      bool pin,
      bool from_last_session,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const std::string& user_agent_override) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      base::Optional<base::Token> group,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const std::string& user_agent_override) override;
  void CloseTab() override;
  void SetTabGroupMetadata(base::Token group,
                           TabGroupMetadata group_metadata) override;

 private:
  // Retrieves the current |WebStateList| corresponding to |browser_state_|;
  WebStateList* GetWebStateList() const;

  ios::ChromeBrowserState* browser_state_;  // weak
  SessionID session_id_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreServiceDelegateImplIOS);
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_IMPL_IOS_H_
