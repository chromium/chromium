// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/tab_id_tab_helper.h"

#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TabIdTabHelper class.
class TabIdTabHelperTest : public PlatformTest {
 protected:
  web::FakeWebState first_web_state_;
  web::FakeWebState second_web_state_;
};

// Tests that a tab ID is returned for a WebState, and tab ID's are different
// for different WebStates.
TEST_F(TabIdTabHelperTest, UniqueIdentifiers) {
  TabIdTabHelper::CreateForWebState(&first_web_state_);
  TabIdTabHelper::CreateForWebState(&second_web_state_);

  const NSString* first_tab_id =
      TabIdTabHelper::FromWebState(&first_web_state_)->tab_id();
  const NSString* second_tab_id =
      TabIdTabHelper::FromWebState(&second_web_state_)->tab_id();

  EXPECT_GT([first_tab_id length], 0U);
  EXPECT_GT([second_tab_id length], 0U);
  EXPECT_NSNE(first_tab_id, second_tab_id);
}

// Tests that a tab ID is stable across successive calls.
TEST_F(TabIdTabHelperTest, StableAcrossCalls) {
  TabIdTabHelper::CreateForWebState(&first_web_state_);
  TabIdTabHelper* tab_helper = TabIdTabHelper::FromWebState(&first_web_state_);

  const NSString* first_call_id = tab_helper->tab_id();
  const NSString* second_call_id = tab_helper->tab_id();

  EXPECT_NSEQ(first_call_id, second_call_id);
}

// Tests serialization of a tab ID.
TEST_F(TabIdTabHelperTest, Serialization) {
  TabIdTabHelper::CreateForWebState(&first_web_state_);
  TabIdTabHelper* first_tab_helper =
      TabIdTabHelper::FromWebState(&first_web_state_);
  const NSString* first_call_id = first_tab_helper->tab_id();

  first_tab_helper->RemoveFromWebState(&first_web_state_);

  TabIdTabHelper::CreateForWebState(&first_web_state_);
  TabIdTabHelper* second_tab_helper =
      TabIdTabHelper::FromWebState(&first_web_state_);
  const NSString* second_call_id = second_tab_helper->tab_id();

  EXPECT_NSEQ(first_call_id, second_call_id);
}
