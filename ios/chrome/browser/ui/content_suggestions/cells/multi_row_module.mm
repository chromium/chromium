// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/multi_row_module.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical spacing between the title and the content.
const CGFloat kContentVerticalSpacing = 12.0f;

// The horizontal inset for the content within this container.
const CGFloat kContentHorizontalInset = 16.0f;

// The top inset for the content within this container.
const CGFloat kContentTopInset = 14.0f;

// The bottom inset for the content within this container.
const CGFloat kContentBottomInset = 10.0f;

const CGFloat kSeparatorHeight = 0.5;

}  // namespace

@implementation MultiRowModule {
  NSLayoutConstraint* _contentViewWidthAnchor;
}

- (instancetype)initWithViews:(NSArray<UIView*>*)views
                         type:(ContentSuggestionsModuleType)type {
  self = [super initWithType:type];
  if (self) {
    UILabel* title = [[UILabel alloc] init];
    title.text = [MagicStackModuleContainer titleStringForModule:type];
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    title.textColor = [UIColor colorNamed:kTextPrimaryColor];
    title.accessibilityTraits |= UIAccessibilityTraitHeader;
    title.accessibilityIdentifier =
        [MagicStackModuleContainer titleStringForModule:type];

    UIStackView* rowsStackView = [[UIStackView alloc] init];
    rowsStackView.spacing = AlignValueToPixel(8.5);
    rowsStackView.axis = UILayoutConstraintAxisVertical;
    rowsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    rowsStackView.alignment = UIStackViewAlignmentLeading;
    rowsStackView.distribution = UIStackViewDistributionFill;
    NSUInteger index = 0;
    for (UIView* view in views) {
      [rowsStackView addArrangedSubview:view];
      if (index < [views count] - 1) {
        UIView* separator = [[UIView alloc] init];
        separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
        [rowsStackView addArrangedSubview:separator];
        [NSLayoutConstraint activateConstraints:@[
          [separator.heightAnchor
              constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
          [separator.widthAnchor
              constraintEqualToConstant:[self contentViewWidth]],
        ]];
      }
      index++;
    }

    _contentViewWidthAnchor = [rowsStackView.widthAnchor
        constraintEqualToConstant:[self contentViewWidth]];
    [NSLayoutConstraint activateConstraints:@[ _contentViewWidthAnchor ]];

    UIStackView* containerStackView = [[UIStackView alloc] init];
    containerStackView.spacing = kContentVerticalSpacing;
    containerStackView.axis = UILayoutConstraintAxisVertical;
    containerStackView.translatesAutoresizingMaskIntoConstraints = NO;
    containerStackView.alignment = UIStackViewAlignmentLeading;
    containerStackView.distribution = UIStackViewDistributionFill;
    [containerStackView addArrangedSubview:title];
    [containerStackView addArrangedSubview:rowsStackView];

    [self addSubview:containerStackView];
    NSDirectionalEdgeInsets contentInsets = NSDirectionalEdgeInsetsMake(
        kContentTopInset, kContentHorizontalInset, kContentBottomInset, 0);
    AddSameConstraintsWithInsets(containerStackView, self, contentInsets);
  }
  return self;
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    _contentViewWidthAnchor.constant = [self contentViewWidth];
  }
}

#pragma mark - Helpers

// Returns the expected width of the contentView subview.
- (CGFloat)contentViewWidth {
  // Give content the same width as the StackView, which is inset from this
  // container view.
  return [MagicStackModuleContainer
             moduleWidthForHorizontalTraitCollection:self.traitCollection] -
         kContentHorizontalInset;
}

@end
