// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_app_interface.h"

#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Website names and urls.
const char kChromiumURL[] = "https://chromium.org/";
const char kGoogleURL[] = "https://google.com/";
const char kYouTubeURL[] = "https://youtube.com/";
const char kBardURL[] = "https://bard.google.com/";

}  // namespace

@implementation BringAndroidTabsAppInterface

// Distant sessions to be injected to fake server. Instance variables to be set
// lazily; please use the respective private getter (e.g.
// [BringAndroidTabsAppInterface recentSessionFromAndroidPhone]) for access.
static synced_sessions::DistantSession _recentSessionFromAndroidPhone;
static synced_sessions::DistantSession _expiredSessionFromAndroidPhone;
static synced_sessions::DistantSession _recentSessionFromDesktop;

// Tab ID to monotonically increment for each new tab to avoid conflicts.
static int _tabId = 1;

+ (void)addSessionToFakeSyncServer:
    (BringAndroidTabsAppInterfaceForeignSession)session {
  switch (session) {
    case BringAndroidTabsAppInterfaceForeignSession::kRecentFromAndroidPhone:
      chrome_test_util::AddSessionToFakeSyncServer(
          [BringAndroidTabsAppInterface recentSessionFromAndroidPhone]);
      return;
    case BringAndroidTabsAppInterfaceForeignSession::kExpiredFromAndroidPhone:
      chrome_test_util::AddSessionToFakeSyncServer(
          [BringAndroidTabsAppInterface expiredSessionFromAndroidPhone]);
      return;
    case BringAndroidTabsAppInterfaceForeignSession::kRecentFromDesktop:
      chrome_test_util::AddSessionToFakeSyncServer(
          [BringAndroidTabsAppInterface recentSessionFromDesktop]);
      return;
  }
}

+ (int)tabsCountForSession:(BringAndroidTabsAppInterfaceForeignSession)session {
  size_t count;
  switch (session) {
    case BringAndroidTabsAppInterfaceForeignSession::kRecentFromAndroidPhone:
      count = [BringAndroidTabsAppInterface recentSessionFromAndroidPhone]
                  .tabs.size();
      break;
    case BringAndroidTabsAppInterfaceForeignSession::kExpiredFromAndroidPhone:
      count = [BringAndroidTabsAppInterface expiredSessionFromAndroidPhone]
                  .tabs.size();
      break;
    case BringAndroidTabsAppInterfaceForeignSession::kRecentFromDesktop:
      count =
          [BringAndroidTabsAppInterface recentSessionFromDesktop].tabs.size();
      break;
  }
  return static_cast<int>(count);
}

#pragma mark - Private

+ (synced_sessions::DistantSession&)recentSessionFromAndroidPhone {
  if (_recentSessionFromAndroidPhone.tag.empty()) {
    std::string sessionName("recent_session_from_android_phone");
    _recentSessionFromAndroidPhone.tag = sessionName;
    _recentSessionFromAndroidPhone.name = sessionName;
    _recentSessionFromAndroidPhone.modified_time =
        base::Time::Now() - base::Days(14) + base::Minutes(10);
    _recentSessionFromAndroidPhone.form_factor =
        syncer::DeviceInfo::FormFactor::kPhone;
    [BringAndroidTabsAppInterface
        addDistantTabWithTitle:"Home"
                           URL:kChromiumURL
                     toSession:_recentSessionFromAndroidPhone];
    [BringAndroidTabsAppInterface
        addDistantTabWithTitle:"Google"
                           URL:kGoogleURL
                     toSession:_recentSessionFromAndroidPhone];
  }
  return _recentSessionFromAndroidPhone;
}

+ (synced_sessions::DistantSession&)expiredSessionFromAndroidPhone {
  if (_expiredSessionFromAndroidPhone.tag.empty()) {
    std::string sessionName("expired_session_from_android_phone");
    _expiredSessionFromAndroidPhone.tag = sessionName;
    _expiredSessionFromAndroidPhone.name = sessionName;
    _expiredSessionFromAndroidPhone.modified_time =
        base::Time::Now() - base::Days(14) - base::Minutes(10);
    _expiredSessionFromAndroidPhone.form_factor =
        syncer::DeviceInfo::FormFactor::kPhone;
    [BringAndroidTabsAppInterface
        addDistantTabWithTitle:"YouTube"
                           URL:kYouTubeURL
                     toSession:_expiredSessionFromAndroidPhone];
  }
  return _expiredSessionFromAndroidPhone;
}

+ (synced_sessions::DistantSession&)recentSessionFromDesktop {
  if (_recentSessionFromDesktop.tag.empty()) {
    std::string sessionName = "recent_session_from_desktop";
    _recentSessionFromDesktop.tag = sessionName;
    _recentSessionFromDesktop.name = sessionName;
    _recentSessionFromDesktop.modified_time =
        base::Time::Now() - base::Days(14) + base::Minutes(10);
    _recentSessionFromDesktop.form_factor =
        syncer::DeviceInfo::FormFactor::kDesktop;
    [BringAndroidTabsAppInterface
        addDistantTabWithTitle:"Bard"
                           URL:kBardURL
                     toSession:_recentSessionFromDesktop];
  }
  return _recentSessionFromDesktop;
}

+ (void)addDistantTabWithTitle:(std::string)title
                           URL:(std::string)url
                     toSession:(synced_sessions::DistantSession&)session {
  auto tab = std::make_unique<synced_sessions::DistantTab>();
  tab->session_tag = session.tag;
  tab->tab_id = SessionID::FromSerializedValue(_tabId++);
  tab->title = base::UTF8ToUTF16(title);
  tab->virtual_url = GURL(url);
  session.tabs.push_back(std::move(tab));
}

@end
