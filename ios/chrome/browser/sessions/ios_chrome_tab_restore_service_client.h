// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_

#include "base/macros.h"
#include "components/sessions/core/tab_restore_service_client.h"

class ChromeBrowserState;

// IOSChromeTabRestoreServiceClient provides an implementation of
// TabRestoreServiceClient that depends on ios/chrome/.
class IOSChromeTabRestoreServiceClient
    : public sessions::TabRestoreServiceClient {
 public:
  explicit IOSChromeTabRestoreServiceClient(ChromeBrowserState* browser_state);
  ~IOSChromeTabRestoreServiceClient() override;

 private:
  // TabRestoreServiceClient:
  sessions::LiveTabContext* CreateLiveTabContext(
      const std::string& app_name,
      const gfx::Rect& bounds,
      ui::WindowShowState show_state,
      const std::string& workspace,
      const std::string& user_title) override;
  sessions::LiveTabContext* FindLiveTabContextForTab(
      const sessions::LiveTab* tab) override;
  sessions::LiveTabContext* FindLiveTabContextWithID(
      SessionID desired_id) override;
  sessions::LiveTabContext* FindLiveTabContextWithGroup(
      tab_groups::TabGroupId group) override;
  bool ShouldTrackURLForRestore(const GURL& url) override;
  std::string GetExtensionAppIDForTab(sessions::LiveTab* tab) override;
  base::FilePath GetPathToSaveTo() override;
  GURL GetNewTabURL() override;
  bool HasLastSession() override;
  void GetLastSession(sessions::GetLastSessionCallback callback) override;

  ChromeBrowserState* browser_state_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeTabRestoreServiceClient);
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_
