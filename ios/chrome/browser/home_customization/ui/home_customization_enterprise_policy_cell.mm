// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_enterprise_policy_cell.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The border radius for the entire cell.
const CGFloat kContainerBorderRadius = 12;

// The margins of the container.
const CGFloat kHorizontalMargin = 12;
const CGFloat kVerticalMargin = 8;

// The width of the main icon image view.
const CGFloat kIconImageViewWidth = 32;

// Helper function to create the attributed string with a link.
NSAttributedString* GetAttributedString(NSString* message) {
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName : [UIFont systemFontOfSize:13
                                            weight:UIFontWeightRegular]
  };

  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName : [UIFont systemFontOfSize:13
                                            weight:UIFontWeightRegular],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
    NSLinkAttributeName :
        [NSString stringWithUTF8String:kManagementLearnMoreURL],
  };

  return AttributedStringFromStringWithLink(message, textAttributes,
                                            linkAttributes);
}

}  // namespace

@interface HomeCustomizationEnterprisePolicyCell () <UITextViewDelegate>
@end

@implementation HomeCustomizationEnterprisePolicyCell {
  // The text view that displays the enterprise management message.
  UITextView* _titleTextView;
  // The mutator for handling learn more actions.
  id<HomeCustomizationMutator> _mutator;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.layer.cornerRadius = kContainerBorderRadius;
    self.contentView.layoutMargins = UIEdgeInsetsMake(
        kVerticalMargin, kHorizontalMargin, kVerticalMargin, kHorizontalMargin);

    // Create Icon
    UIImageView* iconImageView = [[UIImageView alloc]
        initWithImage:SymbolWithPalette(
                          CustomSymbolWithPointSize(kEnterpriseSymbol, 18),
                          @[ [UIColor colorNamed:kTextSecondaryColor] ])];
    iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    iconImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    iconImageView.contentMode = UIViewContentModeCenter;

    // Create Text View
    _titleTextView = [[UITextView alloc] init];
    _titleTextView.editable = NO;
    _titleTextView.scrollEnabled = NO;
    _titleTextView.delegate = self;
    _titleTextView.textContainerInset = UIEdgeInsetsZero;
    _titleTextView.textContainer.lineFragmentPadding = 0;
    _titleTextView.backgroundColor = [UIColor clearColor];
    _titleTextView.attributedText = GetAttributedString(
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_INFO));

    // Create Content Stack View
    UIStackView* contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ iconImageView, _titleTextView ]];
    contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
    contentStackView.axis = UILayoutConstraintAxisHorizontal;
    contentStackView.alignment = UIStackViewAlignmentCenter;
    contentStackView.spacing = UIStackViewSpacingUseSystem;

    [self.contentView addSubview:contentStackView];

    // Activate Constraints
    [NSLayoutConstraint activateConstraints:@[
      [iconImageView.widthAnchor constraintEqualToConstant:kIconImageViewWidth],
    ]];
    AddSameConstraints(contentStackView, self.contentView.layoutMarginsGuide);
  }
  return self;
}

#pragma mark - Public

- (void)configureCellWithMutator:(id<HomeCustomizationMutator>)mutator {
  _mutator = mutator;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange {
  [_mutator navigateToLinkForType:CustomizationLinkType::kEnterpriseLearnMore];
  // Return NO because the navigation is handled by the mutator.
  return NO;
}

@end
