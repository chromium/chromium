// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TestObserver : public web::WebStateObserver {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  bool WasInvokedFor(web::WebState* web_state) {
    return std::find(invoker_web_states_.begin(), invoker_web_states_.end(),
                     web_state) != invoker_web_states_.end();
  }

  void Reset() { invoker_web_states_.clear(); }

  // web::WebStateObserver.
  void RenderProcessGone(web::WebState* web_state) override {
    invoker_web_states_.push_back(web_state);
  }

 private:
  std::vector<web::WebState*> invoker_web_states_;
};

class AllWebStateObservationForwarderTest : public PlatformTest,
                                            public WebStateListDelegate {
 public:
  AllWebStateObservationForwarderTest() : web_state_list_(this) {
    forwarder_ = std::make_unique<AllWebStateObservationForwarder>(
        &web_state_list_, &observer_);
  }

  web::TestWebState* AddWebStateToList(bool activate) {
    std::unique_ptr<web::TestWebState> web_state(
        std::make_unique<web::TestWebState>());
    web::TestWebState* web_state_ptr = web_state.get();
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   activate ? WebStateList::INSERT_ACTIVATE
                                            : WebStateList::INSERT_NO_FLAGS,
                                   WebStateOpener());
    return web_state_ptr;
  }

  // WebStateListDelegate.
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}

 protected:
  WebStateList web_state_list_;
  TestObserver observer_;
  std::unique_ptr<AllWebStateObservationForwarder> forwarder_;
};

}  // namespace

TEST_F(AllWebStateObservationForwarderTest, TestInsertActiveWebState) {
  // Insert two webstates into the list and mark the second one active.  Send
  // observer notifications for both and verify the result.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();

  // The observer should get notifications for both web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));
}

TEST_F(AllWebStateObservationForwarderTest, TestInsertNonActiveWebState) {
  // Insert two webstates into the list, but do not mark the second one active.
  // Send observer notifications for both and verify the result.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(false);
  ASSERT_EQ(web_state_a, web_state_list_.GetActiveWebState());

  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();

  // The observer should get notifications for both web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));
}

TEST_F(AllWebStateObservationForwarderTest, TestDetachActiveWebState) {
  // Insert three webstates into the list.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(true);
  web::TestWebState* web_state_c = AddWebStateToList(true);
  ASSERT_EQ(web_state_c, web_state_list_.GetActiveWebState());

  // Remove the active web state and send observer notifications.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(web_state_list_.active_index());

  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();
  web_state_c->OnRenderProcessGone();

  // The observer should get notifications for the two remaining web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));
  EXPECT_FALSE(observer_.WasInvokedFor(web_state_c));
}

TEST_F(AllWebStateObservationForwarderTest, TestDetachNonActiveWebState) {
  // Insert three webstates into the list.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(true);
  web::TestWebState* web_state_c = AddWebStateToList(true);
  ASSERT_EQ(web_state_c, web_state_list_.GetActiveWebState());

  // Remove a non-active web state and send observer notifications.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(
          web_state_list_.GetIndexOfWebState(web_state_a));
  ASSERT_EQ(web_state_c, web_state_list_.GetActiveWebState());

  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();
  web_state_c->OnRenderProcessGone();

  // The observer should get notifications for the two remaining web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_c));
  EXPECT_FALSE(observer_.WasInvokedFor(web_state_a));
}

TEST_F(AllWebStateObservationForwarderTest, TestReplaceActiveWebState) {
  // Insert two webstates into the list and mark the second one active.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  // Replace the active web state.  Send notifications and verify the result.
  std::unique_ptr<web::TestWebState> replacement_web_state(
      std::make_unique<web::TestWebState>());
  web::TestWebState* web_state_c = replacement_web_state.get();

  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.ReplaceWebStateAt(
          web_state_list_.GetIndexOfWebState(web_state_b),
          std::move(replacement_web_state));

  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();
  web_state_c->OnRenderProcessGone();

  // The observer should get notifications for the two remaining web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_c));
  EXPECT_FALSE(observer_.WasInvokedFor(web_state_b));
}

TEST_F(AllWebStateObservationForwarderTest, TestChangeActiveWebState) {
  // Insert two webstates into the list and mark the second one active.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  // Make web state A active and send notifications.
  web_state_list_.ActivateWebStateAt(
      web_state_list_.GetIndexOfWebState(web_state_a));
  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();

  // The observer should get notifications for both web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));

  // Make web state B active and send notifications.
  observer_.Reset();
  web_state_list_.ActivateWebStateAt(
      web_state_list_.GetIndexOfWebState(web_state_b));
  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();

  // The observer should get notifications for both web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));
}

TEST_F(AllWebStateObservationForwarderTest, TestNonEmptyInitialWebStateList) {
  // Insert two webstates into the list.
  web::TestWebState* web_state_a = AddWebStateToList(true);
  web::TestWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  // Recreate the multi observer to simulate creation with an already-populated
  // WebStateList.
  forwarder_.reset();
  forwarder_ = std::make_unique<AllWebStateObservationForwarder>(
      &web_state_list_, &observer_);

  // Send notifications and verify the result.
  web_state_a->OnRenderProcessGone();
  web_state_b->OnRenderProcessGone();

  // The observer should get notifications for both web states.
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_a));
  EXPECT_TRUE(observer_.WasInvokedFor(web_state_b));
}
