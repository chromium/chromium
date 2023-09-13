// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_migration.h"

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
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

namespace {

// Information about a single tab.
struct TabInfo {
  const int opener_index = -1;
  const int opener_navigation_index = -1;
  const bool create_web_session = true;
};

// Information about a session.
struct SessionInfo {
  const size_t active_index = 0;
  const size_t pinned_tab_count = 0;
  const base::span<const TabInfo> tabs;
};

// Name of the session used by the tests (random string obtained by
// running `uuidgen` on the command-line, no meaning to it).
const char kSessionName[] = "CB5AC1AF-8E93-407B-AE48-65ECE7C241C0";

// Constants used to populate navigation items.
const char kPageURL[] = "https://www.example.com";
const char kPageTitle[] = "Example Domain";

// Constants representing the default session used for tests.
constexpr TabInfo kTabs[] = {
    TabInfo{},
    TabInfo{},
    TabInfo{.create_web_session = false},
    TabInfo{.opener_index = 2, .opener_navigation_index = 0},
    TabInfo{.opener_index = 2, .opener_navigation_index = 0},
};

constexpr SessionInfo kSessionInfo = {
    .active_index = 1,
    .pinned_tab_count = 2,
    .tabs = base::make_span(kTabs),
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
    const bool is_pinned = index < session_info.pinned_tab_count;
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

  SessionWindowIOS* session =
      [[SessionWindowIOS alloc] initWithSessions:sessions
                                   selectedIndex:session_info.active_index];

  // Write the session file.
  const base::FilePath filename =
      GetLegacySessionDir(root, name).Append(kLegacySessionFilename);
  return ios::sessions::WriteSessionWindow(filename, session);
}

// Creates a WebStateMetadataStorage.
web::proto::WebStateMetadataStorage CreateWebStateMetadataStorage() {
  web::proto::WebStateMetadataStorage storage;
  storage.mutable_active_page()->set_page_title(kPageTitle);
  storage.mutable_active_page()->set_page_url(kPageURL);
  web::SerializeTimeToProto(base::Time::Now(),
                            *storage.mutable_creation_time());
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

    // Write the tab metadata file.
    if (!ios::sessions::WriteProto(
            item_dir.Append(kWebStateMetadataStorageFilename),
            CreateWebStateMetadataStorage())) {
      return false;
    }

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

  // Write the session metadata file.
  const base::FilePath filename = session_dir.Append(kSessionMetadataFilename);
  return ios::sessions::WriteProto(filename, storage);
}

}  // namespace

// Tests migrating a session from legacy to optimized works correctly.
TEST_F(SessionMigrationTest, ToOptimized) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Generate a legacy session.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName, kSessionInfo));

  // Ask to migrate the legacy session.
  FakeTabRestoreService restore_service;
  ios::sessions::MigrateNamedSessionToOptimized(root, kSessionName,
                                                &restore_service);

  // Check that the tabs have not been recorded.
  EXPECT_EQ(restore_service.entries().size(), 0u);

  // Check that the legacy session directory was deleted and its parent too
  // since it was empty.
  const base::FilePath from_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir.DirName()));

  // Check that the directory containing the web sessions was also deleted
  // as it was empty after the migration.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_FALSE(ios::sessions::DirectoryExists(web_sessions));

  // Check that an optimized session was created.
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::DirectoryExists(dest_dir));

  // Load the session metadata and check that it agrees with `kSessionInfo`.
  ios::proto::WebStateListStorage storage;
  EXPECT_TRUE(ios::sessions::ParseProto(
      dest_dir.Append(kSessionMetadataFilename), storage));

  EXPECT_EQ(storage.active_index(),
            static_cast<int>(kSessionInfo.active_index));
  EXPECT_EQ(storage.pinned_item_count(),
            static_cast<int>(kSessionInfo.pinned_tab_count));

  // Check that the information for each tab is correct.
  ASSERT_EQ(storage.items_size(), static_cast<int>(kSessionInfo.tabs.size()));
  for (size_t index = 0; index < kSessionInfo.tabs.size(); ++index) {
    const ios::proto::WebStateListItemStorage& item_info = storage.items(index);
    const TabInfo& tab_info = kSessionInfo.tabs[index];

    if (tab_info.opener_index != -1 && tab_info.opener_navigation_index != -1) {
      EXPECT_TRUE(item_info.has_opener());
      EXPECT_EQ(item_info.opener().index(), tab_info.opener_index);
      EXPECT_EQ(item_info.opener().navigation_index(),
                tab_info.opener_navigation_index);
    }

    ASSERT_TRUE(web::WebStateID::IsValidValue(item_info.identifier()));
    const base::FilePath item_dir = GetOptimizedWebStateDir(
        dest_dir, web::WebStateID::FromSerializedValue(item_info.identifier()));

    // Load the tab metadata and check for correctness.
    web::proto::WebStateMetadataStorage metadata;
    EXPECT_TRUE(ios::sessions::ParseProto(
        item_dir.Append(kWebStateMetadataStorageFilename), metadata));

    EXPECT_TRUE(metadata.has_active_page());
    EXPECT_EQ(metadata.navigation_item_count(), 1);
    EXPECT_EQ(metadata.active_page().page_title(), kPageTitle);
    EXPECT_EQ(GURL(metadata.active_page().page_url()), GURL(kPageURL));

    // Load the tab data and check for correctnesss.
    web::proto::WebStateStorage item_storage;
    EXPECT_TRUE(ios::sessions::ParseProto(
        item_dir.Append(kWebStateStorageFilename), item_storage));

    EXPECT_FALSE(item_storage.has_metadata());
    EXPECT_EQ(item_storage.navigation().last_committed_item_index(), 0);
    ASSERT_EQ(item_storage.navigation().items_size(), 1);

    const auto& navigation_item = item_storage.navigation().items(0);
    EXPECT_EQ(navigation_item.title(), kPageTitle);
    EXPECT_EQ(GURL(navigation_item.virtual_url()), GURL(kPageURL));

    // Check that the web session file exists (if created).
    EXPECT_EQ(
        ios::sessions::FileExists(item_dir.Append(kWebStateSessionFilename)),
        tab_info.create_web_session);
  }
}

// Tests migrating a session from legacy to optimized when there is no session.
TEST_F(SessionMigrationTest, ToOptimized_NoSession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Ask to migrate the non-existent legacy session.
  ios::sessions::MigrateNamedSessionToOptimized(root, kSessionName, nullptr);

  // Check that no optimized session was created.
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));
}

// Tests migrating a session from legacy to optimized when there is no session
// but empty directories laying around.
TEST_F(SessionMigrationTest, ToOptimized_NoSessionEmptyDirs) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create empty directories.
  const base::FilePath from_dir = GetLegacySessionDir(root, kSessionName);
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_TRUE(ios::sessions::CreateDirectory(from_dir));
  EXPECT_TRUE(ios::sessions::CreateDirectory(web_sessions));

  // Ask to migrate the non-existent legacy session.
  ios::sessions::MigrateNamedSessionToOptimized(root, kSessionName, nullptr);

  // Check that no optimized session was created.
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the empty directories have been deleted.
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));
  EXPECT_FALSE(ios::sessions::DirectoryExists(web_sessions));
}

// Tests migrating a session from legacy to optimized when the session is
// invalid and cannot be loaded.
TEST_F(SessionMigrationTest, ToOptimized_FailureInvalidSessions) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write incorrect data to the session file.
  const base::FilePath from_dir = GetLegacySessionDir(root, kSessionName);
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(
      ios::sessions::WriteFile(from_dir.Append(kLegacySessionFilename), data));

  // Create some fake web sessions.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_TRUE(ios::sessions::WriteFile(
      GetLegacyWebSessionsFile(web_sessions, web::WebStateID::NewUnique()),
      data));
  EXPECT_TRUE(ios::sessions::WriteFile(
      GetLegacyWebSessionsFile(web_sessions, web::WebStateID::NewUnique()),
      data));

  // Ask to migrate the invalid legacy session.
  ios::sessions::MigrateNamedSessionToOptimized(root, kSessionName, nullptr);

  // Check that no optimized session was created.
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the session directory has been deleted.
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));

  // Check that the web sessions files have not been touched.
  EXPECT_TRUE(ios::sessions::DirectoryExists(web_sessions));
  EXPECT_FALSE(ios::sessions::DirectoryEmpty(web_sessions));
}

// Tests migrating a session from legacy to optimized when the session is
// valid and cannot be migrated.
TEST_F(SessionMigrationTest, ToOptimized_FailureMigration) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Generate a legacy session.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName, kSessionInfo));

  // Create a file with the same name as the optimized session directory
  // which should prevent migrating the session.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::WriteFile(dest_dir, data));

  // Ask to migrate the legacy session.
  ios::sessions::MigrateNamedSessionToOptimized(root, kSessionName, nullptr);

  // Check that no optimized session was created.
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the session directory has been deleted.
  const base::FilePath from_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));

  // Check that the web sessions directory has been deleted.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_FALSE(ios::sessions::DirectoryExists(web_sessions));
}

// Tests migrating a session from legacy to optimized when the session is
// valid and cannot be migrated and that tabs are recorded to the restore
// service.
TEST_F(SessionMigrationTest, ToOptimized_FailureMigrationRecordTabs) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Generate a legacy session.
  EXPECT_TRUE(GenerateLegacySession(root, kSessionName, kSessionInfo));

  // Create a file with the same name as the optimized session directory
  // which should prevent migrating the session.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath dest_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::WriteFile(dest_dir, data));

  // Ask to migrate the legacy session.
  FakeTabRestoreService restore_service;
  ios::sessions::MigrateNamedSessionToOptimized(root, kSessionName,
                                                &restore_service);

  // Check that no optimized session was created.
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the tabs have been recorded.
  EXPECT_EQ(restore_service.entries().size(), kSessionInfo.tabs.size());
}

// Tests migrating a session from optimize to legacy works correctly.
TEST_F(SessionMigrationTest, ToLegacy) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Generate an optimized session.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName, kSessionInfo));

  // Ask to migrate the session.
  FakeTabRestoreService restore_service;
  ios::sessions::MigrateNamedSessionToLegacy(root, kSessionName,
                                             &restore_service);

  // Check that the tabs have not been recorded.
  EXPECT_EQ(restore_service.entries().size(), 0u);

  // Check that the optimized session directory was deleted and its parent too
  // since it was empty.
  const base::FilePath from_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir.DirName()));

  // Check that an optimized session was created.
  const base::FilePath dest_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the directory containing the web sessions was also created
  // and is not empty.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_TRUE(ios::sessions::DirectoryExists(web_sessions));
  EXPECT_FALSE(ios::sessions::DirectoryEmpty(web_sessions));

  // Load the session and check that it agress with `kSessionInfo`.
  SessionWindowIOS* session_window =
      ios::sessions::ReadSessionWindow(dest_dir.Append(kLegacySessionFilename));
  ASSERT_TRUE(session_window);

  EXPECT_EQ(session_window.selectedIndex, kSessionInfo.active_index);

  // Check that the information for each tab is correct.
  ASSERT_EQ(session_window.sessions.count, kSessionInfo.tabs.size());
  for (size_t index = 0; index < kSessionInfo.tabs.size(); ++index) {
    CRWSessionStorage* session = session_window.sessions[index];
    const TabInfo& tab_info = kSessionInfo.tabs[index];

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

    if (index < kSessionInfo.pinned_tab_count) {
      EXPECT_NSEQ(base::apple::ObjCCast<NSNumber>([user_data
                      objectForKey:kLegacyWebStateListPinnedStateKey]),
                  @YES);
    }

    // Check that a stable identifier was generated (randomized).
    EXPECT_TRUE(session.stableIdentifier);

    // Check the data for correctness.
    EXPECT_EQ(session.lastCommittedItemIndex, 0);
    EXPECT_NE(session.creationTime, base::Time());
    ASSERT_EQ(session.itemStorages.count, 1u);

    CRWNavigationItemStorage* navigation_item = session.itemStorages[0];
    EXPECT_EQ(base::UTF16ToUTF8(navigation_item.title), kPageTitle);
    EXPECT_EQ(navigation_item.virtualURL, GURL(kPageURL));

    // Check that the web session file exists (if created).
    EXPECT_EQ(ios::sessions::FileExists(GetLegacyWebSessionsFile(
                  web_sessions, session.uniqueIdentifier)),
              tab_info.create_web_session);
  }
}

// Tests migrating a session from optimized to legacy when there is no session.
TEST_F(SessionMigrationTest, ToLegacy_NoSession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Ask to migrate the non-existent optimized session.
  ios::sessions::MigrateNamedSessionToLegacy(root, kSessionName, nullptr);

  // Check that no legacy session was created.
  const base::FilePath dest_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the web sessions directory was not created.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_FALSE(ios::sessions::DirectoryExists(web_sessions));
}

// Tests migrating a session from optimized to legacy when there is no session
// but empty directories laying around.
TEST_F(SessionMigrationTest, ToLegacy_NoSessionEmptyDirs) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create empty directory.
  const base::FilePath from_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::CreateDirectory(from_dir));

  // Ask to migrate the optimized session.
  ios::sessions::MigrateNamedSessionToLegacy(root, kSessionName, nullptr);

  // Check that no optimized session was created.
  const base::FilePath dest_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the web sessions directory was not created.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_FALSE(ios::sessions::DirectoryExists(web_sessions));

  // Check that the empty directories have been deleted.
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));
}

// Tests migrating a session from optimized to legacy when the session is
// invalid and cannot be loaded.
TEST_F(SessionMigrationTest, ToLegacy_FailureInvalidSessions) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write incorrect data to the session file.
  const base::FilePath from_dir = GetOptimizedSessionDir(root, kSessionName);
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(
      from_dir.Append(kSessionMetadataFilename), data));

  // Ask to migrate the invalid optimized session.
  ios::sessions::MigrateNamedSessionToLegacy(root, kSessionName, nullptr);

  // Check that no legacy session was created.
  const base::FilePath dest_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the session directory has been deleted.
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));
}

// Tests migrating a session from optimized to legacy when the session is
// valid and cannot be migrated.
TEST_F(SessionMigrationTest, ToLegacy_FailureMigration) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Generate an optimized session.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName, kSessionInfo));

  // Create a file with the same name as the legacy session directory
  // which should prevent migrating the session.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath dest_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::WriteFile(dest_dir, data));

  // Ask to migrate the session.
  ios::sessions::MigrateNamedSessionToLegacy(root, kSessionName, nullptr);

  // Check that no legacy session was created.
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the session directory has been deleted.
  const base::FilePath from_dir = GetOptimizedSessionDir(root, kSessionName);
  EXPECT_FALSE(ios::sessions::DirectoryExists(from_dir));

  // Check that the web sessions directory has not been created.
  const base::FilePath web_sessions = root.Append(kLegacyWebSessionsDirname);
  EXPECT_FALSE(ios::sessions::DirectoryExists(web_sessions));
}

// Tests migrating a session from optimized to legacy when the session is
// valid and cannot be migrated and that tabs are recorded to the restore
// service.
TEST_F(SessionMigrationTest, ToLegacy_FailureMigrationRecordTabs) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Generate an optimized session.
  EXPECT_TRUE(GenerateOptimizedSession(root, kSessionName, kSessionInfo));

  // Create a file with the same name as the legacy session directory
  // which should prevent migrating the session.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath dest_dir = GetLegacySessionDir(root, kSessionName);
  EXPECT_TRUE(ios::sessions::WriteFile(dest_dir, data));

  // Ask to migrate the session.
  FakeTabRestoreService restore_service;
  ios::sessions::MigrateNamedSessionToLegacy(root, kSessionName,
                                             &restore_service);

  // Check that no legacy session was created.
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that the tabs have been recorded.
  EXPECT_EQ(restore_service.entries().size(), kSessionInfo.tabs.size());
}
