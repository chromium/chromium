// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_header.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

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

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.title = nil;
  self.value = nil;
  self.titleLabel.text = nil;
  self.valueLabel.text = nil;
  self.valueLabel.hidden = YES;
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

@end
