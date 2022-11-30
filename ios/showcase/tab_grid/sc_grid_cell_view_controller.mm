// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_grid_cell_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCellIdentifier = @"GridCellIdentifier";
}  // namespace

@interface SCGridCellViewController ()<UICollectionViewDataSource,
                                       UICollectionViewDelegate>
@property(nonatomic, strong) NSArray* sizes;
@property(nonatomic, strong) NSIndexPath* selectedIndexPath;
@property(nonatomic, strong) ProtocolAlerter* alerter;
@end

@implementation SCGridCellViewController
@synthesize sizes = _sizes;
@synthesize selectedIndexPath = _selectedIndexPath;
@synthesize alerter = _alerter;

- (instancetype)init {
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.sectionInset = UIEdgeInsetsMake(20.0f, 20.0f, 20.0f, 20.0f);
  layout.minimumInteritemSpacing = 15.0f;
  layout.minimumLineSpacing = 15.0f;
  if (self = [super initWithCollectionViewLayout:layout]) {
    _alerter = [[ProtocolAlerter alloc]
        initWithProtocols:@[ @protocol(GridCellDelegate) ]];
    self.alerter.baseViewController = self;
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    self.collectionView.allowsMultipleSelection = YES;
    [self.collectionView registerClass:[GridCell class]
            forCellWithReuseIdentifier:kCellIdentifier];
    self.collectionView.backgroundColor = [UIColor blackColor];
    _sizes = @[
      [NSValue valueWithCGSize:CGSizeMake(140.0f, 168.0f)],
      [NSValue valueWithCGSize:CGSizeMake(180.0f, 208.0f)],
      [NSValue valueWithCGSize:CGSizeMake(220.0f, 248.0f)],
      [NSValue valueWithCGSize:CGSizeMake(180.0f, 208.0f)],
    ];
  }
  return self;
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return base::checked_cast<NSInteger>(self.sizes.count);
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                forIndexPath:indexPath]);
  cell.delegate = static_cast<id<GridCellDelegate>>(self.alerter);
  if (indexPath.item == 1)
    cell.theme = GridThemeDark;
  else
    cell.theme = GridThemeLight;
  cell.title = @"YouTube - Trending videos";
  cell.icon = [UIImage imageNamed:@"Icon-180"];
  cell.snapshot = [UIImage imageNamed:@"Sample-screenshot-portrait"];
  return cell;
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self.sizes[indexPath.item] CGSizeValue];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [collectionView deselectItemAtIndexPath:self.selectedIndexPath animated:NO];
  self.selectedIndexPath = indexPath;
}

@end
