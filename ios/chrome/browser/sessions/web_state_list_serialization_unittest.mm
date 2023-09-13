// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/web_state_list_serialization.h"

#import <memory>
#import <ostream>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Creates a fake WebState with `cnt` navigation items.
std::unique_ptr<web::WebState> CreateWebStateWithNavigationItemCount(int cnt) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationItemCount(cnt);
  return web_state;
}

// Creates a fake WebState with some navigations.
std::unique_ptr<web::WebState> CreateWebState() {
  return CreateWebStateWithNavigationItemCount(1);
}

// Creates a fake WebState with no navigation items.
std::unique_ptr<web::WebState> CreateWebStateWithNoNavigation() {
  return CreateWebStateWithNavigationItemCount(0);
}

// Creates a fake WebState from `storage`.
std::unique_ptr<web::WebState> CreateWebStateWithSessionStorage(
    CRWSessionStorage* storage) {
  return CreateWebState();
}

// Creates a fake WebState from `session_id`.
std::unique_ptr<web::WebState> CreateWebStateWithSessionID(
    SessionID session_id) {
  return CreateWebState();
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

using WebStateListSerializationTest = PlatformTest;

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
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0, CreateWebState(),
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
      4, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(2), 1));

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(session_window.sessions.count, 3u);
  EXPECT_EQ(session_window.selectedIndex, 2u);
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
    ASSERT_TRUE(SessionID::IsValidValue(item_storage.identifier()));
    EXPECT_EQ(SessionID::FromSerializedValue(item_storage.identifier()),
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
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  web_state_list.InsertWebState(
      0, CreateWebState(),
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
      4, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list.GetWebStateAt(2), 1));

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(storage.items_size(), 3);
  EXPECT_EQ(storage.active_index(), 2);
  EXPECT_EQ(storage.pinned_item_count(), 1);
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

  DeserializeWebStateList(
      &restored_web_state_list, session_window, SessionRestorationScope::kAll,
      /* enable_pinned_web_states*/ true,
      base::BindRepeating(&CreateWebStateWithSessionStorage));

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

  DeserializeWebStateList(
      &restored_web_state_list, session_window,
      SessionRestorationScope::kRegularOnly,
      /* enable_pinned_web_states*/ true,
      base::BindRepeating(&CreateWebStateWithSessionStorage));

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

  DeserializeWebStateList(
      &restored_web_state_list, session_window,
      SessionRestorationScope::kPinnedOnly,
      /* enable_pinned_web_states*/ true,
      base::BindRepeating(&CreateWebStateWithSessionStorage));

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

  DeserializeWebStateList(
      &restored_web_state_list, session_window, SessionRestorationScope::kAll,
      /* enable_pinned_web_states*/ false,
      base::BindRepeating(&CreateWebStateWithSessionStorage));

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

  DeserializeWebStateList(
      &restored_web_state_list, session_window,
      SessionRestorationScope::kRegularOnly,
      /* enable_pinned_web_states*/ false,
      base::BindRepeating(&CreateWebStateWithSessionStorage));

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

  DeserializeWebStateList(
      &restored_web_state_list, session_window,
      SessionRestorationScope::kPinnedOnly,
      /* enable_pinned_web_states*/ false,
      base::BindRepeating(&CreateWebStateWithSessionStorage));

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_EQ(restored_web_state_list.active_index(), 0);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);
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

  DeserializeWebStateList(restored_web_state_list, std::move(storage),
                          SessionRestorationScope::kAll,
                          /* enable_pinned_web_states*/ true,
                          base::BindRepeating(&CreateWebStateWithSessionID));

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

  DeserializeWebStateList(restored_web_state_list, std::move(storage),
                          SessionRestorationScope::kRegularOnly,
                          /* enable_pinned_web_states*/ true,
                          base::BindRepeating(&CreateWebStateWithSessionID));

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

  DeserializeWebStateList(restored_web_state_list, std::move(storage),
                          SessionRestorationScope::kPinnedOnly,
                          /* enable_pinned_web_states*/ true,
                          base::BindRepeating(&CreateWebStateWithSessionID));

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

  DeserializeWebStateList(restored_web_state_list, std::move(storage),
                          SessionRestorationScope::kAll,
                          /* enable_pinned_web_states*/ false,
                          base::BindRepeating(&CreateWebStateWithSessionID));

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

  DeserializeWebStateList(restored_web_state_list, std::move(storage),
                          SessionRestorationScope::kRegularOnly,
                          /* enable_pinned_web_states*/ false,
                          base::BindRepeating(&CreateWebStateWithSessionID));

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

  DeserializeWebStateList(restored_web_state_list, std::move(storage),
                          SessionRestorationScope::kPinnedOnly,
                          /* enable_pinned_web_states*/ false,
                          base::BindRepeating(&CreateWebStateWithSessionID));

  ASSERT_EQ(restored_web_state_list.count(), 1);
  EXPECT_EQ(restored_web_state_list.active_index(), 0);
  EXPECT_EQ(restored_web_state_list.pinned_tabs_count(), 0);
}
