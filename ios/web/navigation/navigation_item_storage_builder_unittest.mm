// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_storage_builder.h"

#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using NavigationItemStorageBuilderTest = PlatformTest;

namespace web {

TEST_F(NavigationItemStorageBuilderTest, DecodeDifferentScheme) {
  NavigationItemStorageBuilder item_storage_builder;
  CRWNavigationItemStorage* item_storage =
      [[CRWNavigationItemStorage alloc] init];

  // HTTP url.
  [item_storage setURL:GURL("http://url.test")];
  [item_storage setVirtualURL:GURL("http://virtual.test")];

  ASSERT_NE(item_storage.URL, item_storage.virtualURL);

  std::unique_ptr<NavigationItemImpl> navigation_item =
      item_storage_builder.BuildNavigationItemImpl(item_storage);
  ASSERT_EQ(item_storage.URL, navigation_item->GetURL());
  ASSERT_EQ(item_storage.virtualURL, navigation_item->GetVirtualURL());

  // File URL.
  [item_storage setURL:GURL("file://myfile.test")];

  ASSERT_NE(item_storage.URL, item_storage.virtualURL);

  navigation_item = item_storage_builder.BuildNavigationItemImpl(item_storage);
  ASSERT_EQ(item_storage.virtualURL, navigation_item->GetURL());
  ASSERT_EQ(item_storage.virtualURL, navigation_item->GetVirtualURL());

  // Blob URL.
  [item_storage setURL:GURL("blob:myfile.test")];

  ASSERT_NE(item_storage.URL, item_storage.virtualURL);

  navigation_item = item_storage_builder.BuildNavigationItemImpl(item_storage);
  ASSERT_EQ(item_storage.virtualURL, navigation_item->GetURL());
  ASSERT_EQ(item_storage.virtualURL, navigation_item->GetVirtualURL());
}

}  // namespace web
