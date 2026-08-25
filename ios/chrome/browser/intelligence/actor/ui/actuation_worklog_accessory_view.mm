// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_accessory_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;

const CGFloat kCaretSize = 16.0;
const CGFloat kIconSize = 32.0;
const CGFloat kTextStackSpacing = 2.0;
const CGFloat kHighlightedAlpha = 0.6;
const CGFloat kDisabledAlpha = 0.5;
const NSTimeInterval kHighlightAnimationDuration = 0.15;

}  // namespace

@implementation ActuationWorklogAccessoryView {
  UIImageView* _iconView;
  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UILabel* _detailLabel;
  UIImageView* _chevronView;
  UIStackView* _rowStack;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
    self.layer.cornerRadius = kSpacingMedium;
    self.backgroundColor = [UIColor colorNamed:kTertiaryBackgroundColor];

    [self setupSubviews];
    [self setupConstraints];

    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - Public

- (void)configureWithAccessoryItem:
    (ActuationWorklogAccessoryItem*)accessoryItem {
  CHECK(accessoryItem);
  _accessoryItem = accessoryItem;

  _iconView.image = accessoryItem.icon;
  _iconView.hidden = (accessoryItem.icon == nil);

  _titleLabel.text = accessoryItem.title;

  BOOL showSubtitle = (accessoryItem.subtitle.length > 0);
  _subtitleLabel.hidden = !showSubtitle;
  _subtitleLabel.text = showSubtitle ? accessoryItem.subtitle : nil;

  BOOL showDetail = (accessoryItem.detailText.length > 0);
  _detailLabel.hidden = !showDetail;
  _detailLabel.text = showDetail ? accessoryItem.detailText : nil;

  _chevronView.hidden = !accessoryItem.hasChevron;

  self.accessibilityLabel = [self accessibilityLabelForItem:accessoryItem];
}

#pragma mark - UIControl

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [UIView animateWithDuration:kHighlightAnimationDuration
                        delay:0.0
                      options:UIViewAnimationOptionBeginFromCurrentState |
                              UIViewAnimationOptionAllowUserInteraction
                   animations:^{
                     [self updateAppearance];
                   }
                   completion:nil];
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  [self updateAppearance];
}

#pragma mark - Private

- (void)updateAppearance {
  if (self.enabled) {
    self.alpha = self.highlighted ? kHighlightedAlpha : 1.0;
  } else {
    self.alpha = kDisabledAlpha;
  }
}

// Creates and initializes internal subviews and view hierarchy.
- (void)setupSubviews {
  _iconView = [[UIImageView alloc] init];
  _iconView.contentMode = UIViewContentModeScaleAspectFit;
  _iconView.translatesAutoresizingMaskIntoConstraints = NO;

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightBold);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.adjustsFontForContentSizeCategory = YES;
  _subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  _detailLabel = [[UILabel alloc] init];
  _detailLabel.adjustsFontForContentSizeCategory = YES;
  _detailLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _detailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _detailLabel.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageConfiguration* chevronConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kCaretSize
                          weight:UIImageSymbolWeightSemibold];
  UIImage* chevronImage =
      SymbolWithConfiguration(SymbolChevronForward, chevronConfig);

  _chevronView = [[UIImageView alloc] init];
  _chevronView.contentMode = UIViewContentModeScaleAspectFit;
  _chevronView.image =
      [chevronImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  _chevronView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  _chevronView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel, _detailLabel ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.spacing = kTextStackSpacing;
  textStack.alignment = UIStackViewAlignmentLeading;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;

  _rowStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _iconView, textStack, _chevronView ]];
  _rowStack.alignment = UIStackViewAlignmentCenter;
  _rowStack.spacing = kSpacingMedium;
  _rowStack.translatesAutoresizingMaskIntoConstraints = NO;
  _rowStack.userInteractionEnabled = NO;
  _rowStack.accessibilityElementsHidden = YES;
  _rowStack.layoutMarginsRelativeArrangement = YES;
  _rowStack.layoutMargins = UIEdgeInsetsMake(kSpacingSmall, kSpacingMedium,
                                             kSpacingSmall, kSpacingMedium);
  [self addSubview:_rowStack];
}

// Configures Auto Layout constraints.
- (void)setupConstraints {
  AddSquareConstraints(_iconView, kIconSize);
  AddSquareConstraints(_chevronView, kCaretSize);
  AddSameConstraints(self, _rowStack);
}

// Returns a joined accessibility label synthesized from item text fields.
- (NSString*)accessibilityLabelForItem:(ActuationWorklogAccessoryItem*)item {
  NSMutableArray<NSString*>* parts = [[NSMutableArray alloc] init];
  if (item.title.length > 0) {
    [parts addObject:item.title];
  }
  if (item.subtitle.length > 0) {
    [parts addObject:item.subtitle];
  }
  if (item.detailText.length > 0) {
    [parts addObject:item.detailText];
  }
  return [parts componentsJoinedByString:@", "];
}

@end
