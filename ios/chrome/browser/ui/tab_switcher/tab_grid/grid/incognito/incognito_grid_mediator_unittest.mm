// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"

class IncognitoGridMediatorTest : public GridMediatorTestClass {
 public:
  IncognitoGridMediatorTest() {}
  ~IncognitoGridMediatorTest() override {}

  void SetUp() override {
    GridMediatorTestClass::SetUp();
    mediator_ = [[IncognitoGridMediator alloc] initWithConsumer:consumer_];
    mediator_.browser = browser_.get();
  }

  void TearDown() override {
    // Forces the IncognitoGridMediator to removes its Observer from
    // WebStateList before the Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

 protected:
  IncognitoGridMediator* mediator_;
};

// Tests that the WebStateList and consumer's list are empty when
// `-closeAllItems` is called.
TEST_F(IncognitoGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}
