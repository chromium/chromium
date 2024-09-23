// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"

#import <memory>
#import <vector>

#import "base/containers/contains.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class TestObserver : public ContextualPanelTabHelperObserver {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  bool WasInvokedFor(ContextualPanelTabHelper* tab_helper) {
    return base::Contains(invoker_tab_helpers_, tab_helper);
  }

  void Reset() { invoker_tab_helpers_.clear(); }

  // ContextualPanelTabHelperObserver.
  void ContextualPanelHasNewData(
      ContextualPanelTabHelper* tab_helper,
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) override {
    invoker_tab_helpers_.push_back(tab_helper);
  }

 private:
  std::vector<ContextualPanelTabHelper*> invoker_tab_helpers_;
};

class ActiveContextualPanelTabHelperObservationForwarderTest
    : public PlatformTest {
 public:
  ActiveContextualPanelTabHelperObservationForwarderTest()
      : web_state_list_(&web_state_list_delegate_) {
    forwarder_ =
        std::make_unique<ActiveContextualPanelTabHelperObservationForwarder>(
            &web_state_list_, &observer_);
  }

  web::FakeWebState* AddWebStateToList(bool activate) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* web_state_ptr = web_state.get();
    std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
    ContextualPanelTabHelper::CreateForWebState(web_state_ptr, models);
    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(activate));
    return web_state_ptr;
  }

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  TestObserver observer_;
  std::unique_ptr<ActiveContextualPanelTabHelperObservationForwarder>
      forwarder_;
};

}  // namespace

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestInsertActiveWebState) {
  // Insert two webstates into the list and mark the second one active.  Send
  // tab helper observer notifications for both and verify the result.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);

  // The observer should only be notified for the active tab helper B.
  ContextualPanelTabHelper* tab_helper_a =
      ContextualPanelTabHelper::FromWebState(web_state_a);
  ContextualPanelTabHelper* tab_helper_b =
      ContextualPanelTabHelper::FromWebState(web_state_b);
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_b));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_a));
}

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestInsertNonActiveWebState) {
  // Insert two webstates into the list, but do not mark the second one active.
  // Send tab helper observer notifications for both and verify the result.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(false);
  ASSERT_EQ(web_state_a, web_state_list_.GetActiveWebState());

  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);

  // The observer should only be notified for the active web state A.
  ContextualPanelTabHelper* tab_helper_a =
      ContextualPanelTabHelper::FromWebState(web_state_a);
  ContextualPanelTabHelper* tab_helper_b =
      ContextualPanelTabHelper::FromWebState(web_state_b);
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_a));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_b));
}

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestDetachActiveWebState) {
  // Insert three webstates into the list.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(true);
  web::FakeWebState* web_state_c = AddWebStateToList(true);
  ASSERT_EQ(web_state_c, web_state_list_.GetActiveWebState());

  // Remove the active web state and send tab helper observer notifications.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(web_state_list_.active_index());
  web::WebState* active_web_state = web_state_list_.GetActiveWebState();
  web::WebState* non_active_web_state =
      (active_web_state == web_state_a ? web_state_b : web_state_a);

  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);
  web_state_c->OnNavigationStarted(&context);

  // The observer should only be notified for the new active web state.
  ContextualPanelTabHelper* active_tab_helper =
      ContextualPanelTabHelper::FromWebState(active_web_state);
  ContextualPanelTabHelper* non_active_tab_helper =
      ContextualPanelTabHelper::FromWebState(non_active_web_state);
  ContextualPanelTabHelper* tab_helper_c =
      ContextualPanelTabHelper::FromWebState(web_state_c);
  EXPECT_TRUE(observer_.WasInvokedFor(active_tab_helper));
  EXPECT_FALSE(observer_.WasInvokedFor(non_active_tab_helper));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_c));
}

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestDetachNonActiveWebState) {
  // Insert three webstates into the list.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(true);
  web::FakeWebState* web_state_c = AddWebStateToList(true);
  ASSERT_EQ(web_state_c, web_state_list_.GetActiveWebState());

  // Remove a non-active web state and send tab helper observer notifications.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(
          web_state_list_.GetIndexOfWebState(web_state_a));
  ASSERT_EQ(web_state_c, web_state_list_.GetActiveWebState());

  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);
  web_state_c->OnNavigationStarted(&context);

  // The observer should only be notified for the active web state.
  ContextualPanelTabHelper* tab_helper_a =
      ContextualPanelTabHelper::FromWebState(web_state_a);
  ContextualPanelTabHelper* tab_helper_b =
      ContextualPanelTabHelper::FromWebState(web_state_b);
  ContextualPanelTabHelper* tab_helper_c =
      ContextualPanelTabHelper::FromWebState(web_state_c);
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_c));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_a));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_b));
}

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestReplaceActiveWebState) {
  // Insert two webstates into the list and mark the second one active.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  // Replace the active web state.  Send notifications and verify the result.
  auto replacement_web_state = std::make_unique<web::FakeWebState>();

  web::FakeWebState* web_state_c = replacement_web_state.get();
  std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
  ContextualPanelTabHelper::CreateForWebState(web_state_c, models);
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.ReplaceWebStateAt(
          web_state_list_.GetIndexOfWebState(web_state_b),
          std::move(replacement_web_state));

  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);
  web_state_c->OnNavigationStarted(&context);

  // The observer should only be notified for the new active web state C.
  ContextualPanelTabHelper* tab_helper_a =
      ContextualPanelTabHelper::FromWebState(web_state_a);
  ContextualPanelTabHelper* tab_helper_b =
      ContextualPanelTabHelper::FromWebState(web_state_b);
  ContextualPanelTabHelper* tab_helper_c =
      ContextualPanelTabHelper::FromWebState(web_state_c);
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_c));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_a));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_b));
}

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestChangeActiveWebState) {
  // Insert two webstates into the list and mark the second one active.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  // Make web state A active and send notifications.
  web_state_list_.ActivateWebStateAt(
      web_state_list_.GetIndexOfWebState(web_state_a));

  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);

  // The observer should only be notified for the active web state A.
  ContextualPanelTabHelper* tab_helper_a =
      ContextualPanelTabHelper::FromWebState(web_state_a);
  ContextualPanelTabHelper* tab_helper_b =
      ContextualPanelTabHelper::FromWebState(web_state_b);
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_a));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_b));

  // Make web state B active and send notifications.
  observer_.Reset();
  web_state_list_.ActivateWebStateAt(
      web_state_list_.GetIndexOfWebState(web_state_b));
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);

  // The observer should only be notified for the active web state B.
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_b));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_a));
}

TEST_F(ActiveContextualPanelTabHelperObservationForwarderTest,
       TestNonEmptyInitialWebStateList) {
  // Insert two webstates into the list.
  web::FakeWebState* web_state_a = AddWebStateToList(true);
  web::FakeWebState* web_state_b = AddWebStateToList(true);
  ASSERT_EQ(web_state_b, web_state_list_.GetActiveWebState());

  // Recreate the multi observer to simulate creation with an already-populated
  // WebStateList.
  forwarder_.reset();
  forwarder_ =
      std::make_unique<ActiveContextualPanelTabHelperObservationForwarder>(
          &web_state_list_, &observer_);

  // Send notifications and verify the result.
  web::FakeNavigationContext context;
  web_state_a->OnNavigationStarted(&context);
  web_state_b->OnNavigationStarted(&context);

  // The observer should only be notified for the active web state B.
  ContextualPanelTabHelper* tab_helper_a =
      ContextualPanelTabHelper::FromWebState(web_state_a);
  ContextualPanelTabHelper* tab_helper_b =
      ContextualPanelTabHelper::FromWebState(web_state_b);
  EXPECT_TRUE(observer_.WasInvokedFor(tab_helper_b));
  EXPECT_FALSE(observer_.WasInvokedFor(tab_helper_a));
}
