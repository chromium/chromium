// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#import "base/scoped_multi_source_observation.h"
#import "base/supports_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
const char kURL0[] = "https://chromium.org/0";
const char kURL1[] = "https://chromium.org/1";
const char kURL2[] = "https://chromium.org/2";
const char kURL3[] = "https://chromium.org/3";

// WebStateList observer that records which events have been called by the
// WebStateList.
class WebStateListTestObserver : public WebStateListObserver {
 public:
  WebStateListTestObserver() = default;

  WebStateListTestObserver(const WebStateListTestObserver&) = delete;
  WebStateListTestObserver& operator=(const WebStateListTestObserver&) = delete;

  void Observe(WebStateList* web_state_list) {
    observation_.AddObservation(web_state_list);
  }

  // Reset statistics whether events have been called.
  void ResetStatistics() {
    web_state_inserted_ = false;
    web_state_moved_ = false;
    web_state_replaced_ = false;
    web_state_detached_ = false;
    web_state_activated_ = false;
    pinned_state_changed_ = false;
    batch_operation_started_ = false;
    batch_operation_ended_ = false;
    web_state_list_destroyed_ = false;
  }

  // Returns whether the insertion operation was invoked.
  bool web_state_inserted() const { return web_state_inserted_; }

  // Returns whether the move operation was invoked.
  bool web_state_moved() const { return web_state_moved_; }

  // Returns whether the replacement operation was invoked.
  bool web_state_replaced() const { return web_state_replaced_; }

  // Returns whether a WebState was detached.
  bool web_state_detached() const { return web_state_detached_; }

  // Returns whether a WebState was activated.
  bool web_state_activated() const { return web_state_activated_; }

  // Returns whether the pinned state was updated.
  bool pinned_state_changed() const { return pinned_state_changed_; }

  // Returns whether WillBeginBatchOperation was invoked.
  bool batch_operation_started() const { return batch_operation_started_; }

  // Returns whether BatchOperationEnded was invoked.
  bool batch_operation_ended() const { return batch_operation_ended_; }

  bool web_state_list_destroyed() const { return web_state_list_destroyed_; }

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    switch (change.type()) {
      case WebStateListChange::Type::kStatusOnly: {
        if (status.pinned_state_change) {
          pinned_state_changed_ = true;
        }
        // The activation is handled after this switch statement.
        break;
      }
      case WebStateListChange::Type::kDetach:
        EXPECT_TRUE(web_state_list->IsMutating());
        web_state_detached_ = true;
        break;
      case WebStateListChange::Type::kMove:
        EXPECT_TRUE(web_state_list->IsMutating());
        web_state_moved_ = true;
        break;
      case WebStateListChange::Type::kReplace:
        EXPECT_TRUE(web_state_list->IsMutating());
        web_state_replaced_ = true;
        break;
      case WebStateListChange::Type::kInsert:
        EXPECT_TRUE(web_state_list->IsMutating());
        web_state_inserted_ = true;
        break;
    }

    if (status.active_web_state_change()) {
      web_state_activated_ = true;
    }
  }

  void WillBeginBatchOperation(WebStateList* web_state_list) override {
    batch_operation_started_ = true;
  }

  void BatchOperationEnded(WebStateList* web_state_list) override {
    batch_operation_ended_ = true;
  }

  void WebStateListDestroyed(WebStateList* web_state_list) override {
    web_state_list_destroyed_ = true;
    observation_.RemoveObservation(web_state_list);
  }

 private:
  bool web_state_inserted_ = false;
  bool web_state_moved_ = false;
  bool web_state_replaced_ = false;
  bool web_state_detached_ = false;
  bool web_state_activated_ = false;
  bool pinned_state_changed_ = false;
  bool batch_operation_started_ = false;
  bool batch_operation_ended_ = false;
  bool web_state_list_destroyed_ = false;
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      observation_{this};
};

// A fake NavigationManager used to test opener-opened relationship in the
// WebStateList.
class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  FakeNavigationManager() = default;

  FakeNavigationManager(const FakeNavigationManager&) = delete;
  FakeNavigationManager& operator=(const FakeNavigationManager&) = delete;

  // web::NavigationManager implementation.
  int GetLastCommittedItemIndex() const override {
    return last_committed_item_index;
  }

  bool CanGoBack() const override { return last_committed_item_index > 0; }

  bool CanGoForward() const override {
    return last_committed_item_index < INT_MAX;
  }

  void GoBack() override {
    DCHECK(CanGoBack());
    --last_committed_item_index;
  }

  void GoForward() override {
    DCHECK(CanGoForward());
    ++last_committed_item_index;
  }

  void GoToIndex(int index) override { last_committed_item_index = index; }

  int last_committed_item_index = 0;
};

}  // namespace

class WebStateListTest : public PlatformTest {
 public:
  WebStateListTest() : web_state_list_(&web_state_list_delegate_) {
    observer_.Observe(&web_state_list_);
  }

  WebStateListTest(const WebStateListTest&) = delete;
  WebStateListTest& operator=(const WebStateListTest&) = delete;

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  WebStateListTestObserver observer_;

  std::unique_ptr<web::FakeWebState> CreateWebState(const char* url) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetCurrentURL(GURL(url));
    fake_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    return fake_web_state;
  }

  void AppendNewWebState(const char* url) {
    AppendNewWebState(url, WebStateOpener());
  }

  void AppendNewWebState(const char* url, WebStateOpener opener) {
    web_state_list_.InsertWebState(WebStateList::kInvalidIndex,
                                   CreateWebState(url),
                                   WebStateList::INSERT_NO_FLAGS, opener);
  }

  void AppendNewWebState(std::unique_ptr<web::FakeWebState> web_state) {
    web_state_list_.InsertWebState(
        WebStateList::kInvalidIndex, std::move(web_state),
        WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  }
};

// Tests that empty() matches count() != 0.
TEST_F(WebStateListTest, IsEmpty) {
  EXPECT_EQ(0, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);

  EXPECT_TRUE(observer_.web_state_inserted());
  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_FALSE(web_state_list_.empty());
}

// Tests that inserting a single webstate works.
TEST_F(WebStateListTest, InsertUrlSingle) {
  AppendNewWebState(kURL0);

  EXPECT_TRUE(observer_.web_state_inserted());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
}

// Tests that inserting multiple webstates puts them in the expected places.
TEST_F(WebStateListTest, InsertUrlMultiple) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  web_state_list_.InsertWebState(0, CreateWebState(kURL1),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  web_state_list_.InsertWebState(1, CreateWebState(kURL2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  EXPECT_TRUE(observer_.web_state_inserted());
  ASSERT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests webstate activation.
TEST_F(WebStateListTest, ActivateWebState) {
  AppendNewWebState(kURL0);
  EXPECT_EQ(nullptr, web_state_list_.GetActiveWebState());

  web_state_list_.ActivateWebStateAt(0);

  EXPECT_TRUE(observer_.web_state_activated());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            web_state_list_.GetActiveWebState());
}

// Tests activating a webstate as it is inserted.
TEST_F(WebStateListTest, InsertActivate) {
  web_state_list_.InsertWebState(
      0, CreateWebState(kURL0),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());

  EXPECT_TRUE(observer_.web_state_inserted());
  EXPECT_TRUE(observer_.web_state_activated());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            web_state_list_.GetActiveWebState());
}

// Tests finding a known webstate.
TEST_F(WebStateListTest, GetIndexOfWebState) {
  auto web_state_0 = CreateWebState(kURL0);
  web::WebState* target_web_state = web_state_0.get();
  auto other_web_state = CreateWebState(kURL1);

  // Target not yet in list.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebState(target_web_state));

  AppendNewWebState(kURL2);
  AppendNewWebState(std::move(web_state_0));
  // Target in list at index 1.
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebState(target_web_state));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebState(other_web_state.get()));

  // Another webstate with the same URL as the target also in list.
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebState(target_web_state));

  // Another webstate inserted before target; target now at index 2.
  web_state_list_.InsertWebState(0, CreateWebState(kURL3),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  EXPECT_EQ(2, web_state_list_.GetIndexOfWebState(target_web_state));
}

// Tests finding a webstate by URL.
TEST_F(WebStateListTest, GetIndexOfWebStateWithURL) {
  // Empty list.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // One webstate with a different URL in list.
  AppendNewWebState(kURL1);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Target URL at index 1.
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Another webstate with the target URL also at index 3.
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));
}

// Tests finding a non-active webstate by URL.
TEST_F(WebStateListTest, GetIndexOfInactiveWebStateWithURL) {
  // Empty list.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // One webstate with a different URL in list.
  AppendNewWebState(kURL1);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // Target URL at index 1.
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // Activate webstate at index 1.
  web_state_list_.ActivateWebStateAt(1);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));
  // GetIndexOfWebStateWithURL still finds it.
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Another webstate with the target URL also at index 3.
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL0);
  EXPECT_EQ(3, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Activate the webstate at index 2, so there the target URL is both before
  // and after the active webstate.
  web_state_list_.ActivateWebStateAt(2);
  EXPECT_EQ(1, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // Remove the webstate at index 1, so the only webstate with the target URL
  // is after the active webstate.
  web_state_list_.DetachWebStateAt(1);
  // Active webstate is now index 1, target URL is at index 2.
  EXPECT_EQ(2, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));
}

// Tests that inserted webstates correctly inherit openers.
TEST_F(WebStateListTest, InsertInheritOpener) {
  AppendNewWebState(kURL0);
  web_state_list_.ActivateWebStateAt(0);
  EXPECT_TRUE(observer_.web_state_activated());
  ASSERT_EQ(1, web_state_list_.count());
  ASSERT_EQ(web_state_list_.GetWebStateAt(0),
            web_state_list_.GetActiveWebState());

  web_state_list_.InsertWebState(
      WebStateList::kInvalidIndex, CreateWebState(kURL1),
      WebStateList::INSERT_INHERIT_OPENER, WebStateOpener());

  ASSERT_EQ(2, web_state_list_.count());
  ASSERT_EQ(web_state_list_.GetActiveWebState(),
            web_state_list_.GetOpenerOfWebStateAt(1).opener);
}

// Tests moving webstates one place to the "right" (to a higher index).
TEST_F(WebStateListTest, MoveWebStateAtRightByOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Coherence check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 1);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving webstates more than one place to the "right" (to a higher
// index).
TEST_F(WebStateListTest, MoveWebStateAtRightByMoreThanOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 2);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving webstates one place to the "left" (to a lower index).
TEST_F(WebStateListTest, MoveWebStateAtLeftByOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 1);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving webstates more than one place to the "left" (to a lower index).
TEST_F(WebStateListTest, MoveWebStateAtLeftByMoreThanOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 0);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests "moving" webstates (calling MoveWebStateAt with the same source and
// destination indexes.
TEST_F(WebStateListTest, MoveWebStateAtSameIndex) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 2);

  EXPECT_FALSE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving an active webstate.
TEST_F(WebStateListTest, MoveActiveWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(1, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(1, 2);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(2, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests replacing webstates.
TEST_F(WebStateListTest, ReplaceWebStateAt) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);

  // Sanity check before replacing WebState.
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.ReplaceWebStateAt(1, CreateWebState(kURL2)));

  EXPECT_TRUE(observer_.web_state_replaced());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, old_web_state->GetVisibleURL().spec());
}

// Tests replacing an active webstate.
TEST_F(WebStateListTest, ReplaceActiveWebStateAt) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity check before replacing WebState.
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(1, web_state_list_.active_index());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.ReplaceWebStateAt(1, CreateWebState(kURL2)));

  EXPECT_TRUE(observer_.web_state_replaced());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_EQ(1, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, old_web_state->GetVisibleURL().spec());
}

// Tests detaching webstates at index 0.
TEST_F(WebStateListTest, DetachWebStateAtIndexBegining) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(0);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests detaching webstates at an index that isn't 0 or the last index.
TEST_F(WebStateListTest, DetachWebStateAtIndexMiddle) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(1);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests detaching webstates at the last index.
TEST_F(WebStateListTest, DetachWebStateAtIndexLast) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(2);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests detaching an active webstate.
TEST_F(WebStateListTest, DetachActiveWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  web_state_list_.ActivateWebStateAt(0);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(0, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(0);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests closing all non-pinned webstates (pinned WebStates present).
TEST_F(WebStateListTest, CloseAllNonPinnedWebStates_PinnedWebStatesPresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  web_state_list_.CloseAllNonPinnedWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (non-pinned WebStates not present).
TEST_F(WebStateListTest,
       CloseAllNonPinnedWebStates_NonPinnedWebStatesNotPresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.SetWebStatePinnedAt(1, true);
  web_state_list_.SetWebStatePinnedAt(2, true);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  web_state_list_.CloseAllNonPinnedWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));

  EXPECT_FALSE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (pinned WebStates not present).
TEST_F(WebStateListTest, CloseAllNonPinnedWebStates_PinnedWebStatesNotPresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  web_state_list_.CloseAllNonPinnedWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (pinned active WebState present).
TEST_F(WebStateListTest,
       CloseAllNonPinnedWebStates_PinnedActiveWebStatePresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.ActivateWebStateAt(0);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  web_state_list_.CloseAllNonPinnedWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (pinned WebState and active non-pinned
// WebState present independently).
TEST_F(WebStateListTest,
       CloseAllNonPinnedWebStates_PinnedWebStateAndActiveWebStatePresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(1, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  web_state_list_.CloseAllNonPinnedWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (pinned and non-pinned).
TEST_F(WebStateListTest, CloseAllWebStates_PinnedNonPinned) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.SetWebStatePinnedAt(1, true);

  // Sanity check before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  web_state_list_.CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (non-pinned).
TEST_F(WebStateListTest, CloseAllWebStates_NonPinned) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  web_state_list_.CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (pinned, non-pinned and active WebStates).
TEST_F(WebStateListTest, CloseAllWebStates_PinnedNonPinnedWithActiveWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(1, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  web_state_list_.CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing one webstate.
TEST_F(WebStateListTest, CloseWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  web_state_list_.CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.batch_operation_started());
  EXPECT_FALSE(observer_.batch_operation_ended());
}

// Tests that batch operation can do nothing.
TEST_F(WebStateListTest, StartBatchOperation_DoNothing) {
  observer_.ResetStatistics();

  {
    WebStateList::ScopedBatchOperation lock =
        web_state_list_.StartBatchOperation();
  }

  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests that IsBatchInProgress() returns the correct value.
TEST_F(WebStateListTest, StartBatchOperation_IsBatchInProgress) {
  EXPECT_FALSE(web_state_list_.IsBatchInProgress());

  {
    WebStateList::ScopedBatchOperation lock =
        web_state_list_.StartBatchOperation();
    EXPECT_TRUE(web_state_list_.IsBatchInProgress());
  }

  EXPECT_FALSE(web_state_list_.IsBatchInProgress());
}

// Tests WebStates are pinned correctly while their order in the WebStateList
// doesn't change.
TEST_F(WebStateListTest, SetWebStatePinned_KeepingExisitingOrder) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin kURL0 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  // Pin kURL1 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  // Pin kURL2 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));

  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);
}

// Tests WebStates are pinned correctly while their order in the WebStateList
// change.
TEST_F(WebStateListTest, SetWebStatePinned_InRandomOrder) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin kURL2 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 0);
  // Pin kURL3 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 1);
  // Pin kURL0 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);
  // Unpin kURL3 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, false), 3);

  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));

  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);
}

// Tests pinned_tabs_count() and regular_tabs_count() return correct values.
TEST_F(WebStateListTest, PinnedAndRegularTabsCount) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 0);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 4);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 1);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 2);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 3);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 1);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 3);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 4);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 0);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 1);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 0);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 4);
}

// Tests InsertWebState method correctly updates insertion index if it is in the
// pinned WebStates range.
TEST_F(WebStateListTest, InsertWebState_InsertionInPinnedRange) {
  const char testURL0[] = "https://chromium.org/test_0";
  const char testURL1[] = "https://chromium.org/test_1";
  const char testURL2[] = "https://chromium.org/test_2";

  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(0, CreateWebState(testURL0),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  // Expect a WebState to be added at the end of the WebStateList.
  EXPECT_EQ(web_state_list_.GetWebStateAt(4)->GetVisibleURL().spec(), testURL0);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(2, CreateWebState(testURL1),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  // Expect a WebState to be added at the end of the WebStateList.
  EXPECT_EQ(web_state_list_.GetWebStateAt(5)->GetVisibleURL().spec(), testURL1);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(1, CreateWebState(testURL2),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  // Expect a WebState to be added at the end of the WebStateList.
  EXPECT_EQ(web_state_list_.GetWebStateAt(6)->GetVisibleURL().spec(), testURL2);
}

// Tests InsertWebState method correctly updates insertion index if it is in the
// pinned WebStates range and the flag is INSERT_FORCE_INDEX.
TEST_F(WebStateListTest, InsertWebState_ForceInsertionInPinnedRange) {
  const char testURL0[] = "https://chromium.org/test_0";
  const char testURL1[] = "https://chromium.org/test_1";
  const char testURL2[] = "https://chromium.org/test_2";

  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(0, CreateWebState(testURL0),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  // Expect a WebState to be added at the end of WebStates list.
  EXPECT_EQ(web_state_list_.GetWebStateAt(4)->GetVisibleURL().spec(), testURL0);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(2, CreateWebState(testURL1),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  // Expect a WebState to be added at the end of WebStates list.
  EXPECT_EQ(web_state_list_.GetWebStateAt(5)->GetVisibleURL().spec(), testURL1);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(1, CreateWebState(testURL2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  // Expect a WebState to be added at the end of WebStates list.
  EXPECT_EQ(web_state_list_.GetWebStateAt(6)->GetVisibleURL().spec(), testURL2);
}

// Tests InsertWebState method correctly updates insertion index when the flag
// is INSERT_PINNED.
TEST_F(WebStateListTest, InsertWebState_InsertWebStatePinned) {
  const char testURL0[] = "https://chromium.org/test_0";
  const char testURL1[] = "https://chromium.org/test_1";
  const char testURL2[] = "https://chromium.org/test_2";

  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Insert a pinned WebState at invalid index.
  web_state_list_.InsertWebState(WebStateList::kInvalidIndex,
                                 CreateWebState(testURL0),
                                 WebStateList::INSERT_PINNED, WebStateOpener());
  // Expect a WebState to be added into pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), testURL0);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  // Insert a pinned WebState to the non-pinned WebStates range.
  web_state_list_.InsertWebState(2, CreateWebState(testURL1),
                                 WebStateList::INSERT_PINNED, WebStateOpener());
  // Expect a WebState to be added at the end of the pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), testURL1);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));

  // Insert a pinned WebState to the pinned WebStates range.
  web_state_list_.InsertWebState(0, CreateWebState(testURL2),
                                 WebStateList::INSERT_PINNED, WebStateOpener());
  // Expect a WebState to be added at the end of the pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), testURL2);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));

  // Final check that only first three WebStates were pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));
}

// Tests InsertWebState method correctly updates insertion index when the flags
// are INSERT_PINNED and INSERT_FORCE_INDEX.
TEST_F(WebStateListTest, InsertWebState_InsertWebStatePinnedForceIndex) {
  const char testURL0[] = "https://chromium.org/test_0";
  const char testURL1[] = "https://chromium.org/test_1";
  const char testURL2[] = "https://chromium.org/test_2";

  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Insert a pinned WebState at invalid index.
  web_state_list_.InsertWebState(
      WebStateList::kInvalidIndex, CreateWebState(testURL0),
      WebStateList::INSERT_PINNED | WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());
  // Expect a WebState to be added into pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), testURL0);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  // Insert a pinned WebState to the non-pinned WebStates range.
  web_state_list_.InsertWebState(
      2, CreateWebState(testURL1),
      WebStateList::INSERT_PINNED | WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());
  // Expect a WebState to be added at the end of the pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), testURL1);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));

  // Insert a pinned WebState to the pinned WebStates range.
  web_state_list_.InsertWebState(
      0, CreateWebState(testURL2),
      WebStateList::INSERT_PINNED | WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());
  // Expect a WebState to be added at the same index.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), testURL2);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  // Final check that only first three WebStates were pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));
}

// Tests MoveWebStateAt method moves the pinned WebStates within pinned
// WebStates range only.
TEST_F(WebStateListTest, MoveWebStateAt_KeepsPinnedWebStateWithinPinnedRange) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin first three WebStates.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  // Check the WebStates order.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);

  // Try to move first pinned WebState inside of the pinned WebStates range.
  web_state_list_.MoveWebStateAt(0, 2);

  // Try to move first pinned WebState outside of the pinned WebStates range.
  web_state_list_.MoveWebStateAt(0, 3);

  // Expect the pinned WebStates to be moved within pinned WebStates range only.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);
}

// Tests MoveWebStateAt method moves the non-pinned WebStates within non-pinned
// WebStates range only.
TEST_F(WebStateListTest,
       MoveWebStateAt_KeepsNonPinnedWebStatesWithinNonPinnedRange) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin first two WebStates.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);

  // Check WebStates order.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);

  // Try to move first non-pinned WebState inside of the non-pinned WebStates
  // range.
  web_state_list_.MoveWebStateAt(2, 3);

  // Try to move first non-pinned WebState to the pinned WebStates range.
  web_state_list_.MoveWebStateAt(2, 1);

  // Expect the non-pinned WebStates to be moved within non-pinned WebStates
  // range only.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL3);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL2);
}

TEST_F(WebStateListTest, WebStateListDestroyed) {
  // Using a local WebStateList to observe its destruction.
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate_);
  observer_.Observe(web_state_list.get());
  EXPECT_FALSE(observer_.web_state_list_destroyed());
  web_state_list.reset();
  EXPECT_TRUE(observer_.web_state_list_destroyed());
}

TEST_F(WebStateListTest, WebStateListAsWeakPtr) {
  // Using a local WebStateList to observe its destruction.
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate_);
  base::WeakPtr<WebStateList> weak_web_state_list = web_state_list->AsWeakPtr();
  EXPECT_TRUE(weak_web_state_list);
  web_state_list.reset();
  EXPECT_FALSE(weak_web_state_list);
}
