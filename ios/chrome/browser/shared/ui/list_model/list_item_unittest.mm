// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/list_model/list_item.h"

#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ListItemTest = PlatformTest;

TEST_F(ListItemTest, Accessors) {
  ListItem* five = [[ListItem alloc] initWithType:5];
  ListItem* twelve = [[ListItem alloc] initWithType:12];

  EXPECT_EQ(5, [five type]);
  EXPECT_EQ(12, [twelve type]);

  // Test setting the type property.
  [five setType:55];
  EXPECT_EQ(55, [five type]);

  [twelve setType:1212];
  EXPECT_EQ(1212, [twelve type]);
}

}  // namespace
