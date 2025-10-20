// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_header.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_filter_cell.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing between individual filter cells in the collection view.
const CGFloat kFilterCellSpacing = 8.0;
// Content inset for the collection view to provide breathing room.
const CGFloat kContentInset = 16.0;
// Collection view height.
const CGFloat kCollectionViewHeight = 48.0;
// Spacing between collection view and attribution text.
const CGFloat kTextToCollectionSpacing = 20.0;
// Spacing between attribution text and bottom.
const CGFloat kTextToBottomSpacing = 16.0;

// Reuse identifier for filter collection view cells.
NSString* const kFilterCellIdentifier = @"DownloadFilterCell";

}  // namespace

#pragma mark - DownloadListTableViewHeader

@interface DownloadListTableViewHeader () <UICollectionViewDataSource,
                                           UICollectionViewDelegate,
                                           UITextViewDelegate>
@end

@implementation DownloadListTableViewHeader {
  UICollectionView* _filterCollectionView;
  UICollectionViewFlowLayout* _layout;
  NSLayoutConstraint* _filterCollectionViewWidthConstraint;

  // The currently selected filter type. Defaults to kAll.
  DownloadFilterType _selectedFilterType;

  // Array of available filter types to display as NSNumber objects.
  // Contains all supported download filter categories.
  NSArray<NSNumber*>* _availableFilterTypes;

  // UITextView for displaying attribution text with clickable links at the
  // bottom.
  UITextView* _attributionTextView;
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
    [self setupAttributionTextView];
    [self setupConstraints];

    // Initialize with "All" filter selected by default.
    _selectedFilterType = DownloadFilterType::kAll;
    [_filterCollectionView
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
  CGFloat contentWidth = _filterCollectionView.collectionViewLayout
                             .collectionViewContentSize.width;
  CGFloat maxWidth = self.bounds.size.width;
  _filterCollectionViewWidthConstraint.constant = MIN(contentWidth, maxWidth);
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
  _filterCollectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                             collectionViewLayout:_layout];
  _filterCollectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _filterCollectionView.backgroundColor = [UIColor clearColor];
  _filterCollectionView.showsHorizontalScrollIndicator = NO;
  _filterCollectionView.delegate = self;
  _filterCollectionView.dataSource = self;

  [_filterCollectionView registerClass:[DownloadFilterCell class]
            forCellWithReuseIdentifier:kFilterCellIdentifier];
  [self addSubview:_filterCollectionView];
}

// Configures the attribution text view for displaying clickable text at the
// bottom.
- (void)setupAttributionTextView {
  _attributionTextView = CreateUITextViewWithTextKit1();
  _attributionTextView.translatesAutoresizingMaskIntoConstraints = NO;
  _attributionTextView.backgroundColor = [UIColor clearColor];
  _attributionTextView.scrollEnabled = NO;
  _attributionTextView.editable = NO;
  _attributionTextView.delegate = self;
  _attributionTextView.textContainerInset = UIEdgeInsetsZero;
  _attributionTextView.textContainer.lineFragmentPadding = 0;

  // Set up attribution text with clickable link.
  NSString* attributionMessage =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_MESSAGE);
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
    NSLinkAttributeName : GetFilesAppUrl().absoluteString,
  };

  _attributionTextView.attributedText = AttributedStringFromStringWithLink(
      attributionMessage, textAttributes, linkAttributes);

  [self addSubview:_attributionTextView];
}

// Sets up Auto Layout constraints for the collection view positioning and
// sizing.
- (void)setupConstraints {
  // Create a width constraint that will be updated dynamically based on content
  // size.
  _filterCollectionViewWidthConstraint =
      [_filterCollectionView.widthAnchor constraintEqualToConstant:0];
  [NSLayoutConstraint activateConstraints:@[
    [_filterCollectionView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_filterCollectionView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor],
    // Explicit height constraint ensures consistent filter cell sizing.
    [_filterCollectionView.heightAnchor
        constraintEqualToConstant:kCollectionViewHeight],
    _filterCollectionViewWidthConstraint,

    // Attribution text view constraints with correct spacing.
    [_attributionTextView.topAnchor
        constraintEqualToAnchor:_filterCollectionView.bottomAnchor
                       constant:kTextToCollectionSpacing],
    [_attributionTextView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kContentInset],
    [_attributionTextView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kContentInset],
    [_attributionTextView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kTextToBottomSpacing]
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
  [_filterCollectionView
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

#pragma mark - UITextViewDelegate

// Handles link interactions in the attribution text view.
- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  return [UIAction actionWithTitle:@""
                             image:nil
                        identifier:nil
                           handler:^(__kindof UIAction* action) {
                             [self handleAttributionLinkTap:textItem.link];
                           }];
}

// Prevents text selection in the text view.
- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text while allowing interactions with links.
  textView.selectedTextRange = nil;
}

#pragma mark - Private Methods

// Handles taps on the attribution link.
- (void)handleAttributionLinkTap:(NSURL*)url {
  // Handle the attribution link tap with the received URL.
  if (!url) {
    return;
  }
  [[UIApplication sharedApplication] openURL:url
                                     options:@{}
                           completionHandler:nil];
}

#pragma mark - Public Methods

- (CGFloat)preferredHeightForWidth:(CGFloat)width {
  // Calculate the height needed for the attribution text view.
  CGFloat attributionTextHeight = 0.0;
  if (_attributionTextView.attributedText && !_attributionTextView.hidden) {
    CGSize maxSize = CGSizeMake(width - 2 * kContentInset, CGFLOAT_MAX);
    CGRect textRect = [_attributionTextView.attributedText
        boundingRectWithSize:maxSize
                     options:NSStringDrawingUsesLineFragmentOrigin
                     context:nil];
    attributionTextHeight = ceil(textRect.size.height);
  }

  // Total height calculation based on specific spacing requirements.
  CGFloat totalHeight = kCollectionViewHeight;
  if (attributionTextHeight > 0) {
    totalHeight +=
        kTextToCollectionSpacing + attributionTextHeight + kTextToBottomSpacing;
  }

  return totalHeight;
}

- (void)setAttributionTextShown:(BOOL)shown {
  _attributionTextView.hidden = !shown;
  [self invalidateIntrinsicContentSize];
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric,
                    [self preferredHeightForWidth:self.bounds.size.width]);
}

@end
