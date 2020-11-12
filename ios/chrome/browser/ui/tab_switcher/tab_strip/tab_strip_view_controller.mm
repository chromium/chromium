// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_controller.h"

#import "base/allocator/partition_allocator/partition_alloc.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
static NSString* const kReuseIdentifier = @"TabView";
}  // namespace

@interface TabStripViewController ()
// Returns the number of tabs, the value is taken from the count() of
// the WebStateList.
@property(nonatomic, assign) NSUInteger tabsCount;
@end

@implementation TabStripViewController

- (instancetype)init {
  TabStripViewLayout* layout = [[TabStripViewLayout alloc] init];
  if (self = [super initWithCollectionViewLayout:layout]) {
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.collectionView.alwaysBounceHorizontal = YES;
  [self.collectionView registerClass:[TabStripCell class]
          forCellWithReuseIdentifier:kReuseIdentifier];
}

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _tabsCount;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  TabStripCell* cell = (TabStripCell*)[collectionView
      dequeueReusableCellWithReuseIdentifier:kReuseIdentifier
                                forIndexPath:indexPath];
  cell.titleLabel.text = nil;
  return cell;
}

#pragma mark - TabStripConsumer

- (void)setTabsCount:(NSUInteger)tabsCount {
  if (_tabsCount == tabsCount)
    return;
  _tabsCount = tabsCount;
  [self.collectionView reloadData];
}

@end
