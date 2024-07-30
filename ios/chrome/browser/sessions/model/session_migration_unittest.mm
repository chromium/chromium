// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_migration.h"

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "ios/chrome/browser/sessions/model/session_tab_group.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/tab_group_util.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using SessionMigrationTest = PlatformTest;

using tab_groups::TabGroupId;

namespace {

// Information about a single tab.
struct TabInfo {
  const int opener_index = -1;
  const int opener_navigation_index = -1;
  const bool create_web_session = true;
};

// Information about a tab group.
struct TabGroupInfo {
  const int range_start = -1;
  const int range_count = 0;
  const std::u16string title = u"";
  const tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;
  const bool collapsed_state = false;
  const TabGroupId tab_group_id = TabGroupId::GenerateNew();
};

// Information about a session.
struct SessionInfo {
  const int active_index = -1;
  const int pinned_tab_count = 0;
  const base::span<const TabInfo> tabs;
  const base::span<const TabGroupInfo> tab_groups;
};

// Name of the sessions used by the tests (random string obtained by
// running `uuidgen` on the command-line, no meaning to it).
const char kSessionName1[] = "CB5AC1AF-8E93-407B-AE48-65ECE7C241C0";
const char kSessionName2[] = "B829AD40-FD11-4874-B5BF-B56C13DC2496";

// Name of the sub-directory of the off-the-record BrowserState data.
const base::FilePath::CharType kOTRDirectory[] = FILE_PATH_LITERAL("OTR");

// Constants used to populate navigation items.
const char kPageURL[] = "https://www.example.com";
const char kPageTitle[] = "Example Domain";

// Name of unrelated files.
const base::FilePath::CharType kUnrelatedFilename[] =
    FILE_PATH_LITERAL("Tabs_13346873882166178");

// Constants representing the default session used for tests.
constexpr TabInfo kTabs1[] = {
    TabInfo{},
    TabInfo{},
    TabInfo{.create_web_session = false},
    TabInfo{.opener_index = 2, .opener_navigation_index = 0},
    TabInfo{.opener_index = 2, .opener_navigation_index = 0},
};

constexpr SessionInfo kSessionInfo1 = {
    .active_index = 1,
    .pinned_tab_count = 2,
    .tabs = base::make_span(kTabs1),
};

constexpr TabInfo kTabs2[] = {
    TabInfo{},
    TabInfo{},
};

const TabGroupInfo kTabGroups1[] = {
    TabGroupInfo{
        .range_start = 0,
        .range_count = 1,
        .title = u"kTabGroup1",
        .color = tab_groups::TabGroupColorId::kGrey,
    },
};

constexpr SessionInfo kSessionInfo2 = {
    .active_index = 0,
    .pinned_tab_count = 0,
    .tabs = base::make_span(kTabs2),
};

constexpr SessionInfo kSessionWithGroupsInfo = {
    .active_index = 0,
    .pinned_tab_count = 0,
    .tabs = base::make_span(kTabs2),
    .tab_groups = base::make_span(kTabGroups1),
};

// Returns the path to the directory containing the optimized session
// named `name` (located in `root` sub-directory).
base::FilePath GetOptimizedSessionDir(const base::FilePath& root,
                                      const std::string& name) {
  return root.Append(kSessionRestorationDirname).AppendASCII(name);
}

// Returns the path to the directory containing the legacy session
// named `name` (located in `root` sub-directory).
base::FilePath GetLegacySessionDir(const base::FilePath& root,
                                   const std::string& name) {
  return root.Append(kLegacySessionsDirname).AppendASCII(name);
}

// Returns the path to the file containing the web sessions for the
// tab with identifier `identifier` for a legacy session (relative
// to the web session directory `web_sessions`).
base::FilePath GetLegacyWebSessionsFile(const base::FilePath& web_sessions,
                                        web::WebStateID identifier) {
  return web_sessions.Append(
      base::StringPrintf("%08u", identifier.identifier()));
}

// Returns the path to the directory containing the optimized storage
// for a tab named `identifier` for session `session_dir`.
base::FilePath GetOptimizedWebStateDir(const base::FilePath& session_dir,
                                       web::WebStateID identifier) {
  return session_dir.Append(
      base::StringPrintf("%08x", identifier.identifier()));
}

// Converts an active index into a selected index.
NSUInteger SelectedIndexFromActiveIndex(int active_index) {
  return active_index < 0 ? static_cast<NSUInteger>(NSNotFound)
                          : static_cast<NSUInteger>(active_index);
}

// Creates a CRWNavigationItemStorage.
CRWNavigationItemStorage* CreateNavigationItemStorage() {
  CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
  item.title = base::UTF8ToUTF16(&kPageTitle[0]);
  item.virtualURL = GURL(kPageURL);
  return item;
}

// Creates a CRWSessionStorage following `tab_info` and `is_pinned`.
CRWSessionStorage* CreateSessionStorage(TabInfo tab_info, bool is_pinned) {
  CRWSessionUserData* user_data = [[CRWSessionUserData alloc] init];
  if (tab_info.opener_index != -1 && tab_info.opener_navigation_index != -1) {
    [user_data setObject:@(tab_info.opener_index)
                  forKey:kLegacyWebStateListOpenerIndexKey];
    [user_data setObject:@(tab_info.opener_navigation_index)
                  forKey:kLegacyWebStateListOpenerNavigationIndexKey];
  }
  if (is_pinned) {
    [user_data setObject:@YES forKey:kLegacyWebStateListPinnedStateKey];
  }

  CRWSessionStorage* session = [[CRWSessionStorage alloc] init];
  session.stableIdentifier = [[NSUUID UUID] UUIDString];
  session.uniqueIdentifier = web::WebStateID::NewUnique();
  session.itemStorages = @[ CreateNavigationItemStorage() ];
  session.creationTime = base::Time::Now();
  session.lastActiveTime = base::Time::Now();
  session.userData = user_data;
  return session;
}

// Creates a legacy session named `name` following `session_info` and
// writes it to the expected location relative to `root`. It returns
// whether the creation was a success.
bool GenerateLegacySession(const base::FilePath& root,
                           const std::string& name,
                           SessionInfo session_info) {
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);

  // Create all the tabs for the session. Note that the WebStateList metadata
  // is stored with the tab in the legacy format.
  NSMutableArray<CRWSessionStorage*>* sessions = [[NSMutableArray alloc] init];
  for (size_t index = 0; index < session_info.tabs.size(); ++index) {
    // Create a fake tab with a single navigation item.
    const bool is_pinned =
        static_cast<int>(index) < session_info.pinned_tab_count;
    CRWSessionStorage* session =
        CreateSessionStorage(session_info.tabs[index], is_pinned);

    // Create fake web session data for the tab. As the file contains
    // opaque data from WebKit, the migration code does not care about
    // the format.
    if (session_info.tabs[index].create_web_session) {
      const base::FilePath filename =
          GetLegacyWebSessionsFile(web_sessions, session.uniqueIdentifier);
      NSData* data = [[NSString stringWithFormat:@"data %zu", index]
          dataUsingEncoding:NSUTF8StringEncoding];
      if (!ios::sessions::WriteFile(filename, data)) {
        return false;
      }
    }

    [sessions addObject:session];
  }

  // Create tab groups for the session.
  NSMutableArray<SessionTabGroup*>* groups = [[NSMutableArray alloc] init];
  for (size_t index = 0; index < session_info.tab_groups.size(); ++index) {
    const TabGroupInfo group_info = session_info.tab_groups[index];
    SessionTabGroup* session_tab_group = [[SessionTabGroup alloc]
        initWithRangeStart:group_info.range_start
                rangeCount:group_info.range_count
                     title:base::SysUTF16ToNSString(group_info.title)
                   colorId:static_cast<NSInteger>(group_info.color)
            collapsedState:group_info.collapsed_state
                tabGroupId:group_info.tab_group_id];
    [groups addObject:session_tab_group];
  }

  const NSUInteger selected_index =
      SelectedIndexFromActiveIndex(session_info.active_index);

  SessionWindowIOS* session =
      [[SessionWindowIOS alloc] initWithSessions:sessions
                                       tabGroups:groups
                                   selectedIndex:selected_index];

  // Write the session file.
  const base::FilePath filename =
      GetLegacySessionDir(root, name).Append(kLegacySessionFilename);
  return ios::sessions::WriteSessionWindow(filename, session);
}

// Creates a legacy session named `name` following `session_info` with
// invalid "unique identifiers" and writes it to the expected location
// relative to `root`. It returns whether the creation was a success.
bool GenerateLegacySessionInvalidUniqueIdentifiers(const base::FilePath& root,
                                                   const std::string& name,
                                                   SessionInfo session_info) {
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);

  // Create all the tabs for the session. Note that the WebStateList metadata
  // is stored with the tab in the legacy format.
  NSMutableArray<CRWSessionStorage*>* sessions = [[NSMutableArray alloc] init];
  for (size_t index = 0; index < session_info.tabs.size(); ++index) {
    // Create a fake tab with a single navigation item.
    const bool is_pinned =
        static_cast<int>(index) < session_info.pinned_tab_count;
    CRWSessionStorage* session =
        CreateSessionStorage(session_info.tabs[index], is_pinned);

    // Create fake web session data for the tab. As the file contains
    // opaque data from WebKit, the migration code does not care about
    // the format.
    if (session_info.tabs[index].create_web_session) {
      const base::FilePath filename =
          GetLegacyWebSessionsFile(web_sessions, session.uniqueIdentifier);
      NSData* data = [[NSString stringWithFormat:@"data %zu", index]
          dataUsingEncoding:NSUTF8StringEncoding];
      if (!ios::sessions::WriteFile(filename, data)) {
        return false;
      }
    } else {
      session.uniqueIdentifier = web::WebStateID();  // Clear unique identifier!
    }

    [sessions addObject:session];
  }

  const NSUInteger selected_index =
      SelectedIndexFromActiveIndex(session_info.active_index);

  SessionWindowIOS* session =
      [[SessionWindowIOS alloc] initWithSessions:sessions
                                       tabGroups:@[]
                                   selectedIndex:selected_index];

  // Write the session file.
  const base::FilePath filename =
      GetLegacySessionDir(root, name).Append(kLegacySessionFilename);
  return ios::sessions::WriteSessionWindow(filename, session);
}

// Creates a WebStateMetadataStorage.
web::proto::WebStateMetadataStorage CreateWebStateMetadataStorage() {
  web::proto::WebStateMetadataStorage storage;
  storage.set_navigation_item_count(1);
  storage.mutable_active_page()->set_page_title(kPageTitle);
  storage.mutable_active_page()->set_page_url(kPageURL);
  web::SerializeTimeToProto(base::Time::Now(),
                            *storage.mutable_creation_time());
  web::SerializeTimeToProto(base::Time::Now(),
                            *storage.mutable_last_active_time());
  return storage;
}

// Creates a WebStateStorage.
web::proto::WebStateStorage CreateWebStateStorage() {
  web::proto::NavigationItemStorage item_storage;
  item_storage.set_virtual_url(kPageURL);
  item_storage.set_title(kPageTitle);

  web::proto::WebStateStorage storage;
  storage.mutable_navigation()->set_last_committed_item_index(0);
  *storage.mutable_navigation()->add_items() = item_storage;
  return storage;
}

// Creates an optimized session named `name` following `session_info` and
// writes it to the expected location relative to `root`. It returns
// whether the creation was a success.
bool GenerateOptimizedSession(const base::FilePath& root,
                              const std::string& name,
                              SessionInfo session_info) {
  const base::FilePath session_dir = GetOptimizedSessionDir(root, name);

  ios::proto::WebStateListStorage storage;
  storage.set_active_index(session_info.active_index);
  storage.set_pinned_item_count(session_info.pinned_tab_count);

  // Create all the tabs for the session. Note that the WebStateList metadata
  // is stored with the tab in the legacy format.
  for (size_t index = 0; index < session_info.tabs.size(); ++index) {
    const web::WebStateID identifier = web::WebStateID::NewUnique();
    const base::FilePath item_dir =
        GetOptimizedWebStateDir(session_dir, identifier);
    const TabInfo& tab_info = session_info.tabs[index];

    ios::proto::WebStateListItemStorage& item_storage = *storage.add_items();
    item_storage.set_identifier(identifier.identifier());
    if (tab_info.opener_index != -1 && tab_info.opener_navigation_index != -1) {
      item_storage.mutable_opener()->set_index(tab_info.opener_index);
      item_storage.mutable_opener()->set_navigation_index(
          tab_info.opener_navigation_index);
    }

    // Set the metadata into the WebStateListItemStorage.
    *item_storage.mutable_metadata() = CreateWebStateMetadataStorage();

    // Write the tab data file.
    if (!ios::sessions::WriteProto(item_dir.Append(kWebStateStorageFilename),
                                   CreateWebStateStorage())) {
      return false;
    }

    // Create fake web session data for the tab. As the file contains
    // opaque data from WebKit, the migration code does not care about
    // the format.
    if (tab_info.create_web_session) {
      const base::FilePath filename = item_dir.Append(kWebStateSessionFilename);

      NSData* data = [[NSString stringWithFormat:@"data %zu", index]
          dataUsingEncoding:NSUTF8StringEncoding];
      if (!ios::sessions::WriteFile(filename, data)) {
        return false;
      }
    }
  }

  // Create tab groups for the session.
  for (size_t index = 0; index < session_info.tab_groups.size(); ++index) {
    const TabGroupInfo group_info = session_info.tab_groups[index];

    ios::proto::TabGroupStorage& group_storage = *storage.add_groups();
    ios::proto::RangeIndex& range = *group_storage.mutable_range();

    range.set_start(group_info.range_start);
    range.set_count(group_info.range_count);

    group_storage.set_title(base::UTF16ToUTF8(group_info.title));
    group_storage.set_color(tab_group_util::ColorForStorage(group_info.color));
    group_storage.set_collapsed(group_info.collapsed_state);
    tab_group_util::TabGroupIdForStorage(group_info.tab_group_id,
                                         *group_storage.mutable_tab_group_id());
  }

  // Write the session metadata file.
  const base::FilePath filename = session_dir.Append(kSessionMetadataFilename);
  return ios::sessions::WriteProto(filename, storage);
}

// Checks whether the optimized session in `root` named `name` corresponds
// to the session described by `session_info`.
void CheckOptimizedSession(const base::FilePath& root,
                           const std::string& name,
                           SessionInfo session_info) {
  const base::FilePath session_dir = GetOptimizedSessionDir(root, name);

  // Load the optimized session from disk.
  ios::proto::WebStateListStorage storage;
  ASSERT_TRUE(ios::sessions::ParseProto(
      session_dir.Append(kSessionMetadataFilename), storage));

  // Check that the session metadata are correct.
  EXPECT_EQ(storage.active_index(), session_info.active_index);
  EXPECT_EQ(storage.pinned_item_count(), session_info.pinned_tab_count);

  // Check that each tab metadata and data are correct.
  ASSERT_EQ(storage.items_size(), static_cast<int>(session_info.tabs.size()));
  for (size_t index = 0; index < session_info.tabs.size(); ++index) {
    const ios::proto::WebStateListItemStorage& item_info = storage.items(index);
    const TabInfo& tab_info = session_info.tabs[index];

    // Check the opener-opener relationship for correctness.
    if (tab_info.opener_index != -1 && tab_info.opener_navigation_index != -1) {
      EXPECT_TRUE(item_info.has_opener());
      const ios::proto::OpenerStorage& opener = item_info.opener();
      EXPECT_EQ(opener.index(), tab_info.opener_index);
      EXPECT_EQ(opener.navigation_index(), tab_info.opener_navigation_index);
    } else {
      EXPECT_FALSE(item_info.has_opener());
    }

    // Check the tab metadata for correctness.
    ASSERT_TRUE(item_info.has_metadata());
    const web::proto::WebStateMetadataStorage& metadata = item_info.metadata();

    EXPECT_EQ(metadata.navigation_item_count(), 1);
    EXPECT_NE(web::TimeFromProto(metadata.creation_time()), base::Time());
    EXPECT_NE(web::TimeFromProto(metadata.last_active_time()), base::Time());
    EXPECT_TRUE(metadata.has_active_page());
    EXPECT_EQ(metadata.active_page().page_title(), kPageTitle);
    EXPECT_EQ(GURL(metadata.active_page().page_url()), GURL(kPageURL));

    ASSERT_TRUE(web::WebStateID::IsValidValue(item_info.identifier()));
    const base::FilePath item_dir = GetOptimizedWebStateDir(
        session_dir,
        web::WebStateID::FromSerializedValue(item_info.identifier()));

    // Check the tab data for correctness.
    web::proto::WebStateStorage item_storage;
    ASSERT_TRUE(ios::sessions::ParseProto(
        item_dir.Append(kWebStateStorageFilename), item_storage));

    EXPECT_FALSE(item_storage.has_metadata());
    EXPECT_EQ(item_storage.navigation().last_committed_item_index(), 0);
    ASSERT_EQ(item_storage.navigation().items_size(), 1);

    const web::proto::NavigationItemStorage& navigation_item =
        item_storage.navigation().items(0);
    EXPECT_EQ(navigation_item.title(), kPageTitle);
    EXPECT_EQ(GURL(navigation_item.virtual_url()), GURL(kPageURL));

    // Check that the native session file exists if created.
    EXPECT_EQ(
        ios::sessions::FileExists(item_dir.Append(kWebStateSessionFilename)),
        tab_info.create_web_session);
  }

  for (size_t index = 0; index < session_info.tab_groups.size(); ++index) {
    const ios::proto::TabGroupStorage& group_storage = storage.groups(index);
    const TabGroupInfo& group_info = session_info.tab_groups[index];

    EXPECT_EQ(group_storage.range().start(), group_info.range_start);
    EXPECT_EQ(group_storage.range().count(), group_info.range_count);
    EXPECT_EQ(group_storage.title(), base::UTF16ToUTF8(group_info.title));
    EXPECT_EQ(group_storage.color(),
              tab_group_util::ColorForStorage(group_info.color));
    EXPECT_EQ(group_storage.collapsed(), group_info.collapsed_state);
    EXPECT_EQ(
        tab_group_util::TabGroupIdFromStorage(group_storage.tab_group_id()),
        group_info.tab_group_id);
  }
}

// Checks whether the legacy session in `root` named `name` corresponds
// to the session described by `session_info`.
void CheckLegacySession(const base::FilePath& root,
                        const std::string& name,
                        SessionInfo session_info) {
  const base::FilePath session_dir = GetLegacySessionDir(root, name);
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);

  // Load the legacy session from disk.
  SessionWindowIOS* session_window = ios::sessions::ReadSessionWindow(
      session_dir.Append(kLegacySessionFilename));
  ASSERT_TRUE(session_window);

  EXPECT_EQ(session_window.selectedIndex,
            SelectedIndexFromActiveIndex(session_info.active_index));

  // Check that the information for each tab is correct.
  ASSERT_EQ(session_window.sessions.count, session_info.tabs.size());
  for (size_t index = 0; index < session_info.tabs.size(); ++index) {
    CRWSessionStorage* session = session_window.sessions[index];
    const TabInfo& tab_info = session_info.tabs[index];

    // Check the opener-opener relationship for correctness.
    CRWSessionUserData* user_data = session.userData;
    if (tab_info.opener_index != -1 && tab_info.opener_navigation_index != -1) {
      EXPECT_NSEQ(base::apple::ObjCCast<NSNumber>([user_data
                      objectForKey:kLegacyWebStateListOpenerIndexKey]),
                  @(tab_info.opener_index));
      EXPECT_NSEQ(
          base::apple::ObjCCast<NSNumber>([user_data
              objectForKey:kLegacyWebStateListOpenerNavigationIndexKey]),
          @(tab_info.opener_navigation_index));
    }

    // Check the pinned status for correctness.
    if (static_cast<int>(index) < session_info.pinned_tab_count) {
      EXPECT_NSEQ(base::apple::ObjCCast<NSNumber>([user_data
                      objectForKey:kLegacyWebStateListPinnedStateKey]),
                  @YES);
    } else {
      EXPECT_NSEQ(base::apple::ObjCCast<NSNumber>([user_data
                      objectForKey:kLegacyWebStateListPinnedStateKey]),
                  nil);
    }

    // Check that a stable identifier was generated (randomized).
    EXPECT_TRUE(session.stableIdentifier);
    EXPECT_NE(session.uniqueIdentifier, web::WebStateID());

    // Check the tab data for correctness.
    EXPECT_EQ(session.lastCommittedItemIndex, 0);
    EXPECT_NE(session.creationTime, base::Time());
    EXPECT_NE(session.lastActiveTime, base::Time());
    ASSERT_EQ(session.itemStorages.count, 1u);

    CRWNavigationItemStorage* navigation_item = session.itemStorages[0];
    EXPECT_EQ(base::UTF16ToUTF8(navigation_item.title), kPageTitle);
    EXPECT_EQ(navigation_item.virtualURL, GURL(kPageURL));

    // Check that the native session file exists if created.
    EXPECT_EQ(ios::sessions::FileExists(GetLegacyWebSessionsFile(
                  web_sessions, session.uniqueIdentifier)),
              tab_info.create_web_session);
  }

  for (size_t index = 0; index < session_info.tab_groups.size(); ++index) {
    SessionTabGroup* group_session = session_window.tabGroups[index];
    const TabGroupInfo& group_info = session_info.tab_groups[index];

    EXPECT_EQ(group_session.rangeStart, group_info.range_start);
    EXPECT_EQ(group_session.rangeCount, group_info.range_count);
    EXPECT_EQ(base::SysNSStringToUTF16(group_session.title), group_info.title);
    EXPECT_EQ(group_session.colorId, static_cast<int>(group_info.color));
    EXPECT_EQ(group_session.collapsedState, group_info.collapsed_state);
    EXPECT_EQ(group_session.tabGroupId, group_info.tab_group_id);
  }
}

}  // namespace

// Tests batch migrating sessions from legacy to optimized works correctly.
TEST_F(SessionMigrationTest, BatchToOptimized) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(identifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the directories containing legacy sessions and WKWebView
  // native session data have been deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }

  // Check that the sessions have been correctly converted.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionInfo2);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from legacy to optimized works correctly
// when there are no sessions.
TEST_F(SessionMigrationTest, BatchToOptimized_NoSession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(identifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the optimized session directories have not been created.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }
}

// Tests batch migrating sessions from legacy to optimized works correctly
// when there are no sessions but empty directory laying around.
TEST_F(SessionMigrationTest, BatchToOptimized_NoSessionEmptyDirs) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Create empty directories.
  for (const base::FilePath& path : paths) {
    using ios::sessions::CreateDirectory;
    ASSERT_TRUE(CreateDirectory(GetLegacySessionDir(path, kSessionName1)));
    ASSERT_TRUE(CreateDirectory(GetLegacySessionDir(path, kSessionName2)));
    ASSERT_TRUE(CreateDirectory(path.Append(kLegacyWebSessionsDirname)));
  }

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(identifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the directories containing legacy sessions and WKWebView
  // native session data have been deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }

  // Check that the optimized session directories have not been created.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }
}

// Tests batch migrating sessions from legacy to optimized works correctly
// and does nothing if the sessions are already in the optimized format.
TEST_F(SessionMigrationTest, BatchToOptimized_SessionAreOptimized) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few optimized sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateOptimizedSession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(identifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the sessions have been left untouched.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionInfo2);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from legacy to optimized works correctly
// and leave unrelated files in the legacy session directories untouched.
TEST_F(SessionMigrationTest, BatchToOptimized_UnrelatedFilesUnaffected) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Create a few unrelated files in the legacy session directories (they
  // corresponds to files saved by //components/sessions).
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    const base::FilePath filename = legacy_dir.Append(kUnrelatedFilename);
    EXPECT_TRUE(ios::sessions::WriteFile(filename, data));
  }

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(identifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the directories containing legacy sessions and WKWebView
  // native session data have been deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    const base::FilePath filename = legacy_dir.Append(kUnrelatedFilename);
    EXPECT_TRUE(ios::sessions::DirectoryExists(legacy_dir));
    EXPECT_NSEQ(ios::sessions::ReadFile(filename), data);

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }

  // Check that the sessions have been correctly converted.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionInfo2);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from legacy to optimized works correctly
// when unique identifiers are invalid and assign new unique identifier.
TEST_F(SessionMigrationTest, BatchToOptimized_InvalidUniqueIdentifiers) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySessionInvalidUniqueIdentifiers(root, kSessionName1,
                                                            kSessionInfo1));
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success and that the tabs from both
  // sessions without identifiers have been assigned new identifiers.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  const int32_t expected_idenfifier =
      identifier +
      static_cast<int>(std::count_if(
          kSessionInfo1.tabs.begin(), kSessionInfo1.tabs.end(),
          [](const TabInfo& tab) { return !tab.create_web_session; }));

  ASSERT_NE(identifier, expected_idenfifier);
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(expected_idenfifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the directories containing legacy sessions and WKWebView
  // native session data have been deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }

  // Check that the sessions have been correctly converted.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionInfo2);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from legacy to optimized when one session
// is invalid and cannot be loaded.
TEST_F(SessionMigrationTest, BatchToOptimized_FailureInvalidSessions) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Write invalid data in one sessions.
  const base::FilePath session_path =
      GetLegacySessionDir(root, kSessionName2).Append(kLegacySessionFilename);
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(session_path, data));

  // Check that the migration is a failure.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Failure(),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the legacy sessions have been left untouched, including the
  // invalid session.
  CheckLegacySession(root, kSessionName1, kSessionInfo1);
  CheckLegacySession(otr, kSessionName1, SessionInfo());
  EXPECT_NSEQ(ios::sessions::ReadFile(session_path), data);

  // Check that the optimized session directory was not created.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }
}

// Tests batch migrating sessions from legacy to optimized when one session
// cannot be migrated.
TEST_F(SessionMigrationTest, BatchToOptimized_FailureMigration) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Create a file with the same name as one of the optimized session
  // directory which should prevent migrating the sessions.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName2);
  EXPECT_TRUE(ios::sessions::WriteFile(dest_dir, data));

  // Check that the migration is a failure.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Failure(),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the legacy sessions have been left untouched.
  CheckLegacySession(root, kSessionName1, kSessionInfo1);
  CheckLegacySession(root, kSessionName2, kSessionInfo2);
  CheckLegacySession(otr, kSessionName1, SessionInfo());

  // Check that the optimized session directory was not created, and
  // that any pre-existing data was deleted.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }
}

// Tests batch migrating sessions from optimized to legacy works correctly.
TEST_F(SessionMigrationTest, BatchToLegacy) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few optimized sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateOptimizedSession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Success(identifier),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the directories containing optimized sessions have been
  // deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }

  // Check that the sessions have been correctly converted.
  CheckLegacySession(root, kSessionName1, kSessionInfo1);
  CheckLegacySession(root, kSessionName2, kSessionInfo2);
  CheckLegacySession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from optimized to legacy works correctly
// when there are no sessions.
TEST_F(SessionMigrationTest, BatchToLegacy_NoSession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Success(identifier),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the legacy session and WKWebView native session data
  // directories have not been created.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }
}

// Tests batch migrating sessions from optimized to legacy works correctly
// when there are no sessions but empty directory laying around.
TEST_F(SessionMigrationTest, BatchToLegacy_NoSessionEmptyDirs) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Create empty directories.
  for (const base::FilePath& path : paths) {
    using ios::sessions::CreateDirectory;
    ASSERT_TRUE(CreateDirectory(GetOptimizedSessionDir(path, kSessionName1)));
    ASSERT_TRUE(CreateDirectory(GetOptimizedSessionDir(path, kSessionName2)));
  }

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Success(identifier),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the directories containing optimized sessions have been
  // deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }

  // Check that the legacy session and WKWebView native session data
  // directories have not been created.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }
}

// Tests batch migrating sessions from optimized to legacy works correctly
// and does nothing if the sessions are already in the legacy format.
TEST_F(SessionMigrationTest, BatchToLegacy_SessionAreLegacy) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Success(identifier),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the sessions have been left untouched.
  CheckLegacySession(root, kSessionName1, kSessionInfo1);
  CheckLegacySession(root, kSessionName2, kSessionInfo2);
  CheckLegacySession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from optimized to legacy when one session
// is invalid and cannot be loaded.
TEST_F(SessionMigrationTest, BatchToLegacy_FailureInvalidSessions) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few optimized sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateOptimizedSession(otr, kSessionName1, SessionInfo()));

  // Write invalid data in one sessions.
  const base::FilePath session_path =
      GetOptimizedSessionDir(root, kSessionName2)
          .Append(kSessionMetadataFilename);
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(session_path, data));

  // Check that the migration is a failure.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Failure(),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the optimized sessions have been left untouched, including the
  // invalid session.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
  EXPECT_NSEQ(ios::sessions::ReadFile(session_path), data);

  // Check that the legacy session and WKWebView native session data
  // directories have not been created.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }
}

// Tests batch migrating sessions from optimized to legacy when one session
// cannot be migrated.
TEST_F(SessionMigrationTest, BatchToLegacy_FailureMigration) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few optimized sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateOptimizedSession(otr, kSessionName1, SessionInfo()));

  // Create a file with the same name as one of the legacy session
  // directory which should prevent migrating the sessions.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath dest_dir = GetLegacySessionDir(otr, kSessionName1);
  EXPECT_TRUE(ios::sessions::WriteFile(dest_dir, data));

  // Check that the migration is a failure.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Failure(),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the optimized sessions have been left untouched.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionInfo2);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());

  // Check that the legacy session and WKWebView native session data
  // directories have not been created and that any pre-existing data
  // was deleted.
  for (const base::FilePath& path : paths) {
    if (path != otr) {
      const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
      EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));
    }

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }

  // Check that the pre-existing invalid file is still present.
  EXPECT_NSEQ(ios::sessions::ReadFile(dest_dir), data);
}

// Tests batch migrating sessions from optimized to legacy when one session
// cannot be migrated, with unrelated files in the legacy session directory
// which should be unaffected.
TEST_F(SessionMigrationTest, BatchToLegacy_FailureUnrelatedFilesUnaffected) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few optimized sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName2, kSessionInfo2));
  EXPECT_TRUE(GenerateOptimizedSession(otr, kSessionName1, SessionInfo()));

  // Create a few unrelated files in the legacy session directories (they
  // corresponds to files saved by //components/sessions).
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    const base::FilePath filename = legacy_dir.Append(kUnrelatedFilename);
    EXPECT_TRUE(ios::sessions::WriteFile(filename, data));
  }

  // Write invalid data in one sessions.
  const base::FilePath session_path = GetOptimizedSessionDir(otr, kSessionName2)
                                          .Append(kSessionMetadataFilename);
  EXPECT_TRUE(ios::sessions::WriteFile(session_path, data));

  // Check that the migration is a failure.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Failure(),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the optimized sessions have been left untouched, including
  // the invalid session.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionInfo2);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
  EXPECT_NSEQ(ios::sessions::ReadFile(session_path), data);

  // Check that the legacy session and WKWebView native session data
  // directories have not been created and that any pre-existing data
  // was deleted.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    const base::FilePath filename = legacy_dir.Append(kUnrelatedFilename);
    EXPECT_TRUE(ios::sessions::DirectoryExists(legacy_dir));
    EXPECT_NSEQ(ios::sessions::ReadFile(filename), data);

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }
}

// Tests batch migrating sessions with tab groups from legacy to optimized works
// correctly.
TEST_F(SessionMigrationTest, BatchToOptimizedWithGroups) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few legacy sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(
      GenerateLegacySession(root, kSessionName2, kSessionWithGroupsInfo));
  EXPECT_TRUE(GenerateLegacySession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(
      ios::sessions::MigrationResult::Success(identifier),
      ios::sessions::MigrateSessionsInPathsToOptimized(paths, identifier));

  // Check that the directories containing legacy sessions and WKWebView
  // native session data have been deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy_dir = path.Append(kLegacySessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(legacy_dir));

    const base::FilePath native_dir = path.Append(kLegacyWebSessionsDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(native_dir));
  }

  // Check that the sessions have been correctly converted.
  CheckOptimizedSession(root, kSessionName1, kSessionInfo1);
  CheckOptimizedSession(root, kSessionName2, kSessionWithGroupsInfo);
  CheckOptimizedSession(otr, kSessionName1, SessionInfo());
}

// Tests batch migrating sessions from optimized to legacy works correctly.
TEST_F(SessionMigrationTest, BatchToLegacyWithGroups) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();
  const base::FilePath otr = root.Append(kOTRDirectory);
  const std::vector<base::FilePath> paths{root, otr};

  // Generate a few optimized sessions for the main and OTR BrowserStates.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName1, kSessionInfo1));
  EXPECT_TRUE(
      GenerateOptimizedSession(root, kSessionName2, kSessionWithGroupsInfo));
  EXPECT_TRUE(GenerateOptimizedSession(otr, kSessionName1, SessionInfo()));

  // Check that the migration is a success.
  const int32_t identifier = web::WebStateID::NewUnique().identifier();
  ASSERT_EQ(ios::sessions::MigrationResult::Success(identifier),
            ios::sessions::MigrateSessionsInPathsToLegacy(paths, identifier));

  // Check that the directories containing optimized sessions have been
  // deleted in all paths.
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized_dir =
        path.Append(kSessionRestorationDirname);
    EXPECT_FALSE(ios::sessions::DirectoryExists(optimized_dir));
  }

  // Check that the sessions have been correctly converted.
  CheckLegacySession(root, kSessionName1, kSessionInfo1);
  CheckLegacySession(root, kSessionName2, kSessionWithGroupsInfo);
  CheckLegacySession(otr, kSessionName1, SessionInfo());
}
