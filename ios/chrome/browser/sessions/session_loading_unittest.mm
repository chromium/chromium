// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_loading.h"

#import "base/containers/span.h"
#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/stringprintf.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/sessions/proto_util.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Information about a single tab.
//
// The default constructor represents a valid tab without opener.
struct TabInfo {
  const int opener_index = -1;
  const int opener_navigation_index = -1;
  const int navigation_item_count = 1;
  const web::WebStateID unique_identifier;
};

// Information about a session.
//
// The default constructor represents a valid session without tabs.
struct SessionInfo {
  const int active_index = -1;
  const int pinned_tab_count = 0;
  const base::span<const TabInfo> tabs;
};

// Constants representing the default session used for tests.
constexpr TabInfo kTabs[] = {
    TabInfo{},
    TabInfo{},
    TabInfo{},
    TabInfo{.opener_index = 1, .opener_navigation_index = 1},
    TabInfo{.opener_index = 3, .opener_navigation_index = 1},
};

constexpr SessionInfo kSessionInfo = {
    .active_index = 1,
    .pinned_tab_count = 2,
    .tabs = base::make_span(kTabs),
};

// Returns a test URL (as a string) for a item with `identifier`.
std::string TestUrlForIdentifier(web::WebStateID identifier) {
  return base::StringPrintf("https://example.com/%d", identifier.identifier());
}

// Creates a WebStateListStorage from a SessionInfo.
ios::proto::WebStateListStorage StorageFromSessionInfo(SessionInfo info) {
  DCHECK_LT(info.active_index, static_cast<int>(info.tabs.size()));
  DCHECK_LE(info.pinned_tab_count, static_cast<int>(info.tabs.size()));

  ios::proto::WebStateListStorage storage;
  storage.set_active_index(info.active_index);
  storage.set_pinned_item_count(info.pinned_tab_count);
  for (const TabInfo& tab : info.tabs) {
    ios::proto::WebStateListItemStorage* item_storage = storage.add_items();
    web::WebStateID web_state_id = tab.unique_identifier.valid()
                                       ? tab.unique_identifier
                                       : web::WebStateID::NewUnique();
    item_storage->set_identifier(web_state_id.identifier());
    if (tab.opener_index != -1 && tab.opener_navigation_index != -1) {
      DCHECK_GE(tab.opener_index, 0);
      DCHECK_LT(tab.opener_index, static_cast<int>(info.tabs.size()));

      ios::proto::OpenerStorage* opener_storage =
          item_storage->mutable_opener();

      opener_storage->set_index(tab.opener_index);
      opener_storage->set_navigation_index(tab.opener_navigation_index);
    }
  }
  return storage;
}

// Writes the session storage to disk at `path` optionally marking some
// items as having no navigation items (so that they are dropped during
// load). Returns whether the operation was successful.
bool WriteSessionStorage(const base::FilePath& path,
                         ios::proto::WebStateListStorage storage,
                         RemovingIndexes removing_indexes) {
  const base::FilePath metadata_path = path.Append(kSessionMetadataFilename);
  bool success = ios::sessions::WriteProto(metadata_path, storage);

  const int item_size = storage.items_size();
  for (int index = 0; index < item_size; ++index) {
    const web::WebStateID item_id =
        web::WebStateID::FromSerializedValue(storage.items(index).identifier());

    const base::FilePath item_dir =
        ios::sessions::WebStateDirectory(path, item_id);

    // Write the item metadata (with a navigation item count of zero if
    // the index is contained in `removing_indexes`).
    const base::FilePath item_metadata_path =
        item_dir.Append(kWebStateMetadataStorageFilename);

    web::proto::WebStateMetadataStorage item_metadata;
    if (!removing_indexes.Contains(index)) {
      item_metadata.set_navigation_item_count(1);
      item_metadata.mutable_active_page()->set_page_url(
          TestUrlForIdentifier(item_id));
    }

    success |= ios::sessions::WriteProto(item_metadata_path, item_metadata);

    // Write the item storage (can be a default protobuf message since the
    // value is never read by LoadSessionStorage, only the presence of the
    // file is checked).
    const base::FilePath item_storage_path =
        item_dir.Append(kWebStateStorageFilename);

    web::proto::WebStateStorage item_storage;
    success |= ios::sessions::WriteProto(item_storage_path, item_storage);
  }

  return success;
}

}  // namespace

class SessionLoadingTest : public PlatformTest {
 protected:
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;
};

// Tests that WebStateDirectory returns a correct value that depends on the
// identifier and is a sub-directory of the input.
TEST_F(SessionLoadingTest, WebStateDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create two distinct WebStateID values.
  const web::WebStateID id1 = web::WebStateID::NewUnique();
  const web::WebStateID id2 = web::WebStateID::NewUnique();
  ASSERT_NE(id1, id2);

  // Check that the directories are distinct and are below `root`.
  const base::FilePath dir1 = ios::sessions::WebStateDirectory(root, id1);
  const base::FilePath dir2 = ios::sessions::WebStateDirectory(root, id2);

  // Returned directories should be below `root`.
  EXPECT_TRUE(root.IsParent(dir1));
  EXPECT_TRUE(root.IsParent(dir2));

  // Returned directies should be distinct if ids are distincts.
  EXPECT_NE(dir1, dir2);

  // Returned directory must only depend on `root` and the identifier.
  EXPECT_EQ(dir1, ios::sessions::WebStateDirectory(root, id1));
  EXPECT_EQ(dir2, ios::sessions::WebStateDirectory(root, id2));
}

// Tests that FilterItems correctly remove items according to RemovingIndexes
// and updates the WebStateListStorage invariants.
TEST_F(SessionLoadingTest, FilterItems) {
  const ios::proto::WebStateListStorage storage =
      StorageFromSessionInfo(kSessionInfo);

  // Check that the session is not modified if no item is closed.
  EXPECT_EQ(storage, ios::sessions::FilterItems(storage, RemovingIndexes{}));

  // Check that closing all items return an empty session.
  const ios::proto::WebStateListStorage empty_storage =
      StorageFromSessionInfo(SessionInfo{});
  EXPECT_EQ(empty_storage, ios::sessions::FilterItems(
                               storage, RemovingIndexes{0, 1, 2, 3, 4}));

  // Check that the opener indexes are correctly updated when removing
  // or changing indexes of openers.
  const ios::proto::WebStateListStorage filtered =
      ios::sessions::FilterItems(storage, RemovingIndexes{1, 2});

  EXPECT_EQ(filtered.active_index(), 1);
  EXPECT_EQ(filtered.pinned_item_count(), 1);
  ASSERT_EQ(filtered.items_size(), 3);

  EXPECT_EQ(filtered.items(0).identifier(), storage.items(0).identifier());
  EXPECT_EQ(filtered.items(1).identifier(), storage.items(3).identifier());
  EXPECT_EQ(filtered.items(2).identifier(), storage.items(4).identifier());

  // The opener of tab 3 was closed, so the corresponding item (at index 1)
  // should have no opener.
  EXPECT_FALSE(filtered.items(1).has_opener());

  // The opener of tab 4 was moved to index 1, so the opener index should
  // have been updated.
  EXPECT_EQ(filtered.items(2).opener().index(), 1);
  EXPECT_EQ(filtered.items(2).opener().navigation_index(), 1);
}

// Tests that LoadSessionStorage correctly load a valid session.
TEST_F(SessionLoadingTest, LoadSessionStorage) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write the session described by kSessionInfo.
  const ios::proto::WebStateListStorage session =
      StorageFromSessionInfo(kSessionInfo);
  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{}));

  // Load the session and check it is correct.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata, session);

  for (const auto& item : loaded.session_metadata.items()) {
    const web::WebStateID item_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    // Check that the item has been correctly loaded.
    auto iterator = loaded.web_state_storage_map.find(item_id);
    ASSERT_TRUE(iterator != loaded.web_state_storage_map.end());
    EXPECT_EQ(iterator->second.active_page().page_url(),
              TestUrlForIdentifier(item_id));
  }

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0, 1);
}

// Tests that LoadSessionStorage correctly loads a valid session, removing
// items with no navigation item count.
TEST_F(SessionLoadingTest, LoadSessionStorage_FilterEmptyItems) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write the session described by kSessionInfo.
  const ios::proto::WebStateListStorage session =
      StorageFromSessionInfo(kSessionInfo);
  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{0, 2}));

  // Load the session and check it is correct.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata,
            ios::sessions::FilterItems(session, RemovingIndexes({0, 2})));

  for (const auto& item : loaded.session_metadata.items()) {
    const web::WebStateID item_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    // Check that the item has been correctly loaded.
    auto iterator = loaded.web_state_storage_map.find(item_id);
    ASSERT_TRUE(iterator != loaded.web_state_storage_map.end());
    EXPECT_EQ(iterator->second.active_page().page_url(),
              TestUrlForIdentifier(item_id));
  }

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0, 1);
}

// Tests that LoadSessionStorage correctly loads a valid session, removing
// duplicates (keeping the first occurrence).
TEST_F(SessionLoadingTest, LoadSessionStorage_FilterDuplicateItems) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  web::WebStateID same_web_state_id = web::WebStateID::NewUnique();
  TabInfo tabs[] = {
      TabInfo{.unique_identifier = same_web_state_id},
      TabInfo{.unique_identifier = same_web_state_id},
  };
  SessionInfo session_info = {
      .active_index = 1,
      .pinned_tab_count = 1,
      .tabs = base::make_span(tabs),
  };

  // Write the session described by session_info.
  const ios::proto::WebStateListStorage session =
      StorageFromSessionInfo(session_info);
  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{}));

  // Load the session and check it is correct.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  // Check that the duplicate tab (the second occurrence) is dropped.
  EXPECT_EQ(loaded.session_metadata,
            ios::sessions::FilterItems(session, RemovingIndexes({1})));

  for (const auto& item : loaded.session_metadata.items()) {
    const web::WebStateID item_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    // Check that the item has been correctly loaded.
    auto iterator = loaded.web_state_storage_map.find(item_id);
    ASSERT_TRUE(iterator != loaded.web_state_storage_map.end());
    EXPECT_EQ(iterator->second.active_page().page_url(),
              TestUrlForIdentifier(item_id));
  }

  // Expect a log of 1 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 1, 1);
}

// Tests that LoadSessionStorage correctly load an empty session.
TEST_F(SessionLoadingTest, LoadSessionStorage_EmptySession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write the session described by an empty SessionInfo object.
  const ios::proto::WebStateListStorage session =
      StorageFromSessionInfo(SessionInfo{});
  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{}));

  // Load the session and check it is correct.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata, session);
  EXPECT_EQ(loaded.session_metadata.items_size(), 0);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0, 1);
}

// Tests that LoadSessionStorage returns an empty session if the session is
// missing.
TEST_F(SessionLoadingTest, LoadSessionStorage_MissingSession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Load the session and check it is empty.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata, StorageFromSessionInfo(SessionInfo{}));
  EXPECT_EQ(loaded.session_metadata.items_size(), 0);

  // Expect no log as there was no session. We never got to complete the
  // filtering stage.
  histogram_tester_.ExpectTotalCount(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0);
}

// Tests that LoadSessionStorage returns an empty session if some of the
// items metadata is missing.
TEST_F(SessionLoadingTest, LoadSessionStorage_MissingItemMetadata) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write the session described by kSessionInfo.
  const ios::proto::WebStateListStorage session =
      StorageFromSessionInfo(kSessionInfo);
  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{}));

  // Delete one item metadata.
  ASSERT_GT(session.items_size(), 0);
  const web::WebStateID item_id =
      web::WebStateID::FromSerializedValue(session.items(0).identifier());

  const base::FilePath item_metadata_path =
      ios::sessions::WebStateDirectory(root, item_id)
          .Append(kWebStateMetadataStorageFilename);

  ASSERT_TRUE(ios::sessions::DeleteRecursively(item_metadata_path));

  // Load the session and check it is empty.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata, StorageFromSessionInfo(SessionInfo{}));
  EXPECT_EQ(loaded.session_metadata.items_size(), 0);

  // Expect no log as there was no item metadata. We never got to complete the
  // filtering stage.
  histogram_tester_.ExpectTotalCount(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0);
}

// Tests that LoadSessionStorage returns an empty session if some of the
// items data is missing.
TEST_F(SessionLoadingTest, LoadSessionStorage_MissingItemStorage) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write the session described by kSessionInfo.
  const ios::proto::WebStateListStorage session =
      StorageFromSessionInfo(kSessionInfo);
  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{}));

  // Delete one item storage.
  ASSERT_GT(session.items_size(), 0);
  const web::WebStateID item_id =
      web::WebStateID::FromSerializedValue(session.items(0).identifier());

  const base::FilePath item_metadata_path =
      ios::sessions::WebStateDirectory(root, item_id)
          .Append(kWebStateStorageFilename);

  ASSERT_TRUE(ios::sessions::DeleteRecursively(item_metadata_path));

  // Load the session and check it is empty.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata, StorageFromSessionInfo(SessionInfo{}));
  EXPECT_EQ(loaded.session_metadata.items_size(), 0);

  // Expect no log as there was a missing item storage. We never got to complete
  // the filtering stage.
  histogram_tester_.ExpectTotalCount(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0);
}

// Tests that LoadSessionStorage returns an empty session if the identifiers
// are invalid (corrupt session).
TEST_F(SessionLoadingTest, LoadSessionStorage_InvalidIdentifiers) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Write a corrupt session.
  const web::WebStateID invalid_id = web::WebStateID();
  ASSERT_FALSE(invalid_id.valid());

  ios::proto::WebStateListStorage session;
  session.add_items()->set_identifier(invalid_id.identifier());
  session.set_active_index(0);
  ASSERT_EQ(session.items_size(), 1);

  ASSERT_TRUE(WriteSessionStorage(root, session, RemovingIndexes{}));

  // Load the session and check it is correct.
  const ios::sessions::SessionStorage loaded =
      ios::sessions::LoadSessionStorage(root);
  EXPECT_EQ(loaded.session_metadata, StorageFromSessionInfo(SessionInfo{}));
  EXPECT_EQ(loaded.session_metadata.items_size(), 0);

  // Expect no log as there was an invalid identifier. We never got to complete
  // the filtering stage.
  histogram_tester_.ExpectTotalCount(
      "Tabs.DroppedDuplicatesCountOnSessionRestore", 0);
}
