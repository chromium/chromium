// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/web_state_list_serialization.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/sessions/features.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Creates a fake WebState with `navigation_count` navigation items (all
// pointing to the same `url`). If `has_pending_load` is true, the last
// item will be marked as pending.
std::unique_ptr<web::WebState> CreateWebStateWithNavigations(
    int navigation_count,
    bool has_pending_load,
    bool restore_in_progress,
    const GURL& url,
    web::WebStateID web_state_id) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  for (int index = 0; index < navigation_count; ++index) {
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
  }

  if (navigation_count > 0) {
    if (has_pending_load) {
      const int pending_item_index = navigation_count - 1;
      navigation_manager->SetPendingItemIndex(pending_item_index);
      navigation_manager->SetPendingItem(
          navigation_manager->GetItemAtIndex(pending_item_index));
    }
  } else {
    if (restore_in_progress) {
      navigation_manager->SetIsRestoreSessionInProgress(true);
    }
  }

  auto web_state = std::make_unique<web::FakeWebState>(web_state_id);
  web_state->SetNavigationManager(std::move(navigation_manager));
  web_state->SetNavigationItemCount(navigation_count);
  if (navigation_count > 0) {
    web_state->SetVisibleURL(url);
  }

  return web_state;
}

// Creates a fake WebState with some navigations.
std::unique_ptr<web::WebState> CreateWebState() {
  return CreateWebStateWithNavigations(
      1, false, false, GURL(kChromeUIVersionURL), web::WebStateID::NewUnique());
}

// Creates a fake WebState with no navigation items.
std::unique_ptr<web::WebState> CreateWebStateWithNoNavigation(
    web::WebStateID web_state_id = web::WebStateID::NewUnique()) {
  return CreateWebStateWithNavigations(0, false, false, GURL(), web_state_id);
}

// Creates a fake WebState with no navigation items and restoration in progress.
std::unique_ptr<web::WebState> CreateWebStateRestoreSessionInProgress() {
  return CreateWebStateWithNavigations(0, false, true, GURL(),
                                       web::WebStateID::NewUnique());
}

// Creates a fake WebState with no navigation items but one pending item.
std::unique_ptr<web::WebState> CreateWebStateWithPendingNavigation(
    web::NavigationItem* pending_item) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetPendingItem(pending_item);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(std::move(navigation_manager));
  web_state->SetNavigationItemCount(0);

  return web_state;
}

// Creates a fake WebState from `storage`.
std::unique_ptr<web::WebState> CreateWebStateWithSessionStorage(
    CRWSessionStorage* storage) {
  return CreateWebState();
}

// Creates a fake WebState from `web_state_id`.
std::unique_ptr<web::WebState> CreateWebStateWithWebStateID(
    web::WebStateID web_state_id) {
  return CreateWebStateWithNavigations(1, false, false,
                                       GURL(kChromeUIVersionURL), web_state_id);
}

// Creates a fake WebState from serialized data.
std::unique_ptr<web::WebState> CreateWebStateFromProto(
    web::WebStateID web_state_id,
    web::proto::WebStateMetadataStorage metadata) {
  return CreateWebState();
}

// Creates a WebStateMetadataStorage for `web_state`.
web::proto::WebStateMetadataStorage MetadataStorage(web::WebState* web_state) {
  web::proto::WebStateMetadataStorage storage;
  storage.set_navigation_item_count(web_state->GetNavigationItemCount());
  web::SerializeTimeToProto(web_state->GetCreationTime(),
                            *storage.mutable_creation_time());
  web::SerializeTimeToProto(web_state->GetLastActiveTime(),
                            *storage.mutable_last_active_time());

  const std::u16string& title = web_state->GetTitle();
  if (!title.empty()) {
    storage.mutable_active_page()->set_page_title(base::UTF16ToUTF8(title));
  }

  const GURL& url = web_state->GetVisibleURL();
  if (url.is_valid()) {
    storage.mutable_active_page()->set_page_url(url.spec());
  }

  return storage;
}

// Helper wrapping SerializeWebStateList(...) with an auto-generated
// WebStateMetadataMap.
void SerializeWebStateList(const WebStateList& web_state_list,
                           ios::proto::WebStateListStorage& storage) {
  // Create a metadata map for the WebStateList.
  WebStateMetadataMap metadata_map;
  for (int index = 0; index < web_state_list.count(); ++index) {
    web::WebState* const web_state = web_state_list.GetWebStateAt(index);
    const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
    metadata_map.insert(
        std::make_pair(web_state_id, MetadataStorage(web_state)));
  }

  SerializeWebStateList(web_state_list, metadata_map, storage);
}

}  // namespace

// Comparison operators for testing.
inline bool operator==(const WebStateOpener& lhs, const WebStateOpener& rhs) {
  return lhs.opener == rhs.opener &&
         lhs.navigation_index == rhs.navigation_index;
}

inline bool operator!=(const WebStateOpener& lhs, const WebStateOpener& rhs) {
  return lhs.opener != rhs.opener ||
         lhs.navigation_index != rhs.navigation_index;
}

class WebStateListSerializationTest : public PlatformTest {
 protected:
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;
};

// Tests that serializing an empty WebStateList works and results in an
// empty serialized session and the `selectedIndex` is correctly set to
// NSNotFound.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Serialize_ObjC_Empty) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  EXPECT_EQ(session_window.sessions.count, 0u);
  EXPECT_EQ(session_window.selectedIndex, static_cast<NSUInteger>(NSNotFound));
}

// Tests that serializing a WebStateList with some content works, correctly
// recording the opener-opened relationships and the pinned status of all
// tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Serialize_ObjC) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  ASSERT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  for (int i = 0; i < web_state_list.count(); ++i) {
    CRWSessionStorage* session = session_window.sessions[i];
    EXPECT_EQ(session.uniqueIdentifier,
              web_state_list.GetWebStateAt(i)->GetUniqueIdentifier());

    CRWSessionUserData* user_data = session.userData;
    id is_pinned_object =
        [user_data objectForKey:kLegacyWebStateListPinnedStateKey];
    if (web_state_list.IsWebStatePinnedAt(i)) {
      EXPECT_EQ(is_pinned_object, @YES);
    } else {
      EXPECT_EQ(is_pinned_object, nil);
    }

    id opener_index_object =
        [user_data objectForKey:kLegacyWebStateListOpenerIndexKey];
    id opener_navigation_index_object =
        [user_data objectForKey:kLegacyWebStateListOpenerNavigationIndexKey];

    const WebStateOpener opener = web_state_list.GetOpenerOfWebStateAt(i);
    if (!opener.opener) {
      EXPECT_NSEQ(opener_index_object, nil);
      EXPECT_NSEQ(opener_navigation_index_object, nil);
    } else {
      const int opener_index = web_state_list.GetIndexOfWebState(opener.opener);
      ASSERT_NE(opener_index, WebStateList::kInvalidIndex);

      EXPECT_NSEQ(opener_index_object, @(opener_index));
      EXPECT_NSEQ(opener_navigation_index_object, @(opener.navigation_index));
    }
  }
}

// Tests that serializing a WebStateList drops tabs with no navigation items.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Serialize_ObjC_DropNoNavigation) {
  // In production, it is possible to have a real NavigationManager with
  // no navigation item but a pending item; it is not really possible to
  // simulate this with FakeNavigationManager API except by storing the
  // pending NavigationItem outside of the FakeNavigationManager.
  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();

  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0, CreateWebStateRestoreSessionInProgress(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      0, CreateWebStateWithNoNavigation(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      1, CreateWebStateWithNoNavigation(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  web_state_list.InsertWebState(
      2, CreateWebStateWithNoNavigation(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      4, CreateWebStateWithPendingNavigation(pending_item.get()),
      WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(2), 1));

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(session_window.sessions.count, 3u);
  EXPECT_EQ(session_window.selectedIndex, 2u);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 0, 1);
}

// Tests that serializing a WebStateList drops tabs with similar identifiers.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Serialize_ObjC_DropDuplicates) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      1,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      2,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      3,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  web_state_list.InsertWebState(
      4,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      5,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      6,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1111)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  // Check that the duplicate items have been closed, including the active tab
  // (its next kept sibling should be selected).
  EXPECT_EQ(session_window.sessions.count, 3u);
  EXPECT_EQ(session_window.selectedIndex, 2u);

  // Expect a log of 4 duplicates.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 4, 1);
}

// Tests that serializing a WebStateList drops the tab with no navigation when
// also being a duplicate.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Serialize_ObjC_DropNoNavigationAndDuplicate) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web::WebStateID same_web_state_id = web::WebStateID::NewUnique();
  web_state_list.InsertWebState(
      0, CreateWebStateWithNoNavigation(same_web_state_id),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED |
          WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  web_state_list.InsertWebState(
      1, CreateWebStateWithWebStateID(same_web_state_id),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  // Check that the pinned tab got removed, although it was the first occurrence
  // of the duplicates (as it had no navigation).
  EXPECT_EQ(session_window.sessions.count, 1u);
  EXPECT_EQ(session_window.selectedIndex, 0u);
  CRWSessionUserData* user_data = session_window.sessions[0].userData;
  NSNumber* pinned_state = base::apple::ObjCCast<NSNumber>(
      [user_data objectForKey:kLegacyWebStateListPinnedStateKey]);
  EXPECT_FALSE(pinned_state.boolValue);

  // Expect a log of 0 duplicate, because the empty one got removed first.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 0, 1);
}

// Tests that serializing an empty WebStateList works and results in an
// empty serialized session and the `selectedIndex` is correctly set to
// NSNotFound.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Serialize_Proto_Empty) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 0);
  EXPECT_EQ(storage.active_index(), -1);
  EXPECT_EQ(storage.pinned_item_count(), 0);
}

// Tests that serializing a WebStateList with some content works, correctly
// recording the opener-opened relationships and the pinned status of all
// tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Serialize_Proto) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  for (int i = 0; i < web_state_list.count(); ++i) {
    const auto& item_storage = storage.items(i);
    ASSERT_TRUE(web::WebStateID::IsValidValue(item_storage.identifier()));
    EXPECT_EQ(web::WebStateID::FromSerializedValue(item_storage.identifier()),
              web_state_list.GetWebStateAt(i)->GetUniqueIdentifier());

    const WebStateOpener opener = web_state_list.GetOpenerOfWebStateAt(i);
    if (!opener.opener) {
      EXPECT_FALSE(item_storage.has_opener());
    } else {
      const int opener_index = web_state_list.GetIndexOfWebState(opener.opener);
      ASSERT_NE(opener_index, WebStateList::kInvalidIndex);

      ASSERT_TRUE(item_storage.has_opener());
      const auto& opener_storage = item_storage.opener();
      EXPECT_EQ(opener_storage.index(), opener_index);
      EXPECT_EQ(opener_storage.navigation_index(), opener.navigation_index);
    }
  }
}

// Tests that serializing a WebStateList drops tabs with no navigation items.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Serialize_Proto_DropNoNavigation) {
  // In production, it is possible to have a real NavigationManager with
  // no navigation item but a pending item; it is not really possible to
  // simulate this with FakeNavigationManager API except by storing the
  // pending NavigationItem outside of the FakeNavigationManager.
  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();

  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0, CreateWebStateRestoreSessionInProgress(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      0, CreateWebStateWithNoNavigation(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      1, CreateWebStateWithNoNavigation(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  web_state_list.InsertWebState(
      2, CreateWebStateWithNoNavigation(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      4, CreateWebStateWithPendingNavigation(pending_item.get()),
      WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(2), 1));

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(storage.items_size(), 3);
  EXPECT_EQ(storage.active_index(), 2);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 0, 1);
}

// Tests that serializing a WebStateList drops tabs with similar identifiers.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Serialize_Proto_DropDuplicates) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      1,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  web_state_list.InsertWebState(
      2,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      3,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  web_state_list.InsertWebState(
      4,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      5,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  web_state_list.InsertWebState(
      6,
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1111)),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  // Check that the duplicate items have been closed, including the active tab
  // (its next kept sibling should be selected).
  EXPECT_EQ(storage.items_size(), 3);
  EXPECT_EQ(storage.active_index(), 2);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Expect a log of 4 duplicates.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 4, 1);
}

// Tests that serializing a WebStateList drops the tab with no navigation when
// also being a duplicate.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Serialize_Proto_DropNoNavigationAndDuplicate) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web::WebStateID same_web_state_id = web::WebStateID::NewUnique();
  web_state_list.InsertWebState(
      0, CreateWebStateWithNoNavigation(same_web_state_id),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED |
          WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  web_state_list.InsertWebState(
      1, CreateWebStateWithWebStateID(same_web_state_id),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  // Check that not both tabs got removed.
  EXPECT_EQ(storage.items_size(), 1);
  EXPECT_EQ(storage.active_index(), 0);
  EXPECT_EQ(storage.pinned_item_count(), 0);

  // Expect a log of 0 duplicate, because the empty one got removed first.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 0, 1);
}

// Tests deserializing works when support for pinned tabs is enabled.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_PinnedEnabled) {
  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it, and save data to `session_window`.
  SessionWindowIOS* session_window = nil;
  {
    WebStateList web_state_list(&delegate);
    web_state_list.InsertWebState(
        0, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
        WebStateOpener());
    web_state_list.InsertWebState(
        1, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
        WebStateOpener(web_state_list.GetWebStateAt(0), 3));
    web_state_list.InsertWebState(
        2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(0), 2));
    web_state_list.InsertWebState(
        3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(1), 1));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 1);

  // Check the opener-opened relationship.
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(1),
            WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Check that the pinned tab has been restored at the correct position and
  // as pinned.
  EXPECT_TRUE(web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing works when support for pinned tabs is disabled.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_PinnedDisabled) {
  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it, and save data to `session_window`.
  SessionWindowIOS* session_window = nil;
  {
    WebStateList web_state_list(&delegate);
    web_state_list.InsertWebState(
        0, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
        WebStateOpener());
    web_state_list.InsertWebState(
        1, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
        WebStateOpener(web_state_list.GetWebStateAt(0), 3));
    web_state_list.InsertWebState(
        2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(0), 2));
    web_state_list.InsertWebState(
        3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(1), 1));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(1),
            WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Check that the pinned tab has been restored at the correct position
  // but is no longer pinned.
  EXPECT_FALSE(web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing works when support for pinned tabs is enabled.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_PinnedEnabled) {
  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    web_state_list.InsertWebState(
        0, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
        WebStateOpener());
    web_state_list.InsertWebState(
        1, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
        WebStateOpener(web_state_list.GetWebStateAt(0), 3));
    web_state_list.InsertWebState(
        2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(0), 2));
    web_state_list.InsertWebState(
        3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(1), 1));

    SerializeWebStateList(web_state_list, storage);

    EXPECT_EQ(storage.items_size(), 4);
    EXPECT_EQ(storage.active_index(), 1);
    EXPECT_EQ(storage.pinned_item_count(), 1);
  }

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ true,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 1);

  // Check the opener-opened relationship.
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(1),
            WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Check that the pinned tab has been restored at the correct position and
  // as pinned.
  EXPECT_TRUE(web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing works when support for pinned tabs is disabled.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_PinnedDisabled) {
  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    web_state_list.InsertWebState(
        0, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
        WebStateOpener());
    web_state_list.InsertWebState(
        1, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
        WebStateOpener(web_state_list.GetWebStateAt(0), 3));
    web_state_list.InsertWebState(
        2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(0), 2));
    web_state_list.InsertWebState(
        3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(1), 1));

    SerializeWebStateList(web_state_list, storage);

    EXPECT_EQ(storage.items_size(), 4);
    EXPECT_EQ(storage.active_index(), 1);
    EXPECT_EQ(storage.pinned_item_count(), 1);
  }

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ false,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(1),
            WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Check that the pinned tab has been restored at the correct position
  // but is no longer pinned.
  EXPECT_FALSE(web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing works with the kSessionRestorationSessionIDCheck flag
// enabled.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_SessionIDCheck) {
  base::test::ScopedFeatureList feature_list;
  // This test is just here for exercising the code path behind the feature flag
  // (which is just a CHECK). Remove the test when cleaning up the feature.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{session::features::
                                kSessionRestorationSessionIDCheck},
      /*disabled_features=*/{});

  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    web_state_list.InsertWebState(
        0, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
        WebStateOpener());
    web_state_list.InsertWebState(
        1, CreateWebState(),
        WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
        WebStateOpener(web_state_list.GetWebStateAt(0), 3));
    web_state_list.InsertWebState(
        2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(0), 2));
    web_state_list.InsertWebState(
        3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener(web_state_list.GetWebStateAt(1), 1));

    SerializeWebStateList(web_state_list, storage);

    EXPECT_EQ(storage.items_size(), 4);
    EXPECT_EQ(storage.active_index(), 1);
    EXPECT_EQ(storage.pinned_item_count(), 1);
  }

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ false,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(1),
            WebStateOpener(web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(web_state_list.GetWebStateAt(1), 1));

  // Check that the pinned tab has been restored at the correct position
  // but is no longer pinned.
  EXPECT_FALSE(web_state_list.IsWebStatePinnedAt(0));
}
