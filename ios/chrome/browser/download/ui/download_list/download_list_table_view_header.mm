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

  // The currently selected filter type. Defaults to kAll.
  DownloadFilterType _selectedFilterType;

  // Array of available filter types to display as NSNumber objects.
  // Contains all supported download filter categories.
  NSArray<NSNumber*>* _availableFilterTypes;

  // UITextView for displaying attribution text with clickable links at the
  // bottom.
  UITextView* _attributionTextView;

  // Constraint to manage the bottom spacing of the collection view.
  // Active when attribution text is hidden.
  NSLayoutConstraint* _collectionViewBottomConstraint;

  // Constraints for the attribution text view to manage its visibility and
  // layout. Active when attribution text is shown.
  NSArray<NSLayoutConstraint*>* _attributionTextViewConstraints;
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
  // Collection view constraints.
  _collectionViewBottomConstraint = [_filterCollectionView.bottomAnchor
      constraintEqualToAnchor:self.bottomAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [_filterCollectionView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_filterCollectionView.leadingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor],
    [_filterCollectionView.trailingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor],
    // Explicit height constraint ensures consistent filter cell sizing.
    [_filterCollectionView.heightAnchor
        constraintEqualToConstant:kCollectionViewHeight],
  ]];

  // Attribution text view constraints.
  _attributionTextViewConstraints = @[
    [_attributionTextView.topAnchor
        constraintEqualToAnchor:_filterCollectionView.bottomAnchor
                       constant:kTextToCollectionSpacing],
    [_attributionTextView.leadingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor
                       constant:kContentInset],
    [_attributionTextView.trailingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor
                       constant:-kContentInset],
    [_attributionTextView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kTextToBottomSpacing]
  ];
  [NSLayoutConstraint activateConstraints:_attributionTextViewConstraints];
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

- (void)setAttributionTextShown:(BOOL)shown {
  _attributionTextView.hidden = !shown;
  if (shown) {
    _collectionViewBottomConstraint.active = NO;
    [NSLayoutConstraint activateConstraints:_attributionTextViewConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_attributionTextViewConstraints];
    _collectionViewBottomConstraint.active = YES;
  }
}

@end
