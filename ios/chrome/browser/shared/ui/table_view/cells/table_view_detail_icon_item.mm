// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_cells_constants.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The size of the "new" IPH badge.
constexpr CGFloat kNewIPHBadgeSize = 20.0;
// The font size of the "N" in the "new" IPH badge.
constexpr CGFloat kNewIPHBadgeFontSize = 10.0;

// kDotSize represents the size of the dot (i.e. its height and width).
constexpr CGFloat kDotSize = 10.f;

// By default, the maximum number of lines to be displayed for the text and
// detail text should be 1.
const NSInteger kDefaultNumberOfLines = 1;

}  // namespace

@implementation TableViewDetailIconItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    self.badgeType = BadgeType::kNone;
    _textNumberOfLines = kDefaultNumberOfLines;
    _detailTextNumberOfLines = kDefaultNumberOfLines;
    _textLineBreakMode = NSLineBreakByTruncatingTail;
    _detailTextLineBreakMode = NSLineBreakByTruncatingTail;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* contentConfiguration =
      [[TableViewCellContentConfiguration alloc] init];
  switch (self.badgeType) {
    case BadgeType::kNone:
      contentConfiguration.title = self.text;
      break;
    case BadgeType::kNew: {
      NewFeatureBadgeView* newIPHBadgeView =
          [[NewFeatureBadgeView alloc] initWithBadgeSize:kNewIPHBadgeSize
                                                fontSize:kNewIPHBadgeFontSize];
      newIPHBadgeView.frame =
          CGRectMake(0, 0, kNewIPHBadgeSize, kNewIPHBadgeSize);
      [newIPHBadgeView setNeedsLayout];
      [newIPHBadgeView layoutIfNeeded];
      UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc]
          initWithBounds:newIPHBadgeView.bounds];
      UIImage* newBadgeImage = [renderer
          imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
            [newIPHBadgeView.layer renderInContext:rendererContext.CGContext];
          }];

      contentConfiguration.attributedTitle =
          [self createAttributedTitleWithImage:newBadgeImage];
      break;
    }
    case BadgeType::kNotificationDot: {
      UIImage* dot = [DefaultSymbolWithPointSize(kCircleFillSymbol, kDotSize)
          imageWithTintColor:[UIColor colorNamed:kBlueColor]];

      contentConfiguration.attributedTitle =
          [self createAttributedTitleWithImage:dot];

      break;
    }
  }
  contentConfiguration.titleNumberOfLines = self.textNumberOfLines;
  if (self.textLayoutConstraintAxis == UILayoutConstraintAxisVertical) {
    contentConfiguration.subtitle = self.detailText;
    contentConfiguration.subtitleNumberOfLines = self.detailTextNumberOfLines;
  } else {
    contentConfiguration.trailingText = self.detailText;
  }

  contentConfiguration.titleLineBreakMode = self.textLineBreakMode;
  contentConfiguration.subtitleLineBreakMode = self.detailTextLineBreakMode;

  if (self.iconImage) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage = self.iconImage;
    symbolConfiguration.symbolBackgroundColor = self.iconBackgroundColor;
    symbolConfiguration.symbolTintColor = self.iconTintColor;

    contentConfiguration.leadingConfiguration = symbolConfiguration;
  }

  NSString* accessibilityLabel = contentConfiguration.accessibilityLabel;
  switch (_badgeType) {
    case BadgeType::kNotificationDot:
      accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                     l10n_util::GetNSString(
                                         IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT)];
      break;
    case BadgeType::kNew:
      accessibilityLabel = [NSString
          stringWithFormat:@"%@, %@", accessibilityLabel,
                           l10n_util::GetNSString(
                               IDS_IOS_NEW_FEATURE_ACCESSIBILITY_HINT)];
      break;
    case BadgeType::kNone:
      break;
  }

  cell.contentConfiguration = contentConfiguration;
  cell.accessibilityLabel = accessibilityLabel;
  cell.accessibilityValue = contentConfiguration.accessibilityValue;
  cell.accessibilityHint = contentConfiguration.accessibilityHint;
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

#pragma mark - Private

// Returns an attributed string with `image` at the end.
- (NSAttributedString*)createAttributedTitleWithImage:(UIImage*)image {
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:self.text];

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  CGFloat yOffset = (font.capHeight - image.size.height) / 2.0;

  NSTextAttachment* imageAttachment = [[NSTextAttachment alloc] init];
  imageAttachment.image = image;

  imageAttachment.bounds =
      CGRectMake(0, yOffset, image.size.width, image.size.height);

  [attributedString
      appendAttributedString:[[NSAttributedString alloc] initWithString:@" "]];
  [attributedString
      appendAttributedString:
          [NSAttributedString attributedStringWithAttachment:imageAttachment]];

  return attributedString;
}

@end
