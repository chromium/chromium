// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_header.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GridHeader ()
// Visual components of the view.
@property(nonatomic, weak) UIStackView* containerView;
@property(nonatomic, weak) UILabel* titleLabel;
@property(nonatomic, weak) UILabel* valueLabel;
@end

@implementation GridHeader

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
    self.accessibilityIdentifier = kGridSectionHeaderIdentifier;
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.textColor = UIColorFromRGB(kGridHeaderTitleColor);
    [titleLabel setContentHuggingPriority:UILayoutPriorityDefaultLow
                                  forAxis:UILayoutConstraintAxisHorizontal];
    _titleLabel = titleLabel;

    UILabel* valueLabel = [[UILabel alloc] init];
    valueLabel.translatesAutoresizingMaskIntoConstraints = NO;
    valueLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    valueLabel.adjustsFontForContentSizeCategory = YES;
    valueLabel.textColor = UIColorFromRGB(kGridHeaderValueColor);
    valueLabel.alpha = 0.6;
    [valueLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
    _valueLabel = valueLabel;

    UIStackView* containerView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _valueLabel ]];
    containerView.axis = UILayoutConstraintAxisHorizontal;
    containerView.translatesAutoresizingMaskIntoConstraints = NO;
    containerView.spacing = kGridHeaderContentSpacing;
    containerView.layoutMarginsRelativeArrangement = YES;
    _containerView = containerView;
    [self updateContentInsets];
    [self addSubview:containerView];

    self.layer.masksToBounds = YES;
    NSMutableArray* constraints = [[NSMutableArray alloc] init];
    [constraints addObjectsFromArray:@[
      [containerView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [containerView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [containerView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [valueLabel.heightAnchor
          constraintEqualToAnchor:containerView.heightAnchor],
      [titleLabel.heightAnchor
          constraintEqualToAnchor:containerView.heightAnchor],
    ]];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self updateContentInsets];
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.title = nil;
  self.value = nil;
  self.titleLabel.text = nil;
  self.valueLabel.text = nil;
  self.valueLabel.hidden = YES;
  [self updateContentInsets];
}

#pragma mark - Public

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
  self.titleLabel.accessibilityLabel = title;
  _title = title;
}

- (void)setValue:(NSString*)value {
  self.valueLabel.text = value;
  self.valueLabel.hidden = !value.length;
  self.valueLabel.accessibilityLabel = value;
  _value = value;
}

#pragma mark - Private

// The collection view header always stretch across the whole collection view
// width. to work around that, this method adds a padding to the container view
// based on the current layout and the size classes.
- (void)updateContentInsets {
  UIEdgeInsets contentInsets;
  CGFloat width = CGRectGetWidth(self.bounds);
  UIUserInterfaceSizeClass horizontalSizeClass =
      self.traitCollection.horizontalSizeClass;
  UIUserInterfaceSizeClass verticalSizeClass =
      self.traitCollection.verticalSizeClass;
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    contentInsets = kGridLayoutInsetsRegularCompact;
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    if (width < kGridLayoutCompactCompactLimitedWidth) {
      contentInsets = kGridLayoutInsetsCompactCompactLimitedWidth;
    } else {
      contentInsets = kGridLayoutInsetsCompactCompact;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassRegular) {
    if (width < kGridLayoutCompactRegularLimitedWidth) {
      contentInsets = kGridLayoutInsetsCompactRegularLimitedWidth;
    } else {
      contentInsets = kGridLayoutInsetsCompactRegular;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassRegular &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    contentInsets = kGridLayoutInsetsRegularCompact;
  } else {
    contentInsets = kGridLayoutInsetsRegularRegular;
  }
  self.containerView.layoutMargins =
      UIEdgeInsetsMake(0, contentInsets.left, 0, contentInsets.right);
  [self layoutIfNeeded];
}

@end
