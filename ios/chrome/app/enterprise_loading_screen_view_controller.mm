// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/enterprise_loading_screen_view_controller.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"

namespace {
// Space between the loading icon and text.
constexpr CGFloat kSpacingHeight = 10;
// Space between the bottom margin of the view to the bottom of the screen.
constexpr CGFloat kPaddingHeight = 50;
}  // namespace

@interface EnterpriseLoadScreenViewController ()

// Text displayed during the loading.
@property(nonatomic, strong) UILabel* loadingLabel;

@end

@implementation EnterpriseLoadScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.detailView = [self createStatusView];
  [super viewDidLoad];
  // Override the accessibility ID defined in LaunchScreenViewController.
  self.view.accessibilityIdentifier =
      first_run::kEnterpriseLoadingScreenAccessibilityIdentifier;
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.self ]);
    __weak EnterpriseLoadScreenViewController* weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      // Limit the size of text to avoid truncation.
      weakSelf.loadingLabel.font = PreferredFontForTextStyleWithMaxCategory(
          UIFontTextStyleBody,
          weakSelf.traitCollection.preferredContentSizeCategory,
          UIContentSizeCategoryExtraExtraExtraLarge);
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  // Limit the size of text to avoid truncation.
  self.loadingLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);
}
#endif

#pragma mark - Private

// Creates and configures the status view which contains the loading spinner and
// loading text.
- (UIStackView*)createStatusView {
  self.loadingLabel = [[UILabel alloc] init];
  // Chrome's localization utilities aren't available at this stage, so this
  // method uses the native iOS API.
  self.loadingLabel.text =
      NSLocalizedString(@"IDS_IOS_FIRST_RUN_LAUNCH_SCREEN_ENTERPRISE", @"");

  // Limit the size of text to avoid truncation.
  self.loadingLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);

  self.loadingLabel.numberOfLines = 0;
  self.loadingLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.loadingLabel.textAlignment = NSTextAlignmentCenter;

  UIActivityIndicatorView* spinner = [[UIActivityIndicatorView alloc] init];
  [spinner startAnimating];

  UIView* spacing = [[UIView alloc] init];
  spacing.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* bottomPadding = [[UIView alloc] init];
  bottomPadding.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* statusStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        spinner, spacing, self.loadingLabel, bottomPadding
      ]];
  statusStackView.axis = UILayoutConstraintAxisVertical;
  statusStackView.translatesAutoresizingMaskIntoConstraints = NO;
  statusStackView.alignment = UIStackViewAlignmentCenter;
  statusStackView.spacing = UIStackViewSpacingUseSystem;

  [NSLayoutConstraint activateConstraints:@[
    [spacing.heightAnchor constraintEqualToConstant:kSpacingHeight],
    [bottomPadding.heightAnchor constraintEqualToConstant:kPaddingHeight]
  ]];
  return statusStackView;
}

@end
