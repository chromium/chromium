// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list.h"

#include "base/supports_user_data.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  // Reset statistics whether events have been called.
  void ResetStatistics() {
    web_state_inserted_called_ = false;
    web_state_moved_called_ = false;
    web_state_replaced_called_ = false;
    web_state_detached_called_ = false;
    web_state_activated_called_ = false;
    batch_operation_started_ = false;
    batch_operation_ended_ = false;
  }

  // Returns whether WebStateInsertedAt was invoked.
  bool web_state_inserted_called() const { return web_state_inserted_called_; }

  // Returns whether WebStateMoved was invoked.
  bool web_state_moved_called() const { return web_state_moved_called_; }

  // Returns whether WebStateReplacedAt was invoked.
  bool web_state_replaced_called() const { return web_state_replaced_called_; }

  // Returns whether WebStateDetachedAt was invoked.
  bool web_state_detached_called() const { return web_state_detached_called_; }

  // Returns whether WebStateActivatedAt was invoked.
  bool web_state_activated_called() const {
    return web_state_activated_called_;
  }

  // Returns whether WillBeginBatchOperation was invoked.
  bool batch_operation_started() const { return batch_operation_started_; }

  // Returns whether BatchOperationEnded was invoked.
  bool batch_operation_ended() const { return batch_operation_ended_; }

  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override {
    EXPECT_TRUE(web_state_list->IsMutating());
    web_state_inserted_called_ = true;
  }

  void WebStateMoved(WebStateList* web_state_list,
                     web::WebState* web_state,
                     int from_index,
                     int to_index) override {
    EXPECT_TRUE(web_state_list->IsMutating());
    web_state_moved_called_ = true;
  }

  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override {
    web_state_replaced_called_ = true;
  }

  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override {
    EXPECT_TRUE(web_state_list->IsMutating());
    web_state_detached_called_ = true;
  }

  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override {
    web_state_activated_called_ = true;
  }

  void WillBeginBatchOperation(WebStateList* web_state_list) override {
    batch_operation_started_ = true;
  }

  void BatchOperationEnded(WebStateList* web_state_list) override {
    batch_operation_ended_ = true;
  }

 private:
  bool web_state_inserted_called_ = false;
  bool web_state_moved_called_ = false;
  bool web_state_replaced_called_ = false;
  bool web_state_detached_called_ = false;
  bool web_state_activated_called_ = false;
  bool batch_operation_started_ = false;
  bool batch_operation_ended_ = false;
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
    web_state_list_.AddObserver(&observer_);
  }

  WebStateListTest(const WebStateListTest&) = delete;
  WebStateListTest& operator=(const WebStateListTest&) = delete;

  ~WebStateListTest() override { web_state_list_.RemoveObserver(&observer_); }

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

  EXPECT_TRUE(observer_.web_state_inserted_called());
  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_FALSE(web_state_list_.empty());
}

// Tests that inserting a single webstate works.
TEST_F(WebStateListTest, InsertUrlSingle) {
  AppendNewWebState(kURL0);

  EXPECT_TRUE(observer_.web_state_inserted_called());
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

  EXPECT_TRUE(observer_.web_state_inserted_called());
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

  EXPECT_TRUE(observer_.web_state_activated_called());
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

  EXPECT_TRUE(observer_.web_state_activated_called());
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
  EXPECT_TRUE(observer_.web_state_activated_called());
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

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 1);

  EXPECT_TRUE(observer_.web_state_moved_called());
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

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 2);

  EXPECT_TRUE(observer_.web_state_moved_called());
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

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 1);

  EXPECT_TRUE(observer_.web_state_moved_called());
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

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 0);

  EXPECT_TRUE(observer_.web_state_moved_called());
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

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 2);

  EXPECT_FALSE(observer_.web_state_moved_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests replacing webstates.
TEST_F(WebStateListTest, ReplaceWebStateAt) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);

  // Sanity check before replacing WebState.
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.ReplaceWebStateAt(1, CreateWebState(kURL2)));

  EXPECT_TRUE(observer_.web_state_replaced_called());
  EXPECT_TRUE(observer_.web_state_activated_called());
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

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(0);

  EXPECT_TRUE(observer_.web_state_detached_called());
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

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(1);

  EXPECT_TRUE(observer_.web_state_detached_called());
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

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(2);

  EXPECT_TRUE(observer_.web_state_detached_called());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests finding opended-by indexes on an empty list.
TEST_F(WebStateListTest, OpenersEmptyList) {
  EXPECT_TRUE(web_state_list_.empty());

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, false));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, false));

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, true));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, true));
}

// Tests finding opended-by indexes when no webstates have been opened.
TEST_F(WebStateListTest, OpenersNothingOpened) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  for (int index = 0; index < web_state_list_.count(); ++index) {
    web::WebState* opener = web_state_list_.GetWebStateAt(index);
    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, index, false));
    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, index, false));

    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, index, true));
    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, index, true));
  }
}

// Tests finding opended-by indexes when the opened child is at an index after
// the parent.
TEST_F(WebStateListTest, OpenersChildsAfterOpener) {
  AppendNewWebState(kURL0);
  web::WebState* opener = web_state_list_.GetWebStateAt(0);

  AppendNewWebState(kURL1, WebStateOpener(opener));
  AppendNewWebState(kURL2, WebStateOpener(opener));

  const int start_index = web_state_list_.GetIndexOfWebState(opener);
  EXPECT_EQ(1, web_state_list_.GetIndexOfNextWebStateOpenedBy(
                   opener, start_index, false));
  EXPECT_EQ(2, web_state_list_.GetIndexOfLastWebStateOpenedBy(
                   opener, start_index, false));

  EXPECT_EQ(1, web_state_list_.GetIndexOfNextWebStateOpenedBy(
                   opener, start_index, true));
  EXPECT_EQ(2, web_state_list_.GetIndexOfLastWebStateOpenedBy(
                   opener, start_index, true));

  // Simulate a navigation on the opener, results should not change if not
  // using groups, but should now be kInvalidIndex otherwise.
  opener->GetNavigationManager()->GoForward();

  EXPECT_EQ(1, web_state_list_.GetIndexOfNextWebStateOpenedBy(
                   opener, start_index, false));
  EXPECT_EQ(2, web_state_list_.GetIndexOfLastWebStateOpenedBy(
                   opener, start_index, false));

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           true));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           true));

  // Add a new WebState with the same opener. It should be considered the next
  // WebState if groups are considered and the last independently on whether
  // groups are used or not.
  web_state_list_.InsertWebState(
      3, CreateWebState(kURL2), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener(web_state_list_.GetWebStateAt(0)));

  EXPECT_EQ(1, web_state_list_.GetIndexOfNextWebStateOpenedBy(
                   opener, start_index, false));
  EXPECT_EQ(3, web_state_list_.GetIndexOfLastWebStateOpenedBy(
                   opener, start_index, false));

  EXPECT_EQ(3, web_state_list_.GetIndexOfNextWebStateOpenedBy(
                   opener, start_index, true));
  EXPECT_EQ(3, web_state_list_.GetIndexOfLastWebStateOpenedBy(
                   opener, start_index, true));
}

// Tests finding opended-by indexes when the opened child is at an index before
// the parent.
TEST_F(WebStateListTest, OpenersChildsBeforeOpener) {
  AppendNewWebState(kURL0);
  web::WebState* opener = web_state_list_.GetWebStateAt(0);

  AppendNewWebState(kURL1, WebStateOpener(opener));
  AppendNewWebState(kURL2, WebStateOpener(opener));
  web_state_list_.MoveWebStateAt(0, 2);

  const int start_index = web_state_list_.GetIndexOfWebState(opener);
  EXPECT_EQ(0, web_state_list_.GetIndexOfNextWebStateOpenedBy(
                   opener, start_index, false));
  EXPECT_EQ(1, web_state_list_.GetIndexOfLastWebStateOpenedBy(
                   opener, start_index, false));
}

// Tests closing all webstates.
TEST_F(WebStateListTest, CloseAllWebStates) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  web_state_list_.CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached_called());
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
  EXPECT_TRUE(observer_.web_state_detached_called());
  EXPECT_FALSE(observer_.batch_operation_started());
  EXPECT_FALSE(observer_.batch_operation_ended());
}

// Tests that batch operation can be empty.
TEST_F(WebStateListTest, PerformBatchOperation_EmptyCallback) {
  observer_.ResetStatistics();

  web_state_list_.PerformBatchOperation({});

  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests that batch operation WebStateList is the correct one.
TEST_F(WebStateListTest, PerformBatchOperation_CorrectWebStateList) {
  WebStateList* captured_web_state_list = nullptr;
  web_state_list_.PerformBatchOperation(base::BindOnce(
      [](WebStateList** captured_web_state_list, WebStateList* web_state_list) {
        *captured_web_state_list = web_state_list;
      },
      &captured_web_state_list));

  EXPECT_EQ(captured_web_state_list, &web_state_list_);
}

// Tests that IsBatchInProgress() returns the correct value.
TEST_F(WebStateListTest, PerformBatchOperation_IsBatchInProgress) {
  EXPECT_FALSE(web_state_list_.IsBatchInProgress());

  bool captured_batch_in_progress = false;
  web_state_list_.PerformBatchOperation(base::BindOnce(
      [](bool* captured_batch_in_progress, WebStateList* web_state_list) {
        *captured_batch_in_progress = web_state_list->IsBatchInProgress();
      },
      &captured_batch_in_progress));

  EXPECT_FALSE(web_state_list_.IsBatchInProgress());
  EXPECT_TRUE(captured_batch_in_progress);
}
