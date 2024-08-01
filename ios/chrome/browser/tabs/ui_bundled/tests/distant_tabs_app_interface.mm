// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/tests/distant_tabs_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/tabs/ui_bundled/tests/fake_distant_tab.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "url/gurl.h"

namespace {

// Creates a DistantTab for the given `session`.
std::unique_ptr<synced_sessions::DistantTab> CreateDistantTabWithTitleAndURL(
    const std::string& title,
    const GURL& url,
    synced_sessions::DistantSession& session) {
  // Tab ID to monotonically increment for each new tab to avoid conflicts.
  static int g_next_tab_id = 1;

  std::unique_ptr<synced_sessions::DistantTab> tab =
      std::make_unique<synced_sessions::DistantTab>();
  tab->session_tag = session.tag;
  tab->tab_id = SessionID::FromSerializedValue(g_next_tab_id++);
  tab->title = base::UTF8ToUTF16(title);
  tab->virtual_url = url;
  return tab;
}

// Creates a distant session.
synced_sessions::DistantSession& CreateDistantSession(
    std::string session_name,
    base::Time modified_time) {
  static synced_sessions::DistantSession distant_session;
  distant_session.tabs.clear();
  distant_session.tag = session_name;
  distant_session.name = session_name;
  distant_session.modified_time = modified_time;
  distant_session.form_factor = syncer::DeviceInfo::FormFactor::kDesktop;
  return distant_session;
}

}  // namespace

#pragma mark - DistantTabsAppInterface

@implementation DistantTabsAppInterface

+ (void)addSessionToFakeSyncServer:(NSString*)sessionName
                 modifiedTimeDelta:(base::TimeDelta)modifiedTimeDelta
                              tabs:(NSArray<FakeDistantTab*>*)tabs {
  base::Time modifiedTime = base::Time::Now() - modifiedTimeDelta;

  synced_sessions::DistantSession& distantSession =
      CreateDistantSession(base::SysNSStringToUTF8(sessionName), modifiedTime);
  for (NSUInteger i = 0; i < tabs.count; ++i) {
    FakeDistantTab* fakeDistantTab = tabs[i];

    std::unique_ptr<synced_sessions::DistantTab> distantTab =
        CreateDistantTabWithTitleAndURL(
            base::SysNSStringToUTF8(fakeDistantTab.title),
            GURL(base::SysNSStringToUTF8(fakeDistantTab.URL)), distantSession);
    distantTab->last_active_time = modifiedTime;
    distantSession.tabs.push_back(std::move(distantTab));
  }

  chrome_test_util::AddSessionToFakeSyncServer(distantSession);
}

@end
