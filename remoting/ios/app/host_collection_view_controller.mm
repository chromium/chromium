// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_collection_view_controller.h"

#import <MaterialComponents/MaterialCollections.h>
#import <MaterialComponents/MaterialInk.h>
#import <MaterialComponents/MaterialShadowElevations.h>
#import <MaterialComponents/MaterialShadowLayer.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/host_collection_header_view.h"
#include "ui/base/l10n/l10n_util.h"

static NSString* const kReusableIdentifierItem =
    @"remotingHostCollectionViewControllerItem";

static CGFloat kHostCollectionViewControllerCellHeight = 70.f;
static CGFloat kHostCollectionHeaderViewHeight = 25.f;

@implementation HostCollectionViewController

@synthesize delegate = _delegate;
@synthesize scrollViewDelegate = _scrollViewDelegate;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout {
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    self.collectionView.backgroundColor = [UIColor clearColor];
    [self.collectionView registerClass:[HostCollectionViewCell class]
            forCellWithReuseIdentifier:NSStringFromClass(
                                           [HostCollectionViewCell class])];

    [self.collectionView registerClass:[HostCollectionHeaderView class]
            forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                   withReuseIdentifier:UICollectionElementKindSectionHeader];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.styler.cellStyle = MDCCollectionViewCellStyleDefault;
  self.styler.cellLayoutType = MDCCollectionViewCellLayoutTypeList;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return [_delegate getHostCount];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  HostCollectionViewCell* cell =
      [collectionView dequeueReusableCellWithReuseIdentifier:
                          NSStringFromClass([HostCollectionViewCell class])
                                                forIndexPath:indexPath];
  HostInfo* host = [_delegate getHostAtIndexPath:indexPath];
  if (host) {
    [cell populateContentWithHostInfo:host];
  }
  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  HostCollectionHeaderView* supplementaryView =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    supplementaryView.text = l10n_util::GetNSString(IDS_REMOTE_DEVICES_TITLE);
  }
  return supplementaryView;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  HostCollectionViewCell* cell = (HostCollectionViewCell*)[collectionView
      cellForItemAtIndexPath:indexPath];
  [_delegate didSelectCell:cell
                completion:^{
                }];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  return kHostCollectionViewControllerCellHeight;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [_scrollViewDelegate scrollViewDidScroll:scrollView];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  return CGSizeMake(collectionView.bounds.size.width,
                    kHostCollectionHeaderViewHeight);
}

@end
