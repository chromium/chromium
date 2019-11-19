// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The width and height of the favicon ImageView.
const CGFloat kFaviconWidth = 16;
// The width and height of the favicon container view.
const CGFloat kFaviconContainerWidth = 28;
// Default delimiter to use between the hostname and the supplemental URL text
// if text is specified but not the delimiter.
const char kDefaultSupplementalURLTextDelimiter[] = "•";
}

#pragma mark - TableViewURLCellFaviconBadgeView


#pragma mark - TableViewURLItem

@implementation TableViewURLItem

@synthesize title = _title;
@synthesize URL = _URL;
@synthesize supplementalURLText = _supplementalURLText;
@synthesize supplementalURLTextDelimiter = _supplementalURLTextDelimiter;
@synthesize badgeImage = _badgeImage;
@synthesize metadata = _metadata;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewURLCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TableViewURLCell* cell =
      base::mac::ObjCCastStrict<TableViewURLCell>(tableCell);
  cell.titleLabel.text = [self titleLabelText];
  cell.URLLabel.text = [self URLLabelText];
  cell.faviconBadgeView.image = self.badgeImage;
  cell.metadataLabel.text = self.metadata;
  cell.cellUniqueIdentifier = self.uniqueIdentifier;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  if (styler.cellTitleColor)
    cell.titleLabel.textColor = styler.cellTitleColor;
  if (styler.cellDetailColor) {
    cell.URLLabel.textColor = styler.cellDetailColor;
    cell.metadataLabel.textColor = styler.cellDetailColor;
  }

  [cell configureUILayout];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.URL.host());
}

#pragma mark Private

// Returns the text to use when configuring a TableViewURLCell's title label.
- (NSString*)titleLabelText {
  if (self.title.length) {
    return self.title;
  } else if (base::SysUTF8ToNSString(self.URL.host()).length) {
    return base::SysUTF8ToNSString(self.URL.host());
  } else {
    // Backup in case host returns nothing (e.g. about:blank).
    return base::SysUTF8ToNSString(self.URL.spec());
  }
}

// Returns the text to use when configuring a TableViewURLCell's URL label.
- (NSString*)URLLabelText {
  // If there's no title text, the URL is used as the cell title.  Add the
  // supplemental text to the URL label below if it exists.
  if (!self.title.length)
    return self.supplementalURLText;

  // Append the hostname with the supplemental text.
  NSString* hostname = base::SysUTF8ToNSString(self.URL.host());
  if (self.supplementalURLText.length) {
    NSString* delimeter =
        self.supplementalURLTextDelimiter.length
            ? self.supplementalURLTextDelimiter
            : base::SysUTF8ToNSString(kDefaultSupplementalURLTextDelimiter);
    return [NSString stringWithFormat:@"%@ %@ %@", hostname, delimeter,
                                      self.supplementalURLText];
  } else {
    return hostname;
  }
}

@end

#pragma mark - TableViewURLCell

@interface TableViewURLCell ()
// If the cell's accessibility label has not been manually set via
// |-setAccessibilityLabel:|, this property will be YES, and
// |-accessibilityLabel| will return a lazily created label based on the
// text values of the UILabel subviews.
@property(nonatomic, assign) BOOL shouldGenerateAccessibilityLabel;
// Horizontal StackView that holds url, title, and metadata labels.
@property(nonatomic, strong) UIStackView* horizontalStack;
@end

@implementation TableViewURLCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIImage* containerBackground =
        [[UIImage imageNamed:@"table_view_cell_favicon_background"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _faviconContainerView =
        [[UIImageView alloc] initWithImage:containerBackground];
    _faviconContainerView.tintColor =
        [UIColor colorNamed:kFaviconBackgroundColor];

    _faviconView = [[FaviconView alloc] init];
    _faviconView.contentMode = UIViewContentModeScaleAspectFit;
    _faviconView.clipsToBounds = YES;
    [_faviconContainerView addSubview:_faviconView];
    _faviconBadgeView = [[TableViewURLCellFaviconBadgeView alloc] init];
    _titleLabel = [[UILabel alloc] init];
    _URLLabel = [[UILabel alloc] init];
    _metadataLabel = [[UILabel alloc] init];

    // Set font sizes using dynamic type.
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.textColor = UIColor.cr_secondaryLabelColor;
    _URLLabel.hidden = YES;
    _metadataLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _metadataLabel.textColor = UIColor.cr_secondaryLabelColor;
    _metadataLabel.adjustsFontForContentSizeCategory = YES;
    _metadataLabel.hidden = YES;

    // Use stack views to layout the subviews except for the favicon.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _URLLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    [_metadataLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_metadataLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    // Horizontal stack view holds vertical stack view and metadata label.
    self.horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, _metadataLabel ]];
    self.horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    self.horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    self.horizontalStack.distribution = UIStackViewDistributionFill;
    self.horizontalStack.alignment = UIStackViewAlignmentFill;

    UIView* contentView = self.contentView;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
    self.horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_faviconContainerView];
    [contentView addSubview:_faviconBadgeView];
    [contentView addSubview:self.horizontalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    NSLayoutConstraint* topConstraint = [self.horizontalStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing];
    NSLayoutConstraint* bottomConstraint = [self.horizontalStack.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.bottomAnchor
                                    constant:
                                        -
                                        kTableViewTwoLabelsCellVerticalSpacing];

    [NSLayoutConstraint activateConstraints:@[
      // The favicon view is a fixed size, is pinned to the leading edge of the
      // content view, and is centered vertically.
      [_faviconView.heightAnchor constraintEqualToConstant:kFaviconWidth],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconWidth],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
      [_faviconView.centerXAnchor
          constraintEqualToAnchor:_faviconContainerView.centerXAnchor],
      [_faviconContainerView.heightAnchor
          constraintEqualToConstant:kFaviconContainerWidth],
      [_faviconContainerView.widthAnchor
          constraintEqualToConstant:kFaviconContainerWidth],
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The favicon badge view is aligned with the top-trailing corner of the
      // favicon container.
      [_faviconBadgeView.centerXAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor],
      [_faviconBadgeView.centerYAnchor
          constraintEqualToAnchor:_faviconContainerView.topAnchor],

      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [self.horizontalStack.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [self.horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [self.horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      topConstraint, bottomConstraint, heightConstraint
    ]];
  }
  return self;
}

// Hide or show the metadata and URL labels depending on the presence of text.
// Align the horizontal stack properly depending on if the metadata label will
// be present or not.
- (void)configureUILayout {
  if ([self.metadataLabel.text length]) {
    self.metadataLabel.hidden = NO;
    // Align metadataLabel to first line of content in vertical stack.
    self.horizontalStack.alignment = UIStackViewAlignmentFirstBaseline;
  } else {
    self.metadataLabel.hidden = YES;
    self.horizontalStack.alignment = UIStackViewAlignmentFill;
  }
  if ([self.URLLabel.text length]) {
    self.URLLabel.hidden = NO;
  } else {
    self.URLLabel.hidden = YES;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.faviconView configureWithAttributes:nil];
  self.faviconBadgeView.image = nil;
  self.horizontalStack.alignment = UIStackViewAlignmentFill;
  self.metadataLabel.hidden = YES;
  self.URLLabel.hidden = YES;
}

- (void)setAccessibilityLabel:(NSString*)accessibilityLabel {
  self.shouldGenerateAccessibilityLabel = !accessibilityLabel.length;
  [super setAccessibilityLabel:accessibilityLabel];
}

- (NSString*)accessibilityLabel {
  if (self.shouldGenerateAccessibilityLabel) {
    NSString* accessibilityLabel = self.titleLabel.text;
    if (self.URLLabel.text.length > 0) {
      accessibilityLabel = [NSString
          stringWithFormat:@"%@, %@", accessibilityLabel, self.URLLabel.text];
    }
    if (self.metadataLabel.text.length > 0) {
      accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                     self.metadataLabel.text];
    }
    return accessibilityLabel;
  } else {
    return [super accessibilityLabel];
  }
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSMutableArray<NSString*>* userInputLabels = [[NSMutableArray alloc] init];
  if (self.titleLabel.text) {
    [userInputLabels addObject:self.titleLabel.text];
  }

  return userInputLabels;
}

- (NSString*)accessibilityIdentifier {
  return self.titleLabel.text;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

@end
