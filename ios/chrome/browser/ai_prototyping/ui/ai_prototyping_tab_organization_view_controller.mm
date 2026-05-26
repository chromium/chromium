// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_tab_organization_view_controller.h"

#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AIPrototypingTabOrganizationViewController () {
  UIButton* _groupTabsButton;
  UITextView* _responseContainer;
}

@end

@implementation AIPrototypingTabOrganizationViewController

// Synthesized from `AIPrototypingViewControllerProtocol`.
@synthesize mutator = _mutator;
@synthesize feature = _feature;

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  self = [super init];
  if (self) {
    _feature = feature;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_HEADER);

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  _groupTabsButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _groupTabsButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _groupTabsButton.layer.cornerRadius = kCornerRadius;
  [_groupTabsButton
      setTitle:l10n_util::GetNSString(
                   IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUP_TABS_BUTTON)
      forState:UIControlStateNormal];
  [_groupTabsButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_groupTabsButton addTarget:self
                       action:@selector(onGroupTabsButtonPressed:)
             forControlEvents:UIControlEventTouchUpInside];

  _responseContainer = [UITextView textViewUsingTextLayoutManager:NO];
  _responseContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _responseContainer.editable = NO;
  _responseContainer.layer.cornerRadius = kCornerRadius;
  _responseContainer.layer.masksToBounds = YES;
  _responseContainer.layer.borderColor = [primaryColor CGColor];
  _responseContainer.layer.borderWidth = kBorderWidth;
  _responseContainer.textContainer.lineBreakMode = NSLineBreakByWordWrapping;
  _responseContainer.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_RESULT_PLACEHOLDER);

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    label, _groupTabsButton, _responseContainer
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kMainStackViewSpacing;
  [self.view addSubview:stackView];

  [NSLayoutConstraint activateConstraints:@[
    [stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                        constant:kMainStackTopInset],
    [stackView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                            constant:kHorizontalInset],
    [stackView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                             constant:-kHorizontalInset],
    [_responseContainer.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor
                                  multiplier:
                                      kResponseContainerHeightMultiplier],
  ]];
}

- (void)onGroupTabsButtonPressed:(UIButton*)button {
  [self disableSubmitButton];
  [self updateResponseField:@""];
  [self.mutator executeSmartTabGrouping];
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)updateResponseField:(NSString*)response {
  _responseContainer.text = response;
}

- (void)enableSubmitButtons {
  _groupTabsButton.enabled = YES;
  _groupTabsButton.backgroundColor = [UIColor colorNamed:kBlueColor];
}

#pragma mark - Private

// Disable submit button, and style the accordingly.
- (void)disableSubmitButton {
  _groupTabsButton.enabled = NO;
  _groupTabsButton.backgroundColor = [UIColor colorNamed:kDisabledTintColor];
}

@end
