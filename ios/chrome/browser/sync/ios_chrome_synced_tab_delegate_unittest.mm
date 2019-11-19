// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"

#include <memory>

#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using IOSChromeSyncedTabDelegateTest = PlatformTest;

// Regression test for crbug.com/980841: Verifies that the browser does not
// crash if the pending item is null.
TEST_F(IOSChromeSyncedTabDelegateTest, ShouldHandleNullItem) {
  auto navigation_manager = std::make_unique<web::TestNavigationManager>();
  navigation_manager->SetPendingItemIndex(0);

  ASSERT_EQ(0, navigation_manager->GetPendingItemIndex());
  ASSERT_EQ(nullptr, navigation_manager->GetPendingItem());

  web::TestWebState web_state;
  web_state.SetNavigationManager(std::move(navigation_manager));
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  EXPECT_EQ(GURL(), tab_delegate->GetVirtualURLAtIndex(0));
  EXPECT_EQ(GURL(), tab_delegate->GetFaviconURLAtIndex(0));
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      ui::PAGE_TRANSITION_LINK, tab_delegate->GetTransitionAtIndex(0)));
  EXPECT_TRUE(tab_delegate->GetPageLanguageAtIndex(0).empty());
}

}  // namespace
