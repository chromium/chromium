// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeCollectionViewCell : MDCCollectionViewCell
@end

@implementation FakeCollectionViewCell
@end

@interface FakeCollectionViewItem : CollectionViewItem
@property(nonatomic, assign) NSInteger configureCount;
@end
@implementation FakeCollectionViewItem
@synthesize configureCount = _configureCount;
- (void)configureCell:(MDCCollectionViewCell*)cell {
  [super configureCell:cell];
  EXPECT_TRUE([cell isMemberOfClass:[FakeCollectionViewCell class]]);
  self.configureCount++;
}
@end

namespace {

using MDCCollectionViewCellChrome = PlatformTest;

TEST_F(MDCCollectionViewCellChrome, PreferredHeightCallsConfigureCell) {
  FakeCollectionViewItem* item =
      [[FakeCollectionViewItem alloc] initWithType:0];
  item.cellClass = [FakeCollectionViewCell class];
  EXPECT_EQ(0, item.configureCount);

  [MDCCollectionViewCell cr_preferredHeightForWidth:0 forItem:item];

  EXPECT_EQ(1, item.configureCount);
}

}  // namespace
