// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kBackgroundAlpha = 0.6;
constexpr CGFloat kColoredDotSize = 21;
constexpr CGFloat kTitleHorizontalMargin = 16;
constexpr CGFloat kTitleVerticalMargin = 10;
constexpr CGFloat kHorizontalMargin = 32;
constexpr CGFloat kdotAndFieldContainerMargin = 44;
constexpr CGFloat kDotTitleSeparationMargin = 12;
constexpr CGFloat kTitleBackgroundCornerRadius = 17;
constexpr CGFloat kButtonsHeight = 50;
constexpr CGFloat kButtonsMargin = 8;
}

@implementation CreateTabGroupViewController {
  // Text input to name the group.
  UITextField* _tabGroupTextField;
  // Handler to handle user's actions.
  __weak id<TabGroupsCommands> _tabGroupsHandler;
}

- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(handler);
    _tabGroupsHandler = handler;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCreateTabGroupIdentifier;
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    self.view.backgroundColor = [[UIColor colorNamed:kGrey900Color]
        colorWithAlphaComponent:kBackgroundAlpha];
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* blurEffectView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:blurEffectView];
    AddSameConstraints(self.view, blurEffectView);
  } else {
    self.view.backgroundColor = [UIColor blackColor];
  }

  UIView* dotAndFieldContainer = [self configuredDotAndFieldContainer];
  UIButton* cancelButton = [self configuredCancelButton];
  [self.view addSubview:dotAndFieldContainer];
  [self.view addSubview:cancelButton];

  [NSLayoutConstraint activateConstraints:@[
    [dotAndFieldContainer.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [dotAndFieldContainer.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kdotAndFieldContainerMargin],
    [dotAndFieldContainer.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
    [cancelButton.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kButtonsMargin],
    [cancelButton.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [cancelButton.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [cancelButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
  ]];

  // To force display the keyboard when the view is shown.
  [_tabGroupTextField becomeFirstResponder];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - Private helpers

// Configures the text input dedicated for the group name.
- (UITextField*)configuredTabGroupNameTextFieldInput {
  UITextField* tabGroupTextField = [[UITextField alloc] init];
  tabGroupTextField.textColor = [UIColor colorNamed:kSolidWhiteColor];
  tabGroupTextField.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
  tabGroupTextField.adjustsFontForContentSizeCategory = YES;
  tabGroupTextField.translatesAutoresizingMaskIntoConstraints = NO;

  UITraitCollection* interfaceStyleDarkTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  UIColor* placeholderTextColor = [[UIColor colorNamed:kTextSecondaryColor]
      resolvedColorWithTraitCollection:interfaceStyleDarkTraitCollection];

  tabGroupTextField.attributedPlaceholder = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(
                         IDS_IOS_TAB_GROUP_CREATION_PLACEHOLDER)
          attributes:@{NSForegroundColorAttributeName : placeholderTextColor}];
  return tabGroupTextField;
}

// Returns the group color dot view.
- (UIView*)groupDotViewWithColor:(UIColor*)color {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kColoredDotSize / 2;
  dotView.backgroundColor = color;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor constraintEqualToConstant:kColoredDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kColoredDotSize],
  ]];

  return dotView;
}

// Returns the configured full primary title (colored dot and text title).
- (UIView*)configuredDotAndFieldContainer {
  UIView* titleBackground = [[UIView alloc] initWithFrame:CGRectZero];
  titleBackground.translatesAutoresizingMaskIntoConstraints = NO;
  titleBackground.backgroundColor = [UIColor colorWithWhite:1 alpha:0.1];
  titleBackground.layer.cornerRadius = kTitleBackgroundCornerRadius;
  titleBackground.opaque = NO;

  // TODO(crbug.com/1501837): Save the view to be able to change the color of
  // the dot.
  UIView* coloredDotView =
      [self groupDotViewWithColor:[UIColor colorNamed:kYellow500Color]];
  _tabGroupTextField = [self configuredTabGroupNameTextFieldInput];

  [titleBackground addSubview:coloredDotView];
  [titleBackground addSubview:_tabGroupTextField];

  [NSLayoutConstraint activateConstraints:@[
    [_tabGroupTextField.leadingAnchor
        constraintEqualToAnchor:coloredDotView.trailingAnchor
                       constant:kDotTitleSeparationMargin],
    [coloredDotView.centerYAnchor
        constraintEqualToAnchor:_tabGroupTextField.centerYAnchor],
    [coloredDotView.leadingAnchor
        constraintEqualToAnchor:titleBackground.leadingAnchor
                       constant:kTitleHorizontalMargin],
    [titleBackground.trailingAnchor
        constraintEqualToAnchor:_tabGroupTextField.trailingAnchor
                       constant:kTitleHorizontalMargin],
    [_tabGroupTextField.topAnchor
        constraintEqualToAnchor:titleBackground.topAnchor
                       constant:kTitleVerticalMargin],
    [titleBackground.bottomAnchor
        constraintEqualToAnchor:_tabGroupTextField.bottomAnchor
                       constant:kTitleVerticalMargin],
  ]];
  return titleBackground;
}

// Returns the cancel button.
- (UIButton*)configuredCancelButton {
  UIButton* cancelButton = [[UIButton alloc] init];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.titleAlignment =
      UIButtonConfigurationTitleAlignmentCenter;
  NSDictionary* attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : [UIColor colorNamed:kSolidWhiteColor]
  };
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(IDS_CANCEL)
              attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;

  cancelButton.configuration = buttonConfiguration;

  [cancelButton addTarget:self
                   action:@selector(cancelButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];

  [NSLayoutConstraint activateConstraints:@[
    [cancelButton.heightAnchor constraintEqualToConstant:kButtonsHeight],
  ]];

  return cancelButton;
}

// Hides the current view without doing anything else.
- (void)cancelButtonTapped {
  [_tabGroupsHandler hideTabGroupCreation];
}

@end
