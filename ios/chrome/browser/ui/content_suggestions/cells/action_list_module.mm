// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/action_list_module.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical spacing between the content views.
const float kContentVerticalSpacing = 12.0f;

// The horizontal inset for the content within this container.
const float kContentHorizontalInset = 16.0f;

// The top inset for the content within this container.
const float kContentTopInset = 14.0f;

// The bottom inset for the content within this container.
const float kContentBottomInset = 10.0f;

}  // namespace

@implementation ActionListModule {
  NSLayoutConstraint* _contentViewWidthAnchor;
}

- (instancetype)initWithContentView:(UIView*)contentView
                               type:(ContentSuggestionsModuleType)type {
  self = [super initWithType:type];
  if (self) {
    UILabel* title = [[UILabel alloc] init];
    title.text = [self titleString];
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    title.textColor = [UIColor colorNamed:kTextSecondaryColor];
    title.accessibilityTraits |= UIAccessibilityTraitHeader;
    title.accessibilityIdentifier = [self titleString];

    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.alignment = UIStackViewAlignmentLeading;
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.spacing = kContentVerticalSpacing;
    stackView.distribution = UIStackViewDistributionFill;
    [stackView addArrangedSubview:title];
    [stackView addArrangedSubview:contentView];

    self.accessibilityElements = @[ title, contentView ];

    _contentViewWidthAnchor = [contentView.widthAnchor
        constraintEqualToConstant:[self contentViewWidth]];
    [NSLayoutConstraint activateConstraints:@[ _contentViewWidthAnchor ]];

    [self addSubview:stackView];
    NSDirectionalEdgeInsets contentInsets = NSDirectionalEdgeInsetsMake(
        kContentTopInset, kContentHorizontalInset, kContentBottomInset,
        kContentHorizontalInset);
    AddSameConstraintsWithInsets(stackView, self, contentInsets);
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
         (kContentHorizontalInset * 2);
}

@end
