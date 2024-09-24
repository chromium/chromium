// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_client.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/sessions/ios/ios_live_tab.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/sessions/model/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/synced_window_delegate_browser_agent.h"
#import "ui/base/mojom/window_show_state.mojom.h"
#import "url/gurl.h"

namespace {
sessions::LiveTabContext* FindLiveTabContextWithCondition(
    BrowserList* browser_list,
    base::RepeatingCallback<bool(Browser*)> condition) {
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kAll)) {
    if (condition.Run(browser)) {
      return LiveTabContextBrowserAgent::FromBrowser(browser);
    }
  }

  return nullptr;
}
}  // namespace

IOSChromeTabRestoreServiceClient::IOSChromeTabRestoreServiceClient(
    const base::FilePath& state_path,
    BrowserList* browser_list)
    : profile_path_(state_path), browser_list_(browser_list) {
  DCHECK(!profile_path_.empty());
  DCHECK(browser_list_);
}

IOSChromeTabRestoreServiceClient::~IOSChromeTabRestoreServiceClient() = default;

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::CreateLiveTabContext(
    sessions::LiveTabContext* /* existing_context */,
    sessions::SessionWindow::WindowType type,
    const std::string& /* app_name */,
    const gfx::Rect& /* bounds */,
    ui::mojom::WindowShowState /* show_state */,
    const std::string& /* workspace */,
    const std::string& /* user_title */,
    const std::map<std::string, std::string>& /* extra_data */) {
  NOTREACHED_IN_MIGRATION()
      << "Tab restore service attempting to create a new window.";
  return nullptr;
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextForTab(
    const sessions::LiveTab* tab) {
  const sessions::IOSLiveTab* requested_tab =
      static_cast<const sessions::IOSLiveTab*>(tab);
  const web::WebState* web_state = requested_tab->GetWebState();
  if (!web_state) {
    return nullptr;
  }
  return FindLiveTabContextWithCondition(
      browser_list_,
      base::BindRepeating(
          [](const web::WebState* web_state, Browser* browser) {
            WebStateList* web_state_list = browser->GetWebStateList();
            const int index = web_state_list->GetIndexOfWebState(web_state);
            return index != WebStateList::kInvalidIndex;
          },
          web_state));
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextWithID(
    SessionID desired_id) {
  return FindLiveTabContextWithCondition(
      browser_list_,
      base::BindRepeating(
          [](SessionID desired_id, Browser* browser) {
            SyncedWindowDelegateBrowserAgent* syncedWindowDelegate =
                SyncedWindowDelegateBrowserAgent::FromBrowser(browser);
            return syncedWindowDelegate->GetSessionId() == desired_id;
          },
          desired_id));
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextWithGroup(
    tab_groups::TabGroupId group) {
  return nullptr;
}

bool IOSChromeTabRestoreServiceClient::ShouldTrackURLForRestore(
    const GURL& url) {
  // NOTE: In the //chrome client, chrome://quit and chrome://restart are
  // blocked from being tracked to avoid entering infinite loops. However,
  // iOS intentionally does not support those URLs, so there is no need to
  // block them here.
  return url.is_valid();
}

std::string IOSChromeTabRestoreServiceClient::GetExtensionAppIDForTab(
    sessions::LiveTab* tab) {
  return std::string();
}

base::FilePath IOSChromeTabRestoreServiceClient::GetPathToSaveTo() {
  return profile_path_;
}

GURL IOSChromeTabRestoreServiceClient::GetNewTabURL() {
  return GURL(kChromeUINewTabURL);
}

bool IOSChromeTabRestoreServiceClient::HasLastSession() {
  return false;
}

void IOSChromeTabRestoreServiceClient::GetLastSession(
    sessions::GetLastSessionCallback callback) {
  NOTREACHED_IN_MIGRATION();
}
