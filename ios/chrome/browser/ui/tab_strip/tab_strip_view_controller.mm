// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_strip/tab_strip_view_controller.h"

#import "ios/chrome/browser/ui/tab_strip/tab_strip_cell.h"
#import "ios/chrome/browser/ui/tab_strip/tab_strip_view_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabStripViewController

namespace {
static NSString* const kReuseIdentifier = @"TabView";
}  // namespace

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

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  TabStripCell* cell = (TabStripCell*)[collectionView
      dequeueReusableCellWithReuseIdentifier:kReuseIdentifier
                                forIndexPath:indexPath];
  cell.titleLabel.text = nil;
  return cell;
}

@end
