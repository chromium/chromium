// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_LIVE_TAB_CONTEXT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_LIVE_TAB_CONTEXT_BROWSER_AGENT_H_

#import <map>
#import <string>
#import <vector>

#import "components/keyed_service/core/keyed_service.h"
#import "components/sessions/core/live_tab_context.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"

class WebStateList;

// Implementation of sessions::LiveTabContext which uses a WebStateList
// (provided by the attached-to Browser) in order to fulfil its duties.
class LiveTabContextBrowserAgent
    : public sessions::LiveTabContext,
      public BrowserUserData<LiveTabContextBrowserAgent> {
 public:
  // Not copiable or movable.
  LiveTabContextBrowserAgent(const LiveTabContextBrowserAgent&) = delete;
  LiveTabContextBrowserAgent& operator=(const LiveTabContextBrowserAgent&) =
      delete;
  ~LiveTabContextBrowserAgent() override;

  // Sessions::LiveTabContext:
  void ShowBrowserWindow() override;
  SessionID GetSessionID() const override;
  sessions::SessionWindow::WindowType GetWindowType() const override;
  int GetTabCount() const override;
  int GetSelectedIndex() const override;
  std::string GetAppName() const override;
  std::string GetUserTitle() const override;
  sessions::LiveTab* GetLiveTabAt(int index) const override;
  sessions::LiveTab* GetActiveLiveTab() const override;
  std::map<std::string, std::string> GetExtraDataForTab(
      int index) const override;
  std::map<std::string, std::string> GetExtraDataForWindow() const override;
  absl::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;
  bool IsTabPinned(int index) const override;
  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      absl::optional<tab_groups::TabGroupId> group,
      const tab_groups::TabGroupVisualData& group_visual_data,
      bool select,
      bool pin,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override,
      const std::map<std::string, std::string>& extra_data,
      const SessionID* tab_id) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      absl::optional<tab_groups::TabGroupId> group,
      int selected_navigation,
      const std::string& extension_app_id,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override,
      const std::map<std::string, std::string>& extra_data) override;
  void CloseTab() override;

 private:
  friend class BrowserUserData<LiveTabContextBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit LiveTabContextBrowserAgent(Browser* browser);

  ChromeBrowserState* browser_state_;
  WebStateList* web_state_list_;
  SessionID session_id_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_LIVE_TAB_CONTEXT_BROWSER_AGENT_H_
