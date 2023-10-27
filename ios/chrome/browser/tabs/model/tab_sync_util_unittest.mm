// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_sync_util.h"

#import "base/time/time.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/fake_synced_sessions.h"
#import "testing/platform_test.h"

namespace {

using synced_sessions::DistantSession;

// Creates a distant session with the `tab_count` number of tabs. The tab at the
// `index_of_last_active_tab` index will be the latest active tab, with its
// last_active_time property set to `last_active_time`.
std::unique_ptr<DistantSession> CreateDistantSession(
    int tab_count,
    base::Time last_active_time,
    int index_of_last_active_tab) {
  auto distant_session = std::make_unique<DistantSession>();
  distant_session->tag = "session";
  distant_session->name = "session";
  distant_session->modified_time = base::Time::Now();
  distant_session->form_factor = syncer::DeviceInfo::FormFactor::kDesktop;

  for (int i = 0; i < tab_count; i++) {
    auto tab = std::make_unique<synced_sessions::DistantTab>();
    tab->session_tag = distant_session->tag;
    tab->tab_id = SessionID::FromSerializedValue(i);
    tab->title = u"Tab Title";
    tab->virtual_url = GURL("https://url");
    tab->modified_time = last_active_time;
    if (i == index_of_last_active_tab) {
      tab->last_active_time = last_active_time;
    } else {
      tab->last_active_time = last_active_time - base::Minutes(5);
    }

    distant_session->tabs.push_back(std::move(tab));
  }
  return distant_session;
}

}  // namespace

// Test fixture for the tab_sync_util file.
class TabSyncUtilTest : public PlatformTest {};

// Tests the -GetLastActiveDistantTab method.
TEST_F(TabSyncUtilTest, GetLastActiveDistantTab) {
  auto synced_sessions =
      std::make_unique<synced_sessions::FakeSyncedSessions>();
  LastActiveDistantTab no_session =
      GetLastActiveDistantTab(synced_sessions.get(), base::Hours(6));
  EXPECT_EQ(no_session.tab, nullptr);

  synced_sessions->AddSession(
      CreateDistantSession(20, base::Time::Now() - base::Hours(4), 3));
  LastActiveDistantTab one_session =
      GetLastActiveDistantTab(synced_sessions.get(), base::Hours(7));
  EXPECT_EQ(one_session.tab->tab_id, SessionID::FromSerializedValue(3));

  synced_sessions->AddSession(
      CreateDistantSession(30, base::Time::Now() - base::Hours(6), 6));
  LastActiveDistantTab two_sessions =
      GetLastActiveDistantTab(synced_sessions.get(), base::Hours(7));
  EXPECT_EQ(two_sessions.tab->tab_id, SessionID::FromSerializedValue(3));

  synced_sessions->AddSession(
      CreateDistantSession(23, base::Time::Now() - base::Hours(1), 2));
  LastActiveDistantTab three_sessions =
      GetLastActiveDistantTab(synced_sessions.get(), base::Hours(7));
  EXPECT_EQ(three_sessions.tab->tab_id, SessionID::FromSerializedValue(2));

  synced_sessions->AddSession(
      CreateDistantSession(52, base::Time::Now() - base::Hours(2), 5));
  LastActiveDistantTab four_sessions =
      GetLastActiveDistantTab(synced_sessions.get(), base::Hours(7));
  EXPECT_EQ(four_sessions.tab->tab_id, SessionID::FromSerializedValue(2));

  LastActiveDistantTab four_sessions_other_threshold =
      GetLastActiveDistantTab(synced_sessions.get(), base::Hours(3));
  EXPECT_EQ(four_sessions_other_threshold.tab->tab_id,
            SessionID::FromSerializedValue(2));

  LastActiveDistantTab four_sessions_invalid_threshold =
      GetLastActiveDistantTab(synced_sessions.get(), base::Minutes(2));
  EXPECT_EQ(four_sessions_invalid_threshold.tab, nullptr);
}
