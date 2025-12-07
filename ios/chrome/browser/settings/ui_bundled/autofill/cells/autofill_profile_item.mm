// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_profile_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AutofillProfileItem

@synthesize image = _image;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillProfileCell class];
  }
  return self;
}

- (void)configureCell:(AutofillProfileCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if (self.image) {
    cell.imageView.hidden = NO;
    cell.imageView.image = self.image;
  } else {
    // No image. Hide imageView.
    cell.imageView.hidden = YES;
  }

  [cell setText:self.title];
  [cell setDetailText:self.detailText];
  [cell setTrailingDetailText:_trailingDetailText];
  cell.localProfileIconShown = self.localProfileIconShown;
}

@end

@implementation AutofillProfileCell {
  // The cell text.
  UILabel* _textLabel;
  // The cell detail text.
  UILabel* _detailTextLabel;
  // The cell trailing detail text.
  UILabel* _trailingDetailTextLabel;
}

// These properties overrides the ones from UITableViewCell, so this @synthesize
// cannot be removed.
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    _imageView = [[UIImageView alloc] init];
    // The favicon image is smaller than its UIImageView's bounds, so center it.
    _imageView.contentMode = UIViewContentModeCenter;
    [_imageView setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisHorizontal];
    [_imageView setTintColor:[UIColor colorNamed:kTextTertiaryColor]];

    _textLabel = [[UILabel alloc] init];
    _textLabel.numberOfLines = 0;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    _trailingDetailTextLabel = [[UILabel alloc] init];
    _trailingDetailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _trailingDetailTextLabel.textColor =
        [UIColor colorNamed:kTextSecondaryColor];
    _trailingDetailTextLabel.hidden = YES;

    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = 0;
    verticalStack.distribution = UIStackViewDistributionFill;
    verticalStack.alignment = UIStackViewAlignmentLeading;
    [self.contentView addSubview:verticalStack];

    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          verticalStack, _trailingDetailTextLabel, _imageView
        ]];
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    [self.contentView addSubview:horizontalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;
    [NSLayoutConstraint activateConstraints:@[
      // Horizontal Stack constraints.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kTableViewVerticalSpacing],
      [horizontalStack.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kTableViewVerticalSpacing],
      heightConstraint,
    ]];
  }
  return self;
}

- (void)setText:(NSString*)text {
  _textLabel.text = text;

  if (_textLabel.text.length == 0) {
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _detailTextLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  } else {
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
}

- (void)setDetailText:(NSString*)detailText {
  _detailTextLabel.text = detailText;
}

- (void)setTrailingDetailText:(NSString*)trailingText {
  _trailingDetailTextLabel.text = trailingText;
  _trailingDetailTextLabel.hidden = (trailingText.length == 0);
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self setText:nil];
  [self setDetailText:nil];
  [self setTrailingDetailText:nil];
  self.localProfileIconShown = NO;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  NSString* label = _textLabel.text;
  if (_detailTextLabel.text.length > 0) {
    label = [NSString stringWithFormat:@"%@, %@", label, _detailTextLabel.text];
  }

  if (_trailingDetailTextLabel.text.length > 0) {
    label = [NSString
        stringWithFormat:@"%@, %@", label, _trailingDetailTextLabel.text];
  }
  if (self.localProfileIconShown) {
    label = [NSString
        stringWithFormat:@"%@, %@", label,
                         l10n_util::GetNSString(
                             IDS_IOS_LOCAL_ADDRESS_ACCESSIBILITY_LABEL)];
  }
  return label;
}

@end
