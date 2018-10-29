// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Spacing between tiles.
const CGFloat kHorizontalSpacingRegularXRegular = 19;
const CGFloat kHorizontalSpacingOther = 9;
const CGFloat kVerticalSpacing = 16;

// Width of search field.
const CGFloat kSearchFieldLarge = 432;
const CGFloat kSearchFieldSmall = 343;
const CGFloat kSearchFieldMinMargin = 8;

// Veritcla margin of search hint text.
const CGFloat kSearchHintMargin = 3;

// Top margin for the doodle.
const CGFloat kDoodleTopMarginRegularXRegular = 162;
const CGFloat kDoodleTopMarginOther = 58;

// Top margin for the search field
const CGFloat kSearchFieldTopMargin = 32;

// Bottom margin for the search field.
const CGFloat kNTPSearchFieldBottomPadding = 16;

// Alpha for search hint text.
const CGFloat kHintAlpha = 0.3;

const CGFloat kTopSpacingMaterial = 24;

const CGFloat kVoiceSearchButtonWidth = 48;

// Height for the doodle frame.
const CGFloat kGoogleSearchDoodleHeight = 120;

// Height for the doodle frame when Google is not the default search engine.
const CGFloat kNonGoogleSearchDoodleHeight = 60;

// Height for the header view on tablet when Google is not the default search
// engine.
const CGFloat kNonGoogleSearchHeaderHeightIPad = 10;

}

namespace content_suggestions {

const CGFloat kSearchFieldHeight = 50;
const int kSearchFieldBackgroundColor = 0xF1F3F4;
const CGFloat kHintTextScale = 0.15;

const NSUInteger kMostVisitedItemsPerLine = 4;

NSUInteger numberOfTilesForWidth(CGFloat availableWidth) {
  return kMostVisitedItemsPerLine;
}

CGFloat horizontalSpacingBetweenTiles() {
  return (!IsCompactWidth() && !IsCompactHeight())
             ? kHorizontalSpacingRegularXRegular
             : kHorizontalSpacingOther;
}

CGFloat verticalSpacingBetweenTiles() {
  return kVerticalSpacing;
}

CGFloat centeredTilesMarginForWidth(CGFloat width) {
  CGFloat horizontalSpace = horizontalSpacingBetweenTiles();
  NSUInteger columns = numberOfTilesForWidth(width - 2 * horizontalSpace);
  CGFloat whitespace =
      width -
      (columns * [ContentSuggestionsMostVisitedCell defaultSize].width) -
      ((columns - 1) * horizontalSpace);
  CGFloat margin = AlignValueToPixel(whitespace / 2);
  // Allow for less spacing as an edge case on smaller devices.
  if (margin < horizontalSpace) {
    DCHECK(width < 400);  // For now this is only expected on small widths.
    return fmaxf(margin, 0);
  }
  return margin;
}

CGFloat doodleHeight(BOOL logoIsShowing) {
  if (!IsRegularXRegularSizeClass() && !logoIsShowing)
    return kNonGoogleSearchDoodleHeight;

  return kGoogleSearchDoodleHeight;
}

CGFloat doodleTopMargin(BOOL toolbarPresent, CGFloat topInset) {
  if (!IsCompactWidth() && !IsCompactHeight())
    return kDoodleTopMarginRegularXRegular;
  return topInset + kDoodleTopMarginOther;
}

CGFloat searchFieldTopMargin() {
  return kSearchFieldTopMargin;
}

CGFloat searchFieldWidth(CGFloat superviewWidth) {
  if (!IsCompactWidth() && !IsCompactHeight())
    return kSearchFieldLarge;

  // Special case for narrow sizes.
  return MIN(kSearchFieldSmall, superviewWidth - kSearchFieldMinMargin * 2);
}

CGFloat heightForLogoHeader(BOOL logoIsShowing,
                            BOOL promoCanShow,
                            BOOL toolbarPresent,
                            CGFloat topInset) {
  CGFloat headerHeight = doodleTopMargin(toolbarPresent, topInset) +
                         doodleHeight(logoIsShowing) + searchFieldTopMargin() +
                         kSearchFieldHeight + kNTPSearchFieldBottomPadding;
  if (!IsRegularXRegularSizeClass()) {
    return headerHeight;
  }
  if (!logoIsShowing) {
    return kNonGoogleSearchHeaderHeightIPad;
  }
  if (!promoCanShow) {
    headerHeight += kTopSpacingMaterial;
  }

  return headerHeight;
}

void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIButton* searchTapTarget) {
  [searchHintLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:searchHintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [searchHintLabel.centerYAnchor
        constraintEqualToAnchor:searchTapTarget.centerYAnchor],
    [searchHintLabel.heightAnchor
        constraintEqualToConstant:kSearchFieldHeight - 2 * kSearchHintMargin],
  ]];

  [searchHintLabel.centerXAnchor
      constraintEqualToAnchor:searchTapTarget.centerXAnchor]
      .active = YES;

  [searchHintLabel setText:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  if (base::i18n::IsRTL()) {
    [searchHintLabel setTextAlignment:NSTextAlignmentRight];
  }
  [searchHintLabel setTextColor:[UIColor colorWithWhite:0 alpha:kHintAlpha]];
  searchHintLabel.font = [UIFont systemFontOfSize:17];
  searchHintLabel.textAlignment = NSTextAlignmentCenter;
}

void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIButton* searchTapTarget) {
  [voiceSearchButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:voiceSearchButton];

  [NSLayoutConstraint activateConstraints:@[
    [voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:searchTapTarget.centerYAnchor],
    [voiceSearchButton.widthAnchor
        constraintEqualToConstant:kVoiceSearchButtonWidth],
    [voiceSearchButton.heightAnchor
        constraintEqualToAnchor:voiceSearchButton.widthAnchor],
  ]];

  [voiceSearchButton setAdjustsImageWhenHighlighted:NO];

  UIImage* micImage = [[UIImage imageNamed:@"location_bar_voice"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  [voiceSearchButton setImage:micImage forState:UIControlStateNormal];
  voiceSearchButton.tintColor = [UIColor colorWithWhite:0 alpha:0.7];
  [voiceSearchButton setAccessibilityLabel:l10n_util::GetNSString(
                                               IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voiceSearchButton setAccessibilityIdentifier:@"Voice Search"];
}

UIView* nearestAncestor(UIView* view, Class aClass) {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return nearestAncestor([view superview], aClass);
}

}  // namespace content_suggestions
