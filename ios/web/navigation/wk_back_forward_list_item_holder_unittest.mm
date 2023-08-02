// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_back_forward_list_item_holder.h"

#import <WebKit/WebKit.h>

#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

// Test fixture for WKBackForwardListItemHolder class.
typedef PlatformTest WKBackForwardListItemHolderTest;

// Tests that FromNavigationItem returns the same holder for the same
// NavigationItem.
TEST_F(WKBackForwardListItemHolderTest, GetHolderFromNavigationItem) {
  std::unique_ptr<web::NavigationItem> item(NavigationItem::Create());
  WKBackForwardListItemHolder* holder1 =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());
  WKBackForwardListItemHolder* holder2 =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());
  EXPECT_EQ(holder1, holder2);
}

// Tests that FromNavigationItem returns different holders for different
// NavigationItem objects.
TEST_F(WKBackForwardListItemHolderTest, GetHolderFromDifferentNavigationItem) {
  // Create two NavigationItem objects.
  std::unique_ptr<web::NavigationItem> item1(NavigationItem::Create());
  std::unique_ptr<web::NavigationItem> item2(NavigationItem::Create());
  EXPECT_NE(item1.get(), item2.get());

  // Verify that the two objects have different holders.
  WKBackForwardListItemHolder* holder1 =
      WKBackForwardListItemHolder::FromNavigationItem(item1.get());
  WKBackForwardListItemHolder* holder2 =
      WKBackForwardListItemHolder::FromNavigationItem(item2.get());
  EXPECT_NE(holder1, holder2);
}

// Tests that acessors for the WKBackForwardListItem object work as
// expected. The test bellow uses NSObject instead of WKBackForwardListItem
// because WKBackForwardListItem alloc/release is not designed to be called
// directly and will crash.
TEST_F(WKBackForwardListItemHolderTest, GetBackForwardListItemFromHolder) {
  std::unique_ptr<web::NavigationItem> item(NavigationItem::Create());
  NSObject* input = [[NSObject alloc] init];
  WKBackForwardListItemHolder* holder =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());
  holder->set_back_forward_list_item(
      static_cast<WKBackForwardListItem*>(input));
  NSObject* result = holder->back_forward_list_item();
  EXPECT_EQ(input, result);
}

// Tests that acessors for navigation type work as expected.
TEST_F(WKBackForwardListItemHolderTest, GetNavigationTypeFromHolder) {
  std::unique_ptr<web::NavigationItem> item(NavigationItem::Create());
  WKBackForwardListItemHolder* holder =
      WKBackForwardListItemHolder::FromNavigationItem(item.get());

  // Verify that setting 'WKNavigationTypeOther' means
  // `navigation_type` returns WKNavigationTypeBackForward
  WKNavigationType type = WKNavigationTypeOther;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeBackForward' means
  // `navigation_type` returns 'WKNavigationTypeBackForward'
  type = WKNavigationTypeBackForward;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeFormSubmitted' means
  // `navigation_type` returns 'WKNavigationTypeFormSubmitted'
  type = WKNavigationTypeFormSubmitted;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeFormResubmitted' means
  // `navigation_type` returns 'WKNavigationTypeFormResubmitted'
  type = WKNavigationTypeFormResubmitted;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeReload' means
  // `navigation_type` returns 'WKNavigationTypeReload'
  type = WKNavigationTypeReload;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());

  // Verify that setting 'WKNavigationTypeLinkActivated' means
  // `navigation_type` returns 'WKNavigationTypeLinkActivated'
  type = WKNavigationTypeLinkActivated;
  holder->set_navigation_type(type);
  EXPECT_EQ(type, holder->navigation_type());
}

}  // namespace web
