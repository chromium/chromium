// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"

#import <memory>

#import "base/functional/bind.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<web::WebState> CreateWebStateWithNavigationItemCount(int cnt) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetNavigationItemCount(cnt);
  return web_state;
}

std::unique_ptr<web::WebState> CreateWebState() {
  return CreateWebStateWithNavigationItemCount(1);
}

std::unique_ptr<web::WebState> CreateWebStateWithSessionStorage(
    CRWSessionStorage* session_storage) {
  std::unique_ptr<web::WebState> web_state = CreateWebState();
  web::SerializableUserDataManager::FromWebState(web_state.get())
      ->SetUserDataFromSession(session_storage.userData);
  return web_state;
}

// Compares whether both WebStateList `original` and `restored` have the same
// opener-opened relationship. The `restored` WebStateList may have additional
// WebState, so only indices from `restored_index` to `count()` are compared.
void ExpectRelationshipIdenticalFrom(int restored_index,
                                     WebStateList* original,
                                     WebStateList* restored) {
  ASSERT_GE(restored_index, 0);
  EXPECT_EQ(original->count(), restored->count() - restored_index);

  for (int index = 0; index < original->count(); ++index) {
    WebStateOpener original_opener = original->GetOpenerOfWebStateAt(index);
    WebStateOpener restored_opener =
        restored->GetOpenerOfWebStateAt(index + restored_index);

    int restored_opener_index =
        restored_opener.opener
            ? restored->GetIndexOfWebState(restored_opener.opener) -
                  restored_index
            : WebStateList::kInvalidIndex;

    EXPECT_EQ(original->GetIndexOfWebState(original_opener.opener),
              restored_opener_index);
    EXPECT_EQ(original_opener.navigation_index,
              restored_opener.navigation_index);
  }
}

}  // namespace

class WebStateListSerializationTest : public PlatformTest {
 public:
  WebStateListSerializationTest() = default;

  WebStateListSerializationTest(const WebStateListSerializationTest&) = delete;
  WebStateListSerializationTest& operator=(
      const WebStateListSerializationTest&) = delete;

  WebStateListDelegate* web_state_list_delegate() {
    return &web_state_list_delegate_;
  }

 private:
  FakeWebStateListDelegate web_state_list_delegate_;
};

TEST_F(WebStateListSerializationTest, SerializationEmpty) {
  WebStateList original_web_state_list(web_state_list_delegate());
  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  EXPECT_EQ(0u, session_window.sessions.count);
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), session_window.selectedIndex);
}

TEST_F(WebStateListSerializationTest, SerializationRoundTrip) {
  WebStateList original_web_state_list(web_state_list_delegate());
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
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

  EXPECT_EQ(4u, session_window.sessions.count);
  EXPECT_EQ(1u, session_window.selectedIndex);

  // Create a deserialized WebStateList and verify its contents.
  WebStateList restored_web_state_list(web_state_list_delegate());
  restored_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  ASSERT_EQ(1, restored_web_state_list.count());

  DeserializeWebStateList(
      &restored_web_state_list, session_window, SessionRestorationScope::kAll,
      false, base::BindRepeating(&CreateWebStateWithSessionStorage));

  EXPECT_EQ(5, restored_web_state_list.count());
  EXPECT_EQ(2, restored_web_state_list.active_index());
  ExpectRelationshipIdenticalFrom(1, &original_web_state_list,
                                  &restored_web_state_list);

  // Verify that the WebUsageEnabled bit is left to default value.
  for (int i = 0; i < restored_web_state_list.count(); ++i)
    EXPECT_TRUE(restored_web_state_list.GetWebStateAt(i)->IsWebUsageEnabled());
}

TEST_F(WebStateListSerializationTest, Serialize) {
  WebStateList original_web_state_list(web_state_list_delegate());
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
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
  for (int i = 0; i < original_web_state_list.count(); ++i) {
    EXPECT_NSEQ(
        session_window.sessions[i].stableIdentifier,
        original_web_state_list.GetWebStateAt(i)->GetStableIdentifier());
  }
}

TEST_F(WebStateListSerializationTest, SerializationDropNoNavigation) {
  WebStateList original_web_state_list(web_state_list_delegate());
  original_web_state_list.InsertWebState(
      0, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  original_web_state_list.InsertWebState(
      1, CreateWebStateWithNavigationItemCount(0),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 3));
  original_web_state_list.InsertWebState(
      2, CreateWebStateWithNavigationItemCount(0),
      WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(0), 2));
  original_web_state_list.InsertWebState(
      3, CreateWebState(), WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
  original_web_state_list.InsertWebState(
      4, CreateWebState(), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(original_web_state_list.GetWebStateAt(1), 1));

  SessionWindowIOS* session_window =
      SerializeWebStateList(&original_web_state_list);

  // Check that the two tabs with no navigation items have been closed,
  // including the active tab (its next sibling should be selected).
  EXPECT_EQ(3u, session_window.sessions.count);
  EXPECT_EQ(static_cast<NSUInteger>(2), session_window.selectedIndex);
}
