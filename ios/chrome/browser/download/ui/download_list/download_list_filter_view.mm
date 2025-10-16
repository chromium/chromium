// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_filter_view.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_filter_cell.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Padding between the filter view and its parent container.
const CGFloat kFilterViewPadding = 8.0;
// Spacing between individual filter cells in the collection view.
const CGFloat kFilterCellSpacing = 8.0;
// Content inset for the collection view to provide breathing room.
const CGFloat kContentInset = 16.0;

// Reuse identifier for filter collection view cells.
NSString* const kFilterCellIdentifier = @"DownloadFilterCell";

}  // namespace

#pragma mark - DownloadListFilterView

@interface DownloadListFilterView () <UICollectionViewDataSource,
                                      UICollectionViewDelegate>
@end

@implementation DownloadListFilterView {
  UICollectionView* _collectionView;
  UICollectionViewFlowLayout* _layout;
  NSLayoutConstraint* _collectionViewWidthConstraint;

  // The currently selected filter type. Defaults to kAll.
  DownloadFilterType _selectedFilterType;

  // Array of available filter types to display as NSNumber objects.
  // Contains all supported download filter categories.
  NSArray<NSNumber*>* _availableFilterTypes;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _availableFilterTypes = @[
      @((int)DownloadFilterType::kAll), @((int)DownloadFilterType::kDocument),
      @((int)DownloadFilterType::kImage), @((int)DownloadFilterType::kVideo),
      @((int)DownloadFilterType::kAudio), @((int)DownloadFilterType::kPDF),
      @((int)DownloadFilterType::kOther)
    ];
    [self setupCollectionView];
    [self setupConstraints];

    // Initialize with "All" filter selected by default.
    _selectedFilterType = DownloadFilterType::kAll;
    [_collectionView
        selectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                     animated:NO
               scrollPosition:
                   UICollectionViewScrollPositionCenteredHorizontally];
  }
  return self;
}

- (instancetype)init {
  return [self initWithFrame:CGRectZero];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGFloat contentWidth =
      _collectionView.collectionViewLayout.collectionViewContentSize.width;
  CGFloat maxWidth = self.bounds.size.width - 2 * kFilterViewPadding;
  _collectionViewWidthConstraint.constant = MIN(contentWidth, maxWidth);
}

#pragma mark - Private Methods

// Configures the collection view and its flow layout for horizontal scrolling
// filter display.
- (void)setupCollectionView {
  // Configure flow layout for horizontal scrolling with appropriate spacing and
  // insets.
  _layout = [[UICollectionViewFlowLayout alloc] init];
  _layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  _layout.sectionInset = UIEdgeInsetsMake(0, kContentInset, 0, kContentInset);
  _layout.minimumInteritemSpacing = kFilterCellSpacing;
  _layout.minimumLineSpacing = kFilterCellSpacing;

  // Initialize collection view with transparent background and no scroll
  // indicators.
  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:_layout];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.backgroundColor = [UIColor clearColor];
  _collectionView.showsHorizontalScrollIndicator = NO;
  _collectionView.delegate = self;
  _collectionView.dataSource = self;

  [_collectionView registerClass:[DownloadFilterCell class]
      forCellWithReuseIdentifier:kFilterCellIdentifier];
  [self addSubview:_collectionView];
}

// Sets up Auto Layout constraints for the collection view positioning and
// sizing.
- (void)setupConstraints {
  // Create a width constraint that will be updated dynamically based on content
  // size.
  _collectionViewWidthConstraint =
      [_collectionView.widthAnchor constraintEqualToConstant:0];
  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.topAnchor constraintEqualToAnchor:self.topAnchor
                                              constant:kFilterViewPadding],
    [_collectionView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    // Explicit height constraint ensures consistent filter cell sizing.
    [_collectionView.heightAnchor
        constraintEqualToConstant:[DownloadFilterCell cellHeight]],
    _collectionViewWidthConstraint
  ]];
}

// Returns the index of the specified filter type in the available filter types
// array.
- (NSInteger)indexForFilterType:(DownloadFilterType)filterType {
  NSNumber* filterNumber = @(static_cast<int>(filterType));
  return [_availableFilterTypes indexOfObject:filterNumber];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _availableFilterTypes.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  DownloadFilterCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kFilterCellIdentifier
                                forIndexPath:indexPath];

  NSNumber* filterNumber = _availableFilterTypes[indexPath.item];
  DownloadFilterType filterType =
      static_cast<DownloadFilterType>(filterNumber.intValue);
  [cell configureWithFilterType:filterType];

  // Set selection state.
  BOOL isSelected = (filterType == _selectedFilterType);
  cell.selected = isSelected;

  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSNumber* filterNumber = _availableFilterTypes[indexPath.item];
  DownloadFilterType filterType =
      static_cast<DownloadFilterType>(filterNumber.intValue);
  if (filterType == _selectedFilterType) {
    return;
  }

  // Update the internal state to reflect the new selection.
  _selectedFilterType = filterType;

  // Ensure the selected item is visible by scrolling to it with animation.
  [_collectionView
      scrollToItemAtIndexPath:indexPath
             atScrollPosition:UICollectionViewScrollPositionCenteredHorizontally
                     animated:YES];

  // Notify the mutator that the filter selection has changed.
  [self.mutator filterRecordsWithType:filterType];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  NSNumber* filterNumber = _availableFilterTypes[indexPath.item];
  DownloadFilterType filterType =
      static_cast<DownloadFilterType>(filterNumber.intValue);

  // Use the cell's width calculation method and standard height.
  CGFloat width = [DownloadFilterCell cellSizeForFilterType:filterType];
  CGFloat height = [DownloadFilterCell cellHeight];
  return CGSizeMake(width, height);
}

@end
