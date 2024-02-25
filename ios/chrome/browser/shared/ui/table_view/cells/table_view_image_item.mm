// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation TableViewImageItem

@synthesize image = _image;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewImageCell class];
    _enabled = YES;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TableViewImageCell* cell =
      base::apple::ObjCCastStrict<TableViewImageCell>(tableCell);
  if (self.image) {
    cell.imageView.hidden = NO;
    cell.imageView.image = self.image;
  } else {
    // No image. Hide imageView.
    cell.imageView.hidden = YES;
  }

  cell.textLabel.text = self.title;
  cell.detailTextLabel.text = self.detailText;
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  } else if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  } else {
    cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  if (self.detailTextColor) {
    cell.detailTextLabel.textColor = self.detailTextColor;
  } else {
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }

  cell.userInteractionEnabled = self.enabled;
}

@end

@implementation TableViewImageCell

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

    // Set font size using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.numberOfLines = 2;
    [_textLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.numberOfLines = 0;

    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = 0;
    verticalStack.distribution = UIStackViewDistributionFill;
    verticalStack.alignment = UIStackViewAlignmentLeading;
    [self.contentView addSubview:verticalStack];

    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _imageView, verticalStack ]];
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

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.userInteractionEnabled = YES;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }
  return self.textLabel.text;
}

- (UIAccessibilityTraits)accessibilityTraits {
  UIAccessibilityTraits accessibilityTraits = super.accessibilityTraits;
  if (!self.isUserInteractionEnabled) {
    accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  return accessibilityTraits;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSMutableArray<NSString*>* userInputLabels = [[NSMutableArray alloc] init];
  if (self.textLabel.text) {
    [userInputLabels addObject:self.textLabel.text];
  }

  return userInputLabels;
}

@end
