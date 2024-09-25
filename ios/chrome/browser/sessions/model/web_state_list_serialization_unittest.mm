// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_state_list_serialization.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/sessions/model/features.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_tab_group.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/tab_group_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using tab_groups::TabGroupId;

namespace {

// Creates a fake WebState with `navigation_count` navigation items (all
// pointing to the same `url`).
std::unique_ptr<web::WebState> CreateWebStateWithNavigations(
    int navigation_count,
    const GURL& url,
    web::WebStateID web_state_id) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  for (int index = 0; index < navigation_count; ++index) {
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
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
  return CreateWebStateWithNavigations(/*navigation_count=*/1,
                                       GURL(kChromeUIVersionURL),
                                       web::WebStateID::NewUnique());
}

// Creates a fake WebState with no navigation items.
std::unique_ptr<web::WebState> CreateWebStateWithNoNavigation(
    web::WebStateID web_state_id = web::WebStateID::NewUnique()) {
  return CreateWebStateWithNavigations(/*navigation_count=*/0, GURL(),
                                       web_state_id);
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
  return CreateWebStateWithNavigations(/*navigation_count=*/1,
                                       GURL(kChromeUIVersionURL), web_state_id);
}

// Creates a fake WebState from serialized data.
std::unique_ptr<web::WebState> CreateWebStateFromProto(
    web::WebStateID web_state_id,
    web::proto::WebStateMetadataStorage metadata) {
  return CreateWebState();
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

    web::proto::WebStateMetadataStorage metadata;
    web_state->SerializeMetadataToProto(metadata);
    metadata_map.insert(std::make_pair(web_state_id, std::move(metadata)));
  }

  SerializeWebStateList(web_state_list, metadata_map, storage);
}

// Checks that the given `tab_group_range` is present in the `web_state_list`.
bool CheckWebStateListHasTabGroup(const WebStateList& web_state_list,
                                  TabGroupRange tab_group_range) {
  if (!web_state_list.ContainsIndex(tab_group_range.range_begin())) {
    return false;
  }
  const TabGroup* group =
      web_state_list.GetGroupOfWebStateAt(tab_group_range.range_begin());
  return group && group->range() == tab_group_range;
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
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebState(),
      WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
  web_state_list.InsertWebState(
      CreateWebState(),
      WebStateList::InsertionParams::AtIndex(2).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
  web_state_list.InsertWebState(
      CreateWebState(),
      WebStateList::InsertionParams::AtIndex(3).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

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
      CreateWebStateWithNoNavigation(),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithNoNavigation(),
      WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
  web_state_list.InsertWebState(
      CreateWebStateWithNoNavigation(),
      WebStateList::InsertionParams::AtIndex(2).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
  web_state_list.InsertWebState(CreateWebState(),
                                WebStateList::InsertionParams::AtIndex(3));
  web_state_list.InsertWebState(
      CreateWebStateWithPendingNavigation(pending_item.get()),
      WebStateList::InsertionParams::AtIndex(4).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(session_window.sessions.count, 2u);
  EXPECT_EQ(session_window.selectedIndex, 1u);

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
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(1).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::InsertionParams::AtIndex(2));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(3).Activate());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(4));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::InsertionParams::AtIndex(5));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1111)),
      WebStateList::InsertionParams::AtIndex(6));

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

// Tests that serializing a WebStateList drops tabs with similar identifiers and
// updates the appropriate groups.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Serialize_ObjC_DropDuplicatesWithGroups) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  WebStateListBuilderFromDescription builder(&web_state_list);
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(1).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(3333)),
      WebStateList::InsertionParams::AtIndex(2));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(3).Activate());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(4));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(3333)),
      WebStateList::InsertionParams::AtIndex(5));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1111)),
      WebStateList::InsertionParams::AtIndex(6));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1112)),
      WebStateList::InsertionParams::AtIndex(7));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(3333)),
      WebStateList::InsertionParams::AtIndex(8));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(9));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1113)),
      WebStateList::InsertionParams::AtIndex(10));
  web_state_list.CreateGroup({2, 3}, {}, TabGroupId::GenerateNew());
  web_state_list.CreateGroup({6, 7}, {}, TabGroupId::GenerateNew());
  web_state_list.CreateGroup({8}, {}, TabGroupId::GenerateNew());
  web_state_list.CreateGroup({9, 10}, {}, TabGroupId::GenerateNew());
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("a b | [ 0 c d* ] e f [ 1 g h ] [ 2 i ] [ 3 j k ]",
            builder.GetWebStateListDescription());

  // Serialize the session and check the serialized data is correct.
  SessionWindowIOS* session_window = SerializeWebStateList(&web_state_list);

  // Check that the duplicate items have been closed, including the active tab
  // (its next kept sibling should be selected).
  EXPECT_EQ(session_window.sessions.count, 5u);
  EXPECT_EQ(session_window.selectedIndex, 2u);
  EXPECT_EQ(session_window.tabGroups.count, 3u);
  NSArray<SessionTabGroup*>* groups = [session_window.tabGroups
      sortedArrayUsingComparator:^(SessionTabGroup* group_1,
                                   SessionTabGroup* group_2) {
        if (group_1.rangeStart > group_2.rangeStart) {
          return NSOrderedDescending;
        } else if (group_1.rangeStart < group_2.rangeStart) {
          return NSOrderedAscending;
        }
        return NSOrderedSame;
      }];
  EXPECT_EQ(groups[0].rangeStart, 1);
  EXPECT_EQ(groups[0].rangeCount, 1);
  EXPECT_EQ(groups[1].rangeStart, 2);
  EXPECT_EQ(groups[1].rangeCount, 2);
  EXPECT_EQ(groups[2].rangeStart, 4);
  EXPECT_EQ(groups[2].rangeCount, 1);

  // Expect a log of 6 duplicates.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 6, 1);
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
      CreateWebStateWithNoNavigation(same_web_state_id),
      WebStateList::InsertionParams::AtIndex(0).Pinned().Activate());
  web_state_list.InsertWebState(CreateWebStateWithWebStateID(same_web_state_id),
                                WebStateList::InsertionParams::AtIndex(1));
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
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebState(),
      WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
  web_state_list.InsertWebState(
      CreateWebState(),
      WebStateList::InsertionParams::AtIndex(2).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
  web_state_list.InsertWebState(
      CreateWebState(),
      WebStateList::InsertionParams::AtIndex(3).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

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
      CreateWebStateWithNoNavigation(),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithNoNavigation(),
      WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
  web_state_list.InsertWebState(
      CreateWebStateWithNoNavigation(),
      WebStateList::InsertionParams::AtIndex(2).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
  web_state_list.InsertWebState(CreateWebState(),
                                WebStateList::InsertionParams::AtIndex(3));
  web_state_list.InsertWebState(
      CreateWebStateWithPendingNavigation(pending_item.get()),
      WebStateList::InsertionParams::AtIndex(4).WithOpener(
          WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(storage.items_size(), 2);
  EXPECT_EQ(storage.active_index(), 1);

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
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(1).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::InsertionParams::AtIndex(2));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(3).Activate());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(4444)),
      WebStateList::InsertionParams::AtIndex(4));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(2222)),
      WebStateList::InsertionParams::AtIndex(5));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1111)),
      WebStateList::InsertionParams::AtIndex(6));

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

// Tests that serializing a WebStateList drops tabs with similar identifiers.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Serialize_Proto_DropDuplicatesWithGroups) {
  FakeWebStateListDelegate delegate;
  WebStateList web_state_list(&delegate);
  WebStateListBuilderFromDescription builder(&web_state_list);
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(1).Pinned());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(3333)),
      WebStateList::InsertionParams::AtIndex(2));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(3).Activate());
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(4));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(3333)),
      WebStateList::InsertionParams::AtIndex(5));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1111)),
      WebStateList::InsertionParams::AtIndex(6));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1112)),
      WebStateList::InsertionParams::AtIndex(7));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(3333)),
      WebStateList::InsertionParams::AtIndex(8));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(5555)),
      WebStateList::InsertionParams::AtIndex(9));
  web_state_list.InsertWebState(
      CreateWebStateWithWebStateID(web::WebStateID::FromSerializedValue(1113)),
      WebStateList::InsertionParams::AtIndex(10));
  web_state_list.CreateGroup({2, 3}, {}, TabGroupId::GenerateNew());
  web_state_list.CreateGroup({6, 7}, {}, TabGroupId::GenerateNew());
  web_state_list.CreateGroup({8}, {}, TabGroupId::GenerateNew());
  web_state_list.CreateGroup({9, 10}, {}, TabGroupId::GenerateNew());
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("a b | [ 0 c d* ] e f [ 1 g h ] [ 2 i ] [ 3 j k ]",
            builder.GetWebStateListDescription());

  // Serialize the session and check the serialized data is correct.
  ios::proto::WebStateListStorage storage;
  SerializeWebStateList(web_state_list, storage);

  // Check that the duplicate items have been closed, including the active tab
  // (its next kept sibling should be selected).
  EXPECT_EQ(storage.items_size(), 5);
  EXPECT_EQ(storage.active_index(), 2);
  EXPECT_EQ(storage.pinned_item_count(), 1);
  EXPECT_EQ(storage.groups_size(), 3);
  std::sort(storage.mutable_groups()->pointer_begin(),
            storage.mutable_groups()->pointer_end(),
            [](const ios::proto::TabGroupStorage* group_1,
               const ios::proto::TabGroupStorage* group_2) {
              return group_1->range().start() < group_2->range().start();
            });
  EXPECT_EQ(storage.groups(0).range().start(), 1);
  EXPECT_EQ(storage.groups(0).range().count(), 1);
  EXPECT_EQ(storage.groups(1).range().start(), 2);
  EXPECT_EQ(storage.groups(1).range().count(), 2);
  EXPECT_EQ(storage.groups(2).range().start(), 4);
  EXPECT_EQ(storage.groups(2).range().count(), 1);

  // Expect a log of 6 duplicates.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnSessionSave", 6, 1);
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
      CreateWebStateWithNoNavigation(same_web_state_id),
      WebStateList::InsertionParams::AtIndex(0).Pinned().Activate());
  web_state_list.InsertWebState(CreateWebStateWithWebStateID(same_web_state_id),
                                WebStateList::InsertionParams::AtIndex(1));
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
        CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(2).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(3).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ true, /*enable_tab_groups*/ true,
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
        CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(2).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(3).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ false, /*enable_tab_groups*/ true,
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
        CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(2).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(3).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

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
                              /*enable_tab_groups*/ true,
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
        CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(2).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(3).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

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
                              /*enable_tab_groups*/ true,
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
        CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Pinned());
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(1).Activate().WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 3)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(2).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(0), 2)));
    web_state_list.InsertWebState(
        CreateWebState(),
        WebStateList::InsertionParams::AtIndex(3).WithOpener(
            WebStateOpener(web_state_list.GetWebStateAt(1), 1)));

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
                              /*enable_tab_groups*/ true,
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

// Tests deserializing works when support for tab groups is enabled.
// Tests with one tab group.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_TabGroupsEnabled) {
  FakeWebStateListDelegate delegate;
  const TabGroupRange group_range = TabGroupRange(0, 1);

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* c d"));

    SerializeWebStateList(web_state_list, storage);

    EXPECT_EQ(storage.items_size(), 4);
    EXPECT_EQ(storage.active_index(), 1);
    EXPECT_EQ(storage.groups_size(), 1);
  }

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ true,
                              /*enable_tab_groups*/ true,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);

  // Check tab groups.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, group_range));
}

// Tests deserializing works when support for tab groups is enabled.
// Tests with one tab group.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_TabGroupsEnabled) {
  FakeWebStateListDelegate delegate;
  const TabGroupRange group_range = TabGroupRange(0, 1);

  // Create a WebStateList, populate it, and save data to `session_window`.
  SessionWindowIOS* session_window = nil;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* c d"));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
    EXPECT_EQ(session_window.tabGroups.count, 1u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ true, /*enable_tab_groups*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check tab groups.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, group_range));
}

// Tests deserializing works when support for tab groups is enabled.
// Tests with multiple tab groups.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest,
       Deserialize_Proto_TabGroupsEnabled_MultipleGroups) {
  FakeWebStateListDelegate delegate;
  const TabGroupRange group_range_first = TabGroupRange(0, 1);
  const TabGroupRange group_range_second = TabGroupRange(2, 2);

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* [1 c d]"));

    SerializeWebStateList(web_state_list, storage);

    EXPECT_EQ(storage.items_size(), 4);
    EXPECT_EQ(storage.active_index(), 1);
    EXPECT_EQ(storage.groups_size(), 2);
  }

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ true,
                              /*enable_tab_groups*/ true,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);

  // Check tab groups.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 2u);
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, group_range_first));
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, group_range_second));
}

// Tests deserializing works when support for tab groups is enabled.
// Tests with multiple tab groups.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_MultipleGroups) {
  FakeWebStateListDelegate delegate;
  const TabGroupRange group_range_first = TabGroupRange(0, 1);
  const TabGroupRange group_range_second = TabGroupRange(2, 2);

  // Create a WebStateList, populate it, and save data to `session_window`.
  SessionWindowIOS* session_window = nil;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* [1 c d]"));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
    EXPECT_EQ(session_window.tabGroups.count, 2u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ true, /*enable_tab_groups*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check tab groups.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 2u);
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, group_range_first));
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, group_range_second));
}

// Tests deserializing works when support for tab groups is disabled.
// Tests with multiple tab groups.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_TabGroupsDisabled) {
  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* [1 c d]"));

    SerializeWebStateList(web_state_list, storage);

    EXPECT_EQ(storage.items_size(), 4);
    EXPECT_EQ(storage.active_index(), 1);
    EXPECT_EQ(storage.groups_size(), 2);
  }

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ true,
                              /*enable_tab_groups*/ false,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);

  // Check tab groups.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 0u);
}

// Tests deserializing works when support for tab groups is disabled.
// Tests with multiple tab groups.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_TabGroupsDisabled) {
  FakeWebStateListDelegate delegate;

  // Create a WebStateList, populate it, and save data to `session_window`.
  SessionWindowIOS* session_window = nil;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* [1 c d]"));

    session_window = SerializeWebStateList(&web_state_list);

    EXPECT_EQ(session_window.sessions.count, 4u);
    EXPECT_EQ(session_window.selectedIndex, 1u);
    EXPECT_EQ(session_window.tabGroups.count, 2u);
  }

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ true, /*enable_tab_groups*/ false,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check tab groups.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 0u);
}

// Tests deserializing works when support for tab groups is enabled.
// Tests with invalid tab group.
//
// Protobuf message variant.
TEST_F(WebStateListSerializationTest, Deserialize_Proto_TabGroupsInvalid) {
  FakeWebStateListDelegate delegate;
  const TabGroupRange valid_group_range = TabGroupRange(0, 1);

  // Create a WebStateList, populate it and serialize to `storage`.
  ios::proto::WebStateListStorage storage;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* c d"));

    SerializeWebStateList(web_state_list, storage);
  }

  // Add invalid tab group to `storage`.
  ios::proto::TabGroupStorage& group_storage = *storage.add_groups();
  ios::proto::RangeIndex& range = *group_storage.mutable_range();
  range.set_start(2);
  range.set_count(4);
  group_storage.set_title("Invalid");
  group_storage.set_color(ios::proto::TabGroupColorId::GREY);
  group_storage.set_collapsed(false);
  tab_group_util::TabGroupIdForStorage(TabGroupId::GenerateNew(),
                                       *group_storage.mutable_tab_group_id());

  EXPECT_EQ(storage.items_size(), 4);
  EXPECT_EQ(storage.active_index(), 1);
  EXPECT_EQ(storage.groups_size(), 2);

  // Deserialize `storage` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(&web_state_list, std::move(storage),
                              /*enable_pinned_web_states*/ true,
                              /*enable_tab_groups*/ true,
                              base::BindRepeating(&CreateWebStateFromProto));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);

  // Check tab group.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, valid_group_range));
}

// Tests deserializing works when support for tab groups is enabled.
// Tests with invalid tab group.
//
// Objective-C (legacy) variant.
TEST_F(WebStateListSerializationTest, Deserialize_ObjC_TabGroupsInvalid) {
  FakeWebStateListDelegate delegate;
  const TabGroupRange valid_group_range = TabGroupRange(0, 1);

  // Create a WebStateList, populate it, and save data to `session_window`.
  SessionWindowIOS* session_window = nil;
  {
    WebStateList web_state_list(&delegate);
    WebStateListBuilderFromDescription builder(&web_state_list);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a] b* c d"));

    session_window = SerializeWebStateList(&web_state_list);
  }

  // Add invalid tab group to `session_window`.
  SessionTabGroup* invalid_tab_group =
      [[SessionTabGroup alloc] initWithRangeStart:2
                                       rangeCount:4
                                            title:@"Invalid"
                                          colorId:0
                                   collapsedState:NO
                                       tabGroupId:TabGroupId::GenerateNew()];
  NSArray<SessionTabGroup*>* session_groups = [session_window.tabGroups
      arrayByAddingObjectsFromArray:@[ invalid_tab_group ]];
  session_window =
      [[SessionWindowIOS alloc] initWithSessions:session_window.sessions
                                       tabGroups:session_groups
                                   selectedIndex:session_window.selectedIndex];

  EXPECT_EQ(session_window.sessions.count, 4u);
  EXPECT_EQ(session_window.selectedIndex, 1u);
  EXPECT_EQ(session_window.tabGroups.count, 2u);

  // Deserialize `session_window` into a new empty WebStateList.
  WebStateList web_state_list(&delegate);
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          &web_state_list, session_window,
          /*enable_pinned_web_states*/ true, /*enable_tab_groups*/ true,
          base::BindRepeating(&CreateWebStateWithSessionStorage));
  EXPECT_EQ(restored_web_states.size(), 4u);

  ASSERT_EQ(web_state_list.count(), 4);
  EXPECT_EQ(web_state_list.active_index(), 1);
  EXPECT_EQ(web_state_list.pinned_tabs_count(), 0);

  // Check tab group.
  std::set<const TabGroup*> groups = web_state_list.GetGroups();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_TRUE(CheckWebStateListHasTabGroup(web_state_list, valid_group_range));
}
