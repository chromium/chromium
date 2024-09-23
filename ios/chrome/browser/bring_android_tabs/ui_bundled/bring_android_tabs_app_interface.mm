// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_test_session.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "url/gurl.h"

namespace {

void AddDistantTabWithTitleAndURLToSession(
    const std::string& title,
    const GURL& url,
    synced_sessions::DistantSession& session) {
  // Tab ID to monotonically increment for each new tab to avoid conflicts.
  static int g_next_tab_id = 1;

  auto tab = std::make_unique<synced_sessions::DistantTab>();
  tab->session_tag = session.tag;
  tab->tab_id = SessionID::FromSerializedValue(g_next_tab_id++);
  tab->title = base::UTF8ToUTF16(title);
  tab->virtual_url = url;
  session.tabs.push_back(std::move(tab));
}

// Distant session lazy getter for `g_recent_session_android_phone`. The
// `test_server` is needed for initialization purpose, but will be disregarded
// if the property is already set.
synced_sessions::DistantSession& GetRecentSessionFromAndroidPhone(
    const GURL& test_server) {
  static synced_sessions::DistantSession g_recent_session_android_phone;
  if (g_recent_session_android_phone.tag.empty()) {
    std::string sessionName("recent_session_from_android_phone");
    g_recent_session_android_phone.tag = sessionName;
    g_recent_session_android_phone.name = sessionName;
    g_recent_session_android_phone.modified_time =
        base::Time::Now() - base::Days(14) + base::Minutes(10);
    g_recent_session_android_phone.form_factor =
        syncer::DeviceInfo::FormFactor::kPhone;
    AddDistantTabWithTitleAndURLToSession("ponies",
                                          test_server.Resolve("/pony.html"),
                                          g_recent_session_android_phone);
    AddDistantTabWithTitleAndURLToSession(
        "chromium logo", test_server.Resolve("/chromium_logo_page.html"),
        g_recent_session_android_phone);
  }
  return g_recent_session_android_phone;
}

// Distant session lazy getter for `g_expired_session_android_phone`. The
// `test_server` is needed for initialization purpose, but will be disregarded
// if the property is already set.
synced_sessions::DistantSession& GetExpiredSessionFromAndroidPhone(
    const GURL& test_server) {
  static synced_sessions::DistantSession g_expired_session_android_phone;
  if (g_expired_session_android_phone.tag.empty()) {
    std::string sessionName("expired_session_from_android_phone");
    g_expired_session_android_phone.tag = sessionName;
    g_expired_session_android_phone.name = sessionName;
    g_expired_session_android_phone.modified_time =
        base::Time::Now() - base::Days(14) - base::Minutes(10);
    g_expired_session_android_phone.form_factor =
        syncer::DeviceInfo::FormFactor::kPhone;
    AddDistantTabWithTitleAndURLToSession(
        "Full Screen", test_server.Resolve("/tall_page.html"),
        g_expired_session_android_phone);
  }
  return g_expired_session_android_phone;
}

// Distant session lazy getter for `g_recent_session_desktop`. The `test_server`
// is needed for initialization purpose, but will be disregarded if the property
// is already set.
synced_sessions::DistantSession& GetRecentSessionFromDesktop(
    const GURL& test_server) {
  static synced_sessions::DistantSession g_recent_session_desktop;
  if (g_recent_session_desktop.tag.empty()) {
    std::string sessionName = "recent_session_from_desktop";
    g_recent_session_desktop.tag = sessionName;
    g_recent_session_desktop.name = sessionName;
    g_recent_session_desktop.modified_time =
        base::Time::Now() - base::Days(14) + base::Minutes(10);
    g_recent_session_desktop.form_factor =
        syncer::DeviceInfo::FormFactor::kDesktop;
    AddDistantTabWithTitleAndURLToSession(
        "Test Request Desktop/Mobile Script",
        test_server.Resolve("/user_agent_test_page.html"),
        g_recent_session_desktop);
  }
  return g_recent_session_desktop;
}

}  // namespace

#pragma mark - BringAndroidTabsAppInterface

@implementation BringAndroidTabsAppInterface

+ (void)addFakeSyncServerSession:(BringAndroidTabsTestSession)sessionType
                  fromTestServer:(NSString*)testServerHost {
  const GURL testServer = GURL(base::SysNSStringToUTF8(testServerHost));
  switch (sessionType) {
    case BringAndroidTabsTestSession::kRecentFromAndroidPhone:
      chrome_test_util::AddSessionToFakeSyncServer(
          GetRecentSessionFromAndroidPhone(testServer));
      return;
    case BringAndroidTabsTestSession::kExpiredFromAndroidPhone:
      chrome_test_util::AddSessionToFakeSyncServer(
          GetExpiredSessionFromAndroidPhone(testServer));
      return;
    case BringAndroidTabsTestSession::kRecentFromDesktop:
      chrome_test_util::AddSessionToFakeSyncServer(
          GetRecentSessionFromDesktop(testServer));
      return;
  }
}

+ (int)tabsCountForPrompt {
  const synced_sessions::DistantSession& session =
      GetRecentSessionFromAndroidPhone(GURL());
  size_t tabCount = session.tabs.size();

  // If the URL of the session tab is empty, the session itself is created at
  // the time of declaration, instead of before. That means it has not been
  // added to the fake sync server, and should not be prompted.
  if (tabCount < 1 || session.tabs[0]->virtual_url.is_empty()) {
    return 0;
  }
  return static_cast<int>(tabCount);
}

@end
