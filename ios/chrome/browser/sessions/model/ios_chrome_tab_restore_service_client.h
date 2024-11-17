// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

class BrowserList;

// IOSChromeTabRestoreServiceClient provides an implementation of
// TabRestoreServiceClient that depends on ios/chrome/.
class IOSChromeTabRestoreServiceClient
    : public sessions::TabRestoreServiceClient {
 public:
  IOSChromeTabRestoreServiceClient(const base::FilePath& state_path,
                                   BrowserList* browser_list);

  IOSChromeTabRestoreServiceClient(const IOSChromeTabRestoreServiceClient&) =
      delete;
  IOSChromeTabRestoreServiceClient& operator=(
      const IOSChromeTabRestoreServiceClient&) = delete;

  ~IOSChromeTabRestoreServiceClient() override;

 private:
  // TabRestoreServiceClient:
  sessions::LiveTabContext* CreateLiveTabContext(
      sessions::LiveTabContext* existing_context,
      sessions::SessionWindow::WindowType type,
      const std::string& app_name,
      const gfx::Rect& bounds,
      ui::mojom::WindowShowState show_state,
      const std::string& workspace,
      const std::string& user_title,
      const std::map<std::string, std::string>& extra_data) override;
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

  const base::FilePath profile_path_;
  raw_ptr<BrowserList> browser_list_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_
