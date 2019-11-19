// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/find_in_page_manager_delegate_bridge.h"

#import "ios/web/find_in_page/find_in_page_manager_impl.h"
#import "ios/web/public/test/fakes/crw_fake_find_in_page_manager_delegate.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture to test FindInPageManagerDelegateBridge class.
class FindInPageManagerDelegateBridgeTest : public PlatformTest {
 protected:
  FindInPageManagerDelegateBridgeTest()
      : delegate_([[CRWFakeFindInPageManagerDelegate alloc] init]),
        bridge_(std::make_unique<FindInPageManagerDelegateBridge>(delegate_)) {
    FindInPageManagerImpl::CreateForWebState(&test_web_state_);
  }

  CRWFakeFindInPageManagerDelegate* delegate_ = nil;
  std::unique_ptr<FindInPageManagerDelegateBridge> bridge_;
  web::TestWebState test_web_state_;
};

// Tests that CRWFindInPageManagerDelegate properly receives values from
// DidHighlightMatches().
TEST_F(FindInPageManagerDelegateBridgeTest, DidHighlightMatches) {
  bridge_->DidHighlightMatches(&test_web_state_, 1, @"foo");
  EXPECT_EQ(1, delegate_.matchCount);
  EXPECT_EQ(@"foo", delegate_.query);
  EXPECT_EQ(&test_web_state_, delegate_.webState);
}

// Tests that CRWFindInPageManagerDelegate properly receives values from
// DidSelectMatch().
TEST_F(FindInPageManagerDelegateBridgeTest, DidSelectMatch) {
  bridge_->DidSelectMatch(&test_web_state_, 1, @"match context");
  EXPECT_EQ(1, delegate_.index);
  EXPECT_EQ(&test_web_state_, delegate_.webState);
  EXPECT_EQ(@"match context", delegate_.contextString);
}

}  // namespace web
