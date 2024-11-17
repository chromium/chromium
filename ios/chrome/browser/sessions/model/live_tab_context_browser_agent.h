// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_LIVE_TAB_CONTEXT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_LIVE_TAB_CONTEXT_BROWSER_AGENT_H_

#import <map>
#import <optional>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

namespace base {
class Uuid;
}

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
  std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;
  const std::optional<base::Uuid> GetSavedTabGroupIdForGroup(
      const tab_groups::TabGroupId& group) const override;
  bool IsTabPinned(int index) const override;
  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const sessions::tab_restore::Tab& tab,
      int tab_index,
      bool select,
      sessions::tab_restore::Type original_session_type) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const sessions::tab_restore::Tab& tab) override;
  void CloseTab() override;

 private:
  friend class BrowserUserData<LiveTabContextBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit LiveTabContextBrowserAgent(Browser* browser);

  raw_ptr<ProfileIOS> profile_;
  raw_ptr<WebStateList> web_state_list_;
  SessionID session_id_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_LIVE_TAB_CONTEXT_BROWSER_AGENT_H_
