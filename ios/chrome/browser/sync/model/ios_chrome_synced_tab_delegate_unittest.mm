// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/ios_chrome_synced_tab_delegate.h"

#import <memory>

#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using IOSChromeSyncedTabDelegateTest = PlatformTest;

// Regression test for crbug.com/980841: Verifies that the browser does not
// crash if the pending item is null.
TEST_F(IOSChromeSyncedTabDelegateTest, ShouldHandleNullItem) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetPendingItemIndex(0);

  ASSERT_EQ(0, navigation_manager->GetPendingItemIndex());
  ASSERT_EQ(nullptr, navigation_manager->GetPendingItem());

  web::FakeWebState web_state;
  web_state.SetNavigationManager(std::move(navigation_manager));
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  EXPECT_EQ(GURL(), tab_delegate->GetVirtualURLAtIndex(0));
  EXPECT_TRUE(tab_delegate->GetPageLanguageAtIndex(0).empty());
}

}  // namespace
