// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_navigation_item_holder.h"

#import "ios/web/navigation/navigation_item_impl.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Test fixture for CRWNavigationItemHolder.
typedef PlatformTest CRWNavigationItemHolderTest;

TEST_F(CRWNavigationItemHolderTest, NewHolderCreatedAutomatically) {
  WKBackForwardListItem* item = OCMClassMock([WKBackForwardListItem class]);
  CRWNavigationItemHolder* holder =
      [CRWNavigationItemHolder holderForBackForwardListItem:item];
  ASSERT_NSNE(holder, nil);
  EXPECT_TRUE([holder navigationItem] == nullptr);
}

// Tests that stored NavigationItemImpl can be retrieved.
TEST_F(CRWNavigationItemHolderTest, SetNavigationItem) {
  GURL url("http://www.0.com");
  auto navigation_item = std::make_unique<web::NavigationItemImpl>();
  navigation_item->SetURL(url);

  WKBackForwardListItem* item = OCMClassMock([WKBackForwardListItem class]);
  [[CRWNavigationItemHolder holderForBackForwardListItem:item]
      setNavigationItem:std::move(navigation_item)];

  CRWNavigationItemHolder* holder =
      [CRWNavigationItemHolder holderForBackForwardListItem:item];

  ASSERT_NSNE(holder, nil);
  ASSERT_TRUE([holder navigationItem] != nullptr);
  EXPECT_EQ(url, [holder navigationItem]->GetURL());
}

// Tests that each WKBackForwardListItem has its unique CRWNavigationItemHolder.
TEST_F(CRWNavigationItemHolderTest, OneHolderPerWKItem) {
  GURL url1("http://www.1.com");
  auto navigation_item1 = std::make_unique<web::NavigationItemImpl>();
  navigation_item1->SetURL(url1);
  WKBackForwardListItem* item1 = OCMClassMock([WKBackForwardListItem class]);
  [[CRWNavigationItemHolder holderForBackForwardListItem:item1]
      setNavigationItem:std::move(navigation_item1)];

  GURL url2("http://www.2.com");
  auto navigation_item2 = std::make_unique<web::NavigationItemImpl>();
  navigation_item2->SetURL(url2);
  WKBackForwardListItem* item2 = OCMClassMock([WKBackForwardListItem class]);
  [[CRWNavigationItemHolder holderForBackForwardListItem:item2]
      setNavigationItem:std::move(navigation_item2)];

  ASSERT_NSNE(item1, item2);

  CRWNavigationItemHolder* holder1 =
      [CRWNavigationItemHolder holderForBackForwardListItem:item1];
  CRWNavigationItemHolder* holder2 =
      [CRWNavigationItemHolder holderForBackForwardListItem:item2];

  EXPECT_NSNE(holder1, holder2);
  ASSERT_TRUE(holder1.navigationItem);
  EXPECT_EQ(url1, holder1.navigationItem->GetURL());
  ASSERT_TRUE(holder2.navigationItem);
  EXPECT_EQ(url2, holder2.navigationItem->GetURL());
}
