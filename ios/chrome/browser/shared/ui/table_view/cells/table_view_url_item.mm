// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Default delimiter to use between the hostname and the supplemental URL text
// if text is specified but not the delimiter.
const char kDefaultSupplementalURLTextDelimiter[] = "•";
// The max number of lines for the cell title label.
const int kMaxNumberOfLinesForCellTitleLabel = 2;
}  // namespace

#pragma mark - TableViewURLItem

@implementation TableViewURLItem

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
      base::apple::ObjCCastStrict<TableViewURLCell>(tableCell);
  cell.titleLabel.text = [self titleLabelText];
  cell.URLLabel.text = [self URLLabelText];
  cell.thirdRowLabel.text = self.thirdRowText;
  cell.faviconBadgeView.image = self.badgeImage;
  cell.metadataLabel.text = self.metadata;
  cell.metadataImage.image = self.metadataImage;
  cell.metadataImage.tintColor = self.metadataImageColor;
  cell.cellUniqueIdentifier = self.uniqueIdentifier;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  if (styler.cellTitleColor) {
    cell.titleLabel.textColor = styler.cellTitleColor;
  }
  if (self.thirdRowTextColor) {
    cell.thirdRowLabel.textColor = self.thirdRowTextColor;
  }

  [cell configureUILayout];
}

- (NSString*)uniqueIdentifier {
  if (!self.URL) {
    return @"";
  }
  return base::SysUTF8ToNSString(self.URL.gurl.host());
}

#pragma mark Private

// Returns the text to use when configuring a TableViewURLCell's title label.
- (NSString*)titleLabelText {
  if (self.title.length) {
    return self.title;
  }
  if (!self.URL) {
    return @"";
  }
  NSString* hostname = [self displayedURL];
  if (hostname.length) {
    return hostname;
  }
  // Backup in case host returns nothing (e.g. about:blank).
  return base::SysUTF8ToNSString(self.URL.gurl.spec());
}

// Returns the text to use when configuring a TableViewURLCell's URL label.
- (NSString*)URLLabelText {
  // Use detail text instead of the URL if there is one set.
  if (self.detailText) {
    return self.detailText;
  }
  // If there's no title text, the URL is used as the cell title.  Add the
  // supplemental text to the URL label below if it exists.
  if (!self.title.length) {
    return self.supplementalURLText;
  }

  // Append the hostname with the supplemental text.
  if (!self.URL) {
    return @"";
  }

  NSString* hostname = [self displayedURL];
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

- (NSString*)displayedURL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              self.URL.gurl));
}

@end

#pragma mark - TableViewURLCell

@interface TableViewURLCell ()
// If the cell's accessibility label has not been manually set via
// `-setAccessibilityLabel:`, this property will be YES, and
// `-accessibilityLabel` will return a lazily created label based on the
// text values of the UILabel subviews.
@property(nonatomic, assign) BOOL shouldGenerateAccessibilityLabel;
// Container View for the faviconView.
@property(nonatomic, strong) FaviconContainerView* faviconContainerView;
// Activity indicator (spinner) used for indicating an in-flight request related
// to the item represented by this cell.
@property(nonatomic, strong) UIActivityIndicatorView* activityIndicatorView;
@end

@implementation TableViewURLCell {
  // Constraint defining the distance between the title vertical stack view and
  // the metadata image.
  NSLayoutConstraint* _titleMetadataImageSpacingConstraint;
  // Constraint defining the distance between the metadata label and the
  // metadata image views.
  NSLayoutConstraint* _metadataViewsSpacingConstraint;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    _faviconContainerView = [[FaviconContainerView alloc] init];

    _faviconBadgeView = [[TableViewURLCellFaviconBadgeView alloc] init];
    _titleLabel = [[UILabel alloc] init];
    _URLLabel = [[UILabel alloc] init];
    _thirdRowLabel = [[UILabel alloc] init];
    _metadataImage = [[UIImageView alloc] init];
    _metadataLabel = [[UILabel alloc] init];

    // Set font sizes using dynamic type.
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    // Sometimes a very long url is used as the cell title, so be sure it will
    // not stretch the cell to an unlimited number of lines.
    _titleLabel.numberOfLines = kMaxNumberOfLinesForCellTitleLabel;
    _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _URLLabel.hidden = YES;
    _URLLabel.numberOfLines = 0;
    _thirdRowLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _thirdRowLabel.adjustsFontForContentSizeCategory = YES;
    _thirdRowLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _thirdRowLabel.numberOfLines = 0;
    _thirdRowLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _thirdRowLabel.hidden = YES;
    _metadataLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _metadataLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _metadataLabel.adjustsFontForContentSizeCategory = YES;
    _metadataLabel.hidden = YES;
    _metadataImage.contentMode = UIViewContentModeCenter;
    _metadataImage.accessibilityIdentifier = kTableViewURLCellMetadataImageID;

    // Use stack views to layout the subviews except for the favicon.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _URLLabel, _thirdRowLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    [_metadataLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_metadataLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_metadataImage setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_metadataImage
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    // Horizontal view holds vertical stack view and metadata views.
    UIView* horizontalView = [[UIView alloc] init];
    [horizontalView addSubview:verticalStack];
    [horizontalView addSubview:_metadataImage];
    [horizontalView addSubview:_metadataLabel];

    UIView* contentView = self.contentView;
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalView.translatesAutoresizingMaskIntoConstraints = NO;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    _metadataImage.translatesAutoresizingMaskIntoConstraints = NO;
    _metadataLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_faviconContainerView];
    [contentView addSubview:_faviconBadgeView];
    [contentView addSubview:horizontalView];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    _titleMetadataImageSpacingConstraint = [_metadataImage.leadingAnchor
        constraintEqualToAnchor:verticalStack.trailingAnchor
                       constant:0];
    _metadataViewsSpacingConstraint = [_metadataLabel.leadingAnchor
        constraintEqualToAnchor:_metadataImage.trailingAnchor
                       constant:0];

    [NSLayoutConstraint activateConstraints:@[
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
      [horizontalView.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [horizontalView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [verticalStack.topAnchor
          constraintEqualToAnchor:horizontalView.topAnchor],
      [verticalStack.bottomAnchor
          constraintEqualToAnchor:horizontalView.bottomAnchor],
      [verticalStack.leadingAnchor
          constraintEqualToAnchor:horizontalView.leadingAnchor],
      [_metadataImage.centerYAnchor
          constraintEqualToAnchor:horizontalView.centerYAnchor],
      [_metadataImage.heightAnchor
          constraintLessThanOrEqualToAnchor:horizontalView.heightAnchor],
      [_metadataLabel.firstBaselineAnchor
          constraintEqualToAnchor:verticalStack.firstBaselineAnchor],
      [_metadataLabel.trailingAnchor
          constraintEqualToAnchor:horizontalView.trailingAnchor],
      [_metadataLabel.heightAnchor
          constraintLessThanOrEqualToAnchor:horizontalView.heightAnchor],
      [horizontalView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:
                                          kTableViewTwoLabelsCellVerticalSpacing],
      [horizontalView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:
                                       -kTableViewTwoLabelsCellVerticalSpacing],
      _titleMetadataImageSpacingConstraint, _metadataViewsSpacingConstraint,
      heightConstraint
    ]];
  }
  return self;
}

- (FaviconView*)faviconView {
  return self.faviconContainerView.faviconView;
}

- (void)setFaviconContainerBackgroundColor:(UIColor*)backgroundColor {
  [self.faviconContainerView setFaviconBackgroundColor:backgroundColor];
}

- (void)setFaviconContainerBorderColor:(UIColor*)borderColor {
  [self.faviconContainerView setFaviconBorderColor:borderColor];
}

// Hide or show the metadata and URL labels depending on the presence of text.
// Align the horizontal stack properly depending on if the metadata label will
// be present or not.
- (void)configureUILayout {
  if ([self.metadataLabel.text length]) {
    self.metadataLabel.hidden = NO;
    _metadataViewsSpacingConstraint.constant =
        kTableViewTwoLabelsCellVerticalSpacing;
  } else {
    self.metadataLabel.hidden = YES;
    _metadataViewsSpacingConstraint.constant = 0;
  }
  if ([self.metadataImage image]) {
    self.metadataImage.hidden = NO;
    _titleMetadataImageSpacingConstraint.constant =
        kTableViewTwoLabelsCellVerticalSpacing;
  } else {
    self.metadataImage.hidden = YES;
    _titleMetadataImageSpacingConstraint.constant = 0;
  }
  if ([self.URLLabel.text length]) {
    self.URLLabel.hidden = NO;
  } else {
    self.URLLabel.hidden = YES;
  }
  if ([self.thirdRowLabel.text length] && !self.URLLabel.hidden) {
    self.thirdRowLabel.hidden = NO;
  } else {
    // There shouldn't be a third row if the second row isn't even shown.
    self.thirdRowLabel.hidden = YES;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.faviconView configureWithAttributes:nil];
  [self setFaviconContainerBackgroundColor:nil];
  [self setFaviconContainerBorderColor:nil];
  self.faviconBadgeView.image = nil;
  self.metadataLabel.hidden = YES;
  self.metadataImage.image = nil;
  self.URLLabel.hidden = YES;
  self.thirdRowLabel.hidden = YES;
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
    if (self.thirdRowLabel.text.length > 0) {
      accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                     self.thirdRowLabel.text];
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

- (void)startAnimatingActivityIndicator {
  // It may be an edge case if the activity indicator is spinning when we don't
  // expect it. But it's okay to leave indicator spinning instead of crashing.
  if (self.activityIndicatorView != nil) {
    return;
  }

  self.activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  UIActivityIndicatorView* activityView = self.activityIndicatorView;
  activityView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.faviconContainerView addSubview:activityView];
  [NSLayoutConstraint activateConstraints:@[
    [activityView.centerXAnchor
        constraintEqualToAnchor:self.faviconContainerView.centerXAnchor],
    [activityView.centerYAnchor
        constraintEqualToAnchor:self.faviconContainerView.centerYAnchor],
  ]];
  [activityView startAnimating];
  activityView.backgroundColor = self.faviconContainerView.backgroundColor;
}

- (void)stopAnimatingActivityIndicator {
  [self.activityIndicatorView stopAnimating];
  [self.activityIndicatorView removeFromSuperview];
  self.activityIndicatorView = nil;
}

@end
