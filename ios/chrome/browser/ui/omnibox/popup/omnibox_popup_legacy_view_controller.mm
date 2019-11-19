// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_legacy_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller+internal.h"

#include <memory>

#include "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/image_retriever.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#import "ios/chrome/browser/ui/omnibox/popup/self_sizing_table_view.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#include "ios/chrome/browser/ui/util/animation_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kRowCount = 12;
const CGFloat kRowHeight = 48.0;
const CGFloat kAnswerRowHeight = 64.0;
}  // namespace

@interface OmniboxPopupLegacyViewController () <
    OmniboxPopupRowAccessibilityDelegate> {
  // Array containing the OmniboxPopupRow objects displayed in the view.
  NSArray* _rows;
}

// A flag to track if since the last viewWillAppear, the view ever adopted a
// non-zero size. This is a pretty sad workaround for the new iOS 13 behaviour
// where the half-autolayout, half-manual layout code of this legacy class ends
// up sizing cells to a zero width because -layoutRows is never called on the
// first appearance. This should be removed, together with this class, when the
// non-legacy OmniboxPopupViewController becomes the default.
@property(nonatomic, assign) BOOL viewHadNonZeroWidth;

@end

@implementation OmniboxPopupLegacyViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Cache fonts needed for omnibox attributed string.
  NSMutableArray* rowsBuilder = [[NSMutableArray alloc] init];
  for (int i = 0; i < kRowCount; i++) {
    OmniboxPopupRow* row =
        [[OmniboxPopupRow alloc] initWithIncognito:self.incognito];
    row.accessibilityIdentifier =
        [NSString stringWithFormat:@"omnibox suggestion %i", i];
    row.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    [rowsBuilder addObject:row];
    [row.trailingButton addTarget:self
                           action:@selector(trailingButtonTapped:)
                 forControlEvents:UIControlEventTouchUpInside];
    [row.trailingButton setTag:i];
    row.rowNumber = i;
    row.delegate = self;
    row.rowHeight = kRowHeight;
  }
  _rows = [rowsBuilder copy];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if (![self isViewLoaded]) {
    _rows = nil;
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self layoutRows];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [self layoutRows];
      }
                      completion:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (self.view.bounds.size.width == 0) {
    self.viewHadNonZeroWidth = NO;
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // This method will be called multiple times, including after the self-sizing
  // table view will have taken its final (non-zero) size. Calling -layoutRows
  // will result in another viewDidLayoutSubviews call, so a flag is necessary
  // to avoid an infinite loop.
  if (self.view.bounds.size.width > 0 && !self.viewHadNonZeroWidth) {
    self.viewHadNonZeroWidth = YES;
    [self layoutRows];
  }
}

#pragma mark -
#pragma mark Updating data and UI

- (void)updateRow:(OmniboxPopupRow*)row
        withMatch:(id<AutocompleteSuggestion>)match {
  CGFloat kTextCellLeadingPadding =
      [self showsLeadingIcons] ? ([self useRegularWidthOffset] ? 192 : 100)
                               : 16;
  kTextCellLeadingPadding = [self showsLeadingIcons] ? 221 : 24;

  const CGFloat kTextCellTopPadding = 6;
  const CGFloat kDetailCellTopPadding = 26;
  const CGFloat kTextLabelHeight = 24;
  const CGFloat kTextDetailLabelHeight = 22;
  const CGFloat kTrailingButtonWidth = 40;
  const CGFloat kAnswerLabelHeight = 36;
  const CGFloat kAnswerImageWidth = 30;
  const CGFloat kAnswerImageLeftPadding = -1;
  const CGFloat kAnswerImageRightPadding = 4;
  const CGFloat kAnswerImageTopPadding = 2;
  const BOOL alignmentRight = self.alignment == NSTextAlignmentRight;

  BOOL LTRTextInRTLLayout =
      self.alignment == NSTextAlignmentLeft && UseRTLLayout();

  row.rowHeight = match.hasAnswer ? kAnswerRowHeight : kRowHeight;

  // Fetch the answer image if specified.  Currently, no answer types specify an
  // image on the first line so for now we only look at the second line.
  if (match.hasImage) {
    [self.imageRetriever fetchImage:match.imageURL
                         completion:^(UIImage* image) {
                           row.answerImageView.image = image;
                         }];
    // Answers in suggest do not support RTL, left align only.
    CGFloat imageLeftPadding =
        kTextCellLeadingPadding + kAnswerImageLeftPadding;
    if (alignmentRight) {
      imageLeftPadding =
          row.frame.size.width - (kAnswerImageWidth + kTrailingButtonWidth);
    }
    CGFloat imageTopPadding = kDetailCellTopPadding + kAnswerImageTopPadding;
    row.answerImageView.frame =
        CGRectMake(imageLeftPadding, imageTopPadding, kAnswerImageWidth,
                   kAnswerImageWidth);
    row.answerImageView.hidden = NO;
  } else {
    row.answerImageView.hidden = YES;
  }

  // DetailTextLabel and textLabel are fading labels placed in each row. The
  // textLabel is laid out above the detailTextLabel, and vertically centered
  // if the detailTextLabel is empty.
  // For the detail text label, we use either the regular detail label, which
  // truncates by fading, or the answer label, which uses UILabel's standard
  // truncation by ellipse for the multi-line text sometimes shown in answers.
  row.detailTruncatingLabel.hidden = match.hasAnswer;
  row.detailAnswerLabel.hidden = !match.hasAnswer;
  // URLs have have special layout requirements that need to be invoked here.
  row.detailTruncatingLabel.displayAsURL = match.isURL;

  // TODO(crbug.com/697647): The complexity of managing these two separate
  // labels could probably be encapusulated in the row class if we moved the
  // layout logic there.
  UILabel* detailTextLabel =
      match.hasAnswer ? row.detailAnswerLabel : row.detailTruncatingLabel;
  [detailTextLabel setTextAlignment:self.alignment];

  // The width must be positive for CGContextRef to be valid.
  UIEdgeInsets safeAreaInsets = self.view.safeAreaInsets;
  CGRect rowBounds = UIEdgeInsetsInsetRect(self.view.bounds, safeAreaInsets);
  CGFloat labelWidth =
      MAX(40, floorf(rowBounds.size.width) - kTextCellLeadingPadding);
  CGFloat labelHeight =
      match.hasAnswer ? kAnswerLabelHeight : kTextDetailLabelHeight;
  CGFloat answerImagePadding = kAnswerImageWidth + kAnswerImageRightPadding;
  CGFloat leadingPadding =
      (match.hasImage && !alignmentRight ? answerImagePadding : 0) +
      kTextCellLeadingPadding;

  LayoutRect detailTextLabelLayout =
      LayoutRectMake(leadingPadding, CGRectGetWidth(rowBounds),
                     kDetailCellTopPadding, labelWidth, labelHeight);
  detailTextLabel.frame = LayoutRectGetRect(detailTextLabelLayout);

  detailTextLabel.attributedText = match.detailText;

  // Set detail text label number of lines
  if (match.hasAnswer) {
    detailTextLabel.numberOfLines = match.numberOfLines;
  }

  [detailTextLabel setNeedsDisplay];

  FadeTruncatingLabel* textLabel = row.textTruncatingLabel;
  [textLabel setTextAlignment:self.alignment];
  LayoutRect textLabelLayout =
      LayoutRectMake(kTextCellLeadingPadding, CGRectGetWidth(rowBounds), 0,
                     labelWidth, kTextLabelHeight);
  textLabel.frame = LayoutRectGetRect(textLabelLayout);

  // Set the text.
  textLabel.attributedText = match.text;

  // Center the textLabel if detailLabel is empty.
  if (!match.hasAnswer && [match.detailText length] == 0) {
    textLabel.center = CGPointMake(textLabel.center.x, floor(kRowHeight / 2));
    textLabel.frame = AlignRectToPixel(textLabel.frame);
  } else {
    CGRect frame = textLabel.frame;
    frame.origin.y = kTextCellTopPadding;
    textLabel.frame = frame;
  }

  // The leading image (e.g. magnifying glass, star, clock) is only shown on
  // iPad.
  if ([self showsLeadingIcons]) {
    UIImage* image = nil;
    image = match.suggestionTypeIcon;
    DCHECK(image);
    [row updateLeadingImage:image];
  }

  row.tabMatch = match.isTabMatch;

  // Show append button for search history/search suggestions as the right
  // control element (aka an accessory element of a table view cell).
  BOOL hasVisibleTrailingButton = match.isAppendable || match.isTabMatch;
  row.trailingButton.hidden = !hasVisibleTrailingButton;
  [row.trailingButton cancelTrackingWithEvent:nil];

  // If a right accessory element is present or the text alignment is right
  // aligned, adjust the width to align with the accessory element.
  if (hasVisibleTrailingButton || alignmentRight) {
    LayoutRect layout =
        LayoutRectForRectInBoundingRect(textLabel.frame, self.view.frame);
    layout.size.width -= kTrailingButtonWidth;
    textLabel.frame = LayoutRectGetRect(layout);
    layout =
        LayoutRectForRectInBoundingRect(detailTextLabel.frame, self.view.frame);
    layout.size.width -=
        kTrailingButtonWidth + (match.hasImage ? answerImagePadding : 0);
    detailTextLabel.frame = LayoutRectGetRect(layout);
  }

  // Since it's a common use case to type in a left-to-right URL while the
  // device is set to a native RTL language, make sure the left alignment looks
  // good by anchoring the leading edge to the left.
  if (LTRTextInRTLLayout) {
    // This is really a left padding, not a leading padding.
    const CGFloat kLTRTextInRTLLayoutLeftPadding =
        [self showsLeadingIcons] ? ([self useRegularWidthOffset] ? 176 : 94)
                                 : 94;
    CGRect frame = textLabel.frame;
    frame.size.width -= kLTRTextInRTLLayoutLeftPadding - frame.origin.x;
    frame.origin.x = kLTRTextInRTLLayoutLeftPadding;
    textLabel.frame = frame;

    frame = detailTextLabel.frame;
    frame.size.width -= kLTRTextInRTLLayoutLeftPadding - frame.origin.x;
    frame.origin.x = kLTRTextInRTLLayoutLeftPadding;
    detailTextLabel.frame = frame;
  }

  NSString* trailingButtonActionName =
      row.tabMatch
          ? l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB)
          : l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_APPEND);
  UIAccessibilityCustomAction* trailingButtonAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:trailingButtonActionName
                target:row
              selector:@selector(accessibilityTrailingButtonTapped)];

  row.accessibilityCustomActions =
      hasVisibleTrailingButton ? @[ trailingButtonAction ] : nil;

  [textLabel setNeedsDisplay];
}

- (void)updateTableViewWithAnimation:(BOOL)animation {
  [self layoutRows];

  size_t size = self.currentResult.count;
  if (animation && size > 0) {
    [self fadeInRows];
  }
}

- (void)layoutRows {
  size_t size = self.currentResult.count;

  [self.tableView reloadData];
  for (size_t i = 0; i < kRowCount; i++) {
    OmniboxPopupRow* row = _rows[i];

    if (i < size) {
      [self updateRow:row withMatch:self.currentResult[i]];
      row.hidden = NO;
    } else {
      row.hidden = YES;
    }
  }

  if (IsIPadIdiom())
    [self updateContentInsetForKeyboard];
}

- (void)fadeInRows {
  [CATransaction begin];
  [CATransaction
      setAnimationTimingFunction:[CAMediaTimingFunction
                                     functionWithControlPoints:
                                                             0:0:0.2:1]];
  for (size_t i = 0; i < kRowCount; i++) {
    OmniboxPopupRow* row = _rows[i];
    CGFloat beginTime = (i + 1) * .05;
    CABasicAnimation* transformAnimation =
        [CABasicAnimation animationWithKeyPath:@"transform"];
    [transformAnimation
        setFromValue:[NSValue
                         valueWithCATransform3D:CATransform3DMakeTranslation(
                                                    0, -20, 0)]];
    [transformAnimation
        setToValue:[NSValue valueWithCATransform3D:CATransform3DIdentity]];
    [transformAnimation setDuration:0.5];
    [transformAnimation setBeginTime:beginTime];

    CAAnimation* fadeAnimation = OpacityAnimationMake(0, 1);
    [fadeAnimation setDuration:0.5];
    [fadeAnimation setBeginTime:beginTime];

    [[row layer]
        addAnimation:AnimationGroupMake(@[ transformAnimation, fadeAnimation ])
              forKey:@"animateIn"];
  }
  [CATransaction commit];
}

#pragma mark -
#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];

  // TODO(crbug.com/733650): Default to the dragging check once it's been tested
  // on trunk.
  if (!scrollView.dragging)
    return;

  for (OmniboxPopupRow* row in _rows) {
    row.highlighted = NO;
  }
}

#pragma mark -
#pragma mark Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return self.shortcutsViewController.collectionView.collectionViewLayout
        .collectionViewContentSize.height;
  }

  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  return ((OmniboxPopupRow*)(_rows[indexPath.row])).rowHeight;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return self.shortcutsCell;
  }

  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  return _rows[indexPath.row];
}

#pragma mark - private

- (BOOL)showsLeadingIcons {
  return IsRegularXRegularSizeClass();
}

- (BOOL)useRegularWidthOffset {
  return [self showsLeadingIcons] && !IsCompactWidth();
}

#pragma mark - OmniboxPopupRowAccessibilityDelegate

- (void)accessibilityTrailingButtonTappedOmniboxPopupRow:(OmniboxPopupRow*)row {
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:row.rowNumber];
}

@end
