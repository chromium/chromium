// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_button_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The spacing in points between the buttons.
constexpr CGFloat kButtonStackViewSpacing = 8.0;

// Width ratio of the leading quick action button to the total width of the
// quick actions row when there are three (3) buttons in the row.
constexpr CGFloat kLeadingActionWidthFactor = 0.36;

}  // namespace

@implementation NewTabPageQuickActionsViewController {
  // The stack view containing the quick actions buttons.
  UIStackView* _buttonStackView;

  // Quick action buttons.
  UIButton* _aimButton;
  UIButton* _aimImageGenerationButton;
  UIButton* _aimAttachImageButton;
  UIButton* _incognitoSearchButton;

  // Whether AIM image generation is allowed.
  BOOL _aimImageGenerationAllowed;
  // Whether attaching images to an AIM search query is allowed.
  BOOL _aimAttachImagesAllowed;

  // Constraint for the width of the AI Mode button in Split Toolbar mode.
  NSLayoutConstraint* _aimButtonSplitToolbarWidthConstraint;

  // Constraints for the width of the additional AIM buttons.
  NSLayoutConstraint* _aimImageGenerationButtonWidthConstraint;
  NSLayoutConstraint* _aimAttachImageButtonWidthConstraint;

  // The custom title for the AIM button.
  NSString* _aimTitle;
  // The custom icon for the AIM button.
  UIImage* _aimIcon;
  // The custom accessibility label for the AIM button.
  NSString* _aimAccessibilityLabel;
}

#pragma mark - Public

- (void)setAIMTitle:(NSString*)title
                  icon:(UIImage*)icon
    accessibilityLabel:(NSString*)accessibilityLabel {
  _aimTitle = [title copy];
  _aimIcon = icon;
  _aimAccessibilityLabel = [accessibilityLabel copy];
  [NewTabPageQuickActionsButtonFactory updateButton:_aimButton
                                          withTitle:_aimTitle
                                               icon:_aimIcon
                                 accessibilityLabel:_aimAccessibilityLabel];
}

- (void)setAIMImageGenerationAllowed:(BOOL)imageGenerationAllowed
                 attachImagesAllowed:(BOOL)attachmentsAllowed {
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  CHECK(arm == AimButtonRefactorArm::kImageGenerationQuickAction ||
        arm == AimButtonRefactorArm::kAttachImageQuickAction);
  if (imageGenerationAllowed == _aimImageGenerationAllowed &&
      attachmentsAllowed == _aimAttachImagesAllowed) {
    return;
  }
  _aimImageGenerationAllowed = imageGenerationAllowed;
  _aimAttachImagesAllowed = attachmentsAllowed;
  [self updateButtonVisibility];
}

#pragma mark - Accessors & Mutators

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  if (layoutGuideCenter == _layoutGuideCenter) {
    return;
  }
  _layoutGuideCenter = layoutGuideCenter;
  if (_aimButton) {
    [_layoutGuideCenter referenceView:_aimButton underName:kNTPAIMButtonGuide];
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self createSubviews];
}

- (CGSize)preferredContentSize {
  return CGSizeMake(super.preferredContentSize.width, QuickActionsHeight());
}

#pragma mark - Private

// Creates the subviews for the Quick Actions row.
- (void)createSubviews {
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  switch (arm) {
    case AimButtonRefactorArm::kFocusComposeboxAimQuickAction:
    case AimButtonRefactorArm::kDisabled: {
      _aimButton = [NewTabPageQuickActionsButtonFactory
          aimButtonWithTitle:_aimTitle
                        icon:_aimIcon
          accessibilityLabel:_aimAccessibilityLabel];
      _incognitoSearchButton = [NewTabPageQuickActionsButtonFactory
          incognitoSearchButtonWithTitle:YES];

      _buttonStackView = [self createButtonStackWithArrangedSubviews:@[
        _aimButton, _incognitoSearchButton
      ]];
      break;
    }
    case AimButtonRefactorArm::kImageGenerationQuickAction: {
      _aimButton = [NewTabPageQuickActionsButtonFactory
          aimButtonWithTitle:_aimTitle
                        icon:_aimIcon
          accessibilityLabel:_aimAccessibilityLabel];
      _aimImageGenerationButton =
          [NewTabPageQuickActionsButtonFactory aimImageGenerationButton];
      // Attach Images is a fallback button if image generation is not
      // available.
      _aimAttachImageButton =
          [NewTabPageQuickActionsButtonFactory aimAttachImageButton];
      _incognitoSearchButton = [NewTabPageQuickActionsButtonFactory
          incognitoSearchButtonWithTitle:NO];

      _buttonStackView = [self createButtonStackWithArrangedSubviews:@[
        _aimButton, _aimImageGenerationButton, _aimAttachImageButton,
        _incognitoSearchButton
      ]];

      _aimButtonSplitToolbarWidthConstraint = [_aimButton.widthAnchor
          constraintEqualToAnchor:_buttonStackView.widthAnchor
                       multiplier:kLeadingActionWidthFactor];
      _aimImageGenerationButtonWidthConstraint =
          [_aimImageGenerationButton.widthAnchor
              constraintEqualToAnchor:_incognitoSearchButton.widthAnchor];
      _aimAttachImageButtonWidthConstraint = [_aimAttachImageButton.widthAnchor
          constraintEqualToAnchor:_incognitoSearchButton.widthAnchor];

      [NSLayoutConstraint activateConstraints:@[
        _aimButtonSplitToolbarWidthConstraint,
        _aimImageGenerationButtonWidthConstraint,
        _aimAttachImageButtonWidthConstraint,
      ]];
      [self updateButtonVisibility];
      break;
    }
    case AimButtonRefactorArm::kAttachImageQuickAction: {
      _aimButton = [NewTabPageQuickActionsButtonFactory
          aimButtonWithTitle:_aimTitle
                        icon:_aimIcon
          accessibilityLabel:_aimAccessibilityLabel];
      _aimAttachImageButton =
          [NewTabPageQuickActionsButtonFactory aimAttachImageButton];
      _incognitoSearchButton = [NewTabPageQuickActionsButtonFactory
          incognitoSearchButtonWithTitle:NO];

      _buttonStackView = [self createButtonStackWithArrangedSubviews:@[
        _aimButton, _aimAttachImageButton, _incognitoSearchButton
      ]];

      _aimButtonSplitToolbarWidthConstraint = [_aimButton.widthAnchor
          constraintEqualToAnchor:_buttonStackView.widthAnchor
                       multiplier:kLeadingActionWidthFactor];
      _aimAttachImageButtonWidthConstraint = [_aimAttachImageButton.widthAnchor
          constraintEqualToAnchor:_incognitoSearchButton.widthAnchor];

      [NSLayoutConstraint activateConstraints:@[
        _aimButtonSplitToolbarWidthConstraint,
        _aimAttachImageButtonWidthConstraint,
      ]];
      [self updateButtonVisibility];
      break;
    }
    case AimButtonRefactorArm::kAimAsModule:
    case AimButtonRefactorArm::kAimAsMvt:
    case AimButtonRefactorArm::kNoChips:
      // No quick actions row.
      return;
  }

  [self.layoutGuideCenter referenceView:_aimButton
                              underName:kNTPAIMButtonGuide];

  // Add button actions.
  [_aimButton addTarget:self
                 action:@selector(didTapAIMButton)
       forControlEvents:UIControlEventTouchUpInside];
  [_aimImageGenerationButton addTarget:self
                                action:@selector(didTapAIMImageGenerationButton)
                      forControlEvents:UIControlEventTouchUpInside];
  [_aimAttachImageButton addTarget:self
                            action:@selector(didTapAIMAttachImageButton)
                  forControlEvents:UIControlEventTouchUpInside];
  [_incognitoSearchButton addTarget:self
                             action:@selector(didTapIncognitoSearchButton)
                   forControlEvents:UIControlEventTouchUpInside];

  [self.view addSubview:_buttonStackView];
  AddSameConstraints(_buttonStackView, self.view);

  [NSLayoutConstraint activateConstraints:@[
    [_buttonStackView.heightAnchor
        constraintEqualToConstant:QuickActionsHeight()],
  ]];

  if (arm == AimButtonRefactorArm::kImageGenerationQuickAction ||
      arm == AimButtonRefactorArm::kAttachImageQuickAction) {
    [self registerForTraitChanges:@[
      UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class
    ]
                       withAction:@selector(updateButtonStackConstraints)];
    [self updateButtonStackConstraints];
  }
}

// Creates a horizontal stack view for the Quick Action buttons.
- (UIStackView*)createButtonStackWithArrangedSubviews:
    (NSArray<UIView*>*)buttons {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:buttons];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kButtonStackViewSpacing;
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  if (arm == AimButtonRefactorArm::kFocusComposeboxAimQuickAction ||
      arm == AimButtonRefactorArm::kDisabled) {
    stackView.distribution = UIStackViewDistributionFillEqually;
  }
  return stackView;
}

// Updates the visibility of the AI Quick Action buttons based on eligibility.
- (void)updateButtonVisibility {
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  if (arm == AimButtonRefactorArm::kImageGenerationQuickAction) {
    _aimImageGenerationButton.hidden = !_aimImageGenerationAllowed;
    _aimAttachImageButton.hidden =
        _aimImageGenerationAllowed || !_aimAttachImagesAllowed;
    [self updateButtonStackConstraints];
    return;
  }

  if (arm == AimButtonRefactorArm::kAttachImageQuickAction) {
    _aimAttachImageButton.hidden = !_aimAttachImagesAllowed;
    [self updateButtonStackConstraints];
    return;
  }
}

// Updates the horizontal constraints for the button stack view based on the
// layout environment.
- (void)updateButtonStackConstraints {
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  CHECK(arm == AimButtonRefactorArm::kImageGenerationQuickAction ||
        arm == AimButtonRefactorArm::kAttachImageQuickAction);
  NSUInteger numberOfVisibleButtons = [self numberOfVisibleButtons];

  UIButton* incognitoSearchButton = _incognitoSearchButton;
  NSString* incognitoSearchButtonTitle =
      (numberOfVisibleButtons < 3)
          ? l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_INCOGNITO)
          : nil;
  [NewTabPageQuickActionsButtonFactory
            updateButton:incognitoSearchButton
               withTitle:incognitoSearchButtonTitle
                    icon:incognitoSearchButton.configuration.image
      accessibilityLabel:incognitoSearchButton.accessibilityLabel];

  _aimImageGenerationButtonWidthConstraint.active =
      !_aimImageGenerationButton.isHidden;
  _aimAttachImageButtonWidthConstraint.active = !_aimAttachImageButton.isHidden;

  if (!IsSplitToolbarMode(self) || numberOfVisibleButtons < 3) {
    _aimButtonSplitToolbarWidthConstraint.active = NO;
    _buttonStackView.distribution = UIStackViewDistributionFillEqually;
    return;
  }
  _aimButtonSplitToolbarWidthConstraint.active = YES;
  _buttonStackView.distribution = UIStackViewDistributionFill;
}

// Returns the number of visible buttons in `_buttonStackView`.
- (NSUInteger)numberOfVisibleButtons {
  if (!_buttonStackView) {
    return 0;
  }

  NSUInteger visibleButtonsCount = 0;
  for (UIView* button in _buttonStackView.arrangedSubviews) {
    if (!button.isHidden) {
      visibleButtonsCount++;
    }
  }
  return visibleButtonsCount;
}

#pragma mark - Actions

- (void)didTapAIMButton {
  [self.NTPShortcutsHandler openAIM];
}

- (void)didTapAIMImageGenerationButton {
  CHECK_EQ(GetAimButtonRefactorArm(),
           AimButtonRefactorArm::kImageGenerationQuickAction);
  [self.NTPShortcutsHandler openAIMImageGeneration];
}

- (void)didTapAIMAttachImageButton {
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  CHECK(arm == AimButtonRefactorArm::kAttachImageQuickAction ||
        arm == AimButtonRefactorArm::kImageGenerationQuickAction);
  [self.NTPShortcutsHandler openAIMAttachImage];
}

- (void)didTapIncognitoSearchButton {
  [self.NTPShortcutsHandler openIncognitoSearch];
}

@end
