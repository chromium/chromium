// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/web_state_list_serialization.h"

#import <memory>
#import <ostream>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
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

// Creates a fake WebState on NTP. If `has_pending_load`, then the last
// item is marked as pending.
std::unique_ptr<web::WebState> CreateWebStateOnNTP(bool has_pending_load) {
  return CreateWebStateWithNavigations(1, has_pending_load, false,
                                       GURL(kChromeUINewTabURL),
                                       web::WebStateID::NewUnique());
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

// Returns the unique identifier for WebState at `index` in `web_state_list`.
web::WebStateID IdentifierAt(const WebStateList& web_state_list, int index) {
  return web_state_list.GetWebStateAt(index)->GetUniqueIdentifier();
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

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is enabled and the scope includes all tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_PinnedEnabled_All) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(restored_web_state_list.count(), 5);
  EXPECT_EQ(restored_web_state_list.active_index(), 2);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 1);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(restored_web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(4),
            WebStateOpener(restored_web_state_list.GetWebStateAt(2), 1));

  // Check that the pinned tab has been restored at the correct position and
  // as pinned.
  EXPECT_TRUE(restored_web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is enabled and the scope includes only regular tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_ObjC_PinnedEnabled_RegularOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(1, restored_web_state_list.count());

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kRegularOnly,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 3u);

  ASSERT_EQ(restored_web_state_list.count(), 4);
  EXPECT_EQ(restored_web_state_list.active_index(), 1);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 1));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is enabled and the scope includes only pinned tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_ObjC_PinnedEnabled_PinnedOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kPinnedOnly,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 2);
  EXPECT_EQ(restored_web_state_list.active_index(), 1);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 1);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());

  // Check that the pinned tab has been restored at the correct position and
  // as pinned.
  EXPECT_TRUE(restored_web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is disabled and the scope includes all tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_PinnedDisabled_All) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(restored_web_state_list.count(), 5);
  EXPECT_EQ(restored_web_state_list.active_index(), 2);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 3));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 2));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(4),
            WebStateOpener(restored_web_state_list.GetWebStateAt(2), 1));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is disabled and the scope includes only regular tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_ObjC_PinnedDisabled_RegularOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(1, restored_web_state_list.count());

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kRegularOnly,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(restored_web_state_list.count(), 5);
  EXPECT_EQ(restored_web_state_list.active_index(), 2);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 3));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 2));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(4),
            WebStateOpener(restored_web_state_list.GetWebStateAt(2), 1));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is disabled and the scope includes only pinned tabs.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_ObjC_PinnedDisabled_PinnedOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kPinnedOnly,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 0u);

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_EQ(restored_web_state_list.active_index(), 0);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);
}

// Tests deserializing into a non-empty WebStateList containing a single
// WebState displaying the NTP and without pending navigation leads to
// closing the old NTP tab.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_SingleTabNTP) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 1u);
  EXPECT_EQ(session_window.selectedIndex, 0u);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  // Record the WebStateID of the WebState displaying the NTP.
  const auto ntp_web_state_id = IdentifierAt(restored_web_state_list, 0);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_NE(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id);
}

// Tests deserializing into a non-empty WebStateList containing a single
// WebState displaying the NTP and with a pending navigation does not
// cause the tab to be closed.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_ObjC_SingleTabNTP_PendingNavigation) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 1u);
  EXPECT_EQ(session_window.selectedIndex, 0u);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ true),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  // Record the WebStateID of the WebState displaying the NTP.
  const auto ntp_web_state_id = IdentifierAt(restored_web_state_list, 0);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 2);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id);
}

// Tests deserializing an empty session into a non-empty WebStateList
// containing a single WebState displaying the NTP and without pending
// does not cause the tab to be closed.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_ObjC_SingleTabNTP_EmptySession) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 0u);
  EXPECT_EQ(session_window.selectedIndex, static_cast<NSUInteger>(NSNotFound));

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  // Record the WebStateID of the WebState displaying the NTP.
  const auto ntp_web_state_id = IdentifierAt(restored_web_state_list, 0);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 0u);

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id);
}

// Tests deserializing into a non-empty WebStateList containing multiple
// WebStates does not lead to closing those tabs, even if they all display
// the NTP and have no pending navigation.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC__MultipleNTPTabs) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(session_window.sessions.count, 1u);
  EXPECT_EQ(session_window.selectedIndex, 0u);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  restored_web_state_list.InsertWebState(
      1, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 2);

  // Record the WebStateIDs of the WebStates displaying the NTP.
  const auto ntp_web_state_id_0 = IdentifierAt(restored_web_state_list, 0);
  const auto ntp_web_state_id_1 = IdentifierAt(restored_web_state_list, 1);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, session_window,
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 3);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id_0);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 1), ntp_web_state_id_1);
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is enabled and the scope includes all tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_PinnedEnabled_All) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(restored_web_state_list.count(), 5);
  EXPECT_EQ(restored_web_state_list.active_index(), 2);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 1);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(restored_web_state_list.GetWebStateAt(0), 3));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(0), 2));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(4),
            WebStateOpener(restored_web_state_list.GetWebStateAt(2), 1));

  // Check that the pinned tab has been restored at the correct position and
  // as pinned.
  EXPECT_TRUE(restored_web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is enabled and the scope includes only regular tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_PinnedEnabled_RegularOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kRegularOnly,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 3u);

  ASSERT_EQ(restored_web_state_list.count(), 4);
  EXPECT_EQ(restored_web_state_list.active_index(), 1);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 1));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is enabled and the scope includes only pinned tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_PinnedEnabled_PinnedOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kPinnedOnly,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 2);
  EXPECT_EQ(restored_web_state_list.active_index(), 1);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 1);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());

  // Check that the pinned tab has been restored at the correct position and
  // as pinned.
  EXPECT_TRUE(restored_web_state_list.IsWebStatePinnedAt(0));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is disabled and the scope includes all tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_PinnedDisabled_All) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(restored_web_state_list.count(), 5);
  EXPECT_EQ(restored_web_state_list.active_index(), 2);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 3));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 2));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(4),
            WebStateOpener(restored_web_state_list.GetWebStateAt(2), 1));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is disabled and the scope includes only regular tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_PinnedDisabled_RegularOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kRegularOnly,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(restored_web_state_list.count(), 5);
  EXPECT_EQ(restored_web_state_list.active_index(), 2);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);

  // Check the opener-opened relationship.
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(0), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(1), WebStateOpener());
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(2),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 3));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(3),
            WebStateOpener(restored_web_state_list.GetWebStateAt(1), 2));
  EXPECT_EQ(restored_web_state_list.GetOpenerOfWebStateAt(4),
            WebStateOpener(restored_web_state_list.GetWebStateAt(2), 1));
}

// Tests deserializing into a non-empty WebStateList works when support for
// pinned tabs is disabled and the scope includes only pinned tabs.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_PinnedDisabled_PinnedOnly) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_PINNED,
      WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.pinned_item_count(), 1);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebState(),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kPinnedOnly,
          /*enable_pinned_web_states*/ false,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 0u);

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_EQ(restored_web_state_list.active_index(), 0);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);
}

// Tests deserializing into a non-empty WebStateList containing a single
// WebState displaying the NTP and without pending navigation leads to
// closing the old NTP tab.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_SingleTabNTP) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 1);
  EXPECT_EQ(storage.active_index(), 0);
  EXPECT_EQ(storage.pinned_item_count(), 0);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  // Record the WebStateID of the WebState displaying the NTP.
  const auto ntp_web_state_id = IdentifierAt(restored_web_state_list, 0);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_NE(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id);
}

// Tests deserializing into a non-empty WebStateList containing a single
// WebState displaying the NTP and with a pending navigation does not
// cause the tab to be closed.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_SingleTabNTP_PendingNavigation) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 1);
  EXPECT_EQ(storage.active_index(), 0);
  EXPECT_EQ(storage.pinned_item_count(), 0);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ true),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  // Record the WebStateID of the WebState displaying the NTP.
  const auto ntp_web_state_id = IdentifierAt(restored_web_state_list, 0);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 2);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id);
}

// Tests deserializing an empty session into a non-empty WebStateList
// containing a single WebState displaying the NTP and without pending
// does not cause the tab to be closed.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_SingleTabNTP_EmptySession) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 0);
  EXPECT_EQ(storage.active_index(), -1);
  EXPECT_EQ(storage.pinned_item_count(), 0);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 1);

  // Record the WebStateID of the WebState displaying the NTP.
  const auto ntp_web_state_id = IdentifierAt(restored_web_state_list, 0);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 0u);

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id);
}

// Tests deserializing into a non-empty WebStateList containing multiple
// WebStates does not lead to closing those tabs, even if they all display
// the NTP and have no pending navigation.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_MultipleNTPTabs) {
  FakeWebStateListDelegate delegate;
  WebStateList original_web_state_list(&delegate);
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(original_web_state_list, storage);

  EXPECT_EQ(storage.items_size(), 1);
  EXPECT_EQ(storage.active_index(), 0);
  EXPECT_EQ(storage.pinned_item_count(), 0);

  // Create a WebStateList with a single tab displaying NTP.
  WebStateList restored_web_state_list(&delegate);
  restored_web_state_list.InsertWebState(
      0, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());
  restored_web_state_list.InsertWebState(
      1, CreateWebStateOnNTP(/*has_pending_load*/ false),
      WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  ASSERT_EQ(restored_web_state_list.count(), 2);

  // Record the WebStateIDs of the WebStates displaying the NTP.
  const auto ntp_web_state_id_0 = IdentifierAt(restored_web_state_list, 0);
  const auto ntp_web_state_id_1 = IdentifierAt(restored_web_state_list, 1);

  // Check that after restoration, the old tab displaying the NTP
  // has been closed.
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &restored_web_state_list, std::move(storage),
          SessionRestorationScope::kAll,
          /*enable_pinned_web_states*/ true,
          base::BindRepeating(&CreateWebStateWithWebStateID));
  EXPECT_EQ(restored_web_states.size(), 1u);

  ASSERT_EQ(restored_web_state_list.count(), 3);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 0), ntp_web_state_id_0);
  EXPECT_EQ(IdentifierAt(restored_web_state_list, 1), ntp_web_state_id_1);
}
