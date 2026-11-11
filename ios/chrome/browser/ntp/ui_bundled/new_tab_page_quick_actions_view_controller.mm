// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_quick_action_button.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// The spacing in points between the buttons.
constexpr CGFloat kButtonStackViewSpacing = 8.0;

// Width of the compact quick action buttons when in Split Toolbar mode with
// at least 3 visible buttons.
constexpr CGFloat kCompactButtonWidth = 80.0;

}  // namespace

@implementation NewTabPageQuickActionsViewController {
  // The stack view containing the quick actions buttons.
  UIStackView* _buttonStackView;

  // Quick action buttons.
  NTPQuickActionButton* _aimButton;
  NTPQuickActionButton* _aimImageGenerationButton;
  NTPQuickActionButton* _aimAttachImageButton;
  NTPQuickActionButton* _incognitoSearchButton;

  // Whether AIM image generation is allowed.
  BOOL _aimImageGenerationAllowed;
  // Whether attaching images to an AIM search query is allowed.
  BOOL _aimAttachImagesAllowed;

  // Width constraints for the compact buttons in Split Toolbar mode.
  NSLayoutConstraint* _aimImageGenerationButtonWidthConstraint;
  NSLayoutConstraint* _aimAttachImageButtonWidthConstraint;
  NSLayoutConstraint* _incognitoSearchButtonWidthConstraint;

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
  [_aimButton updateTitle:_aimTitle
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
      _aimButton = [NTPQuickActionButton
          aimButtonWithTitle:_aimTitle
                        icon:_aimIcon
          accessibilityLabel:_aimAccessibilityLabel
                  layoutType:NTPQuickActionButtonLayout::kRegular];
      _incognitoSearchButton = [NTPQuickActionButton
          incognitoSearchButton:NTPQuickActionButtonLayout::kRegular];

      _buttonStackView = [self createButtonStackWithArrangedSubviews:@[
        _aimButton, _incognitoSearchButton
      ]];
      break;
    }
    case AimButtonRefactorArm::kImageGenerationQuickAction: {
      _aimButton = [NTPQuickActionButton
          aimButtonWithTitle:_aimTitle
                        icon:_aimIcon
          accessibilityLabel:_aimAccessibilityLabel
                  layoutType:NTPQuickActionButtonLayout::kRegular];
      _aimImageGenerationButton = [NTPQuickActionButton
          aimImageGenerationButton:NTPQuickActionButtonLayout::kCompact];
      // Attach Images is a fallback button if image generation is not
      // available.
      _aimAttachImageButton = [NTPQuickActionButton
          aimAttachImageButton:NTPQuickActionButtonLayout::kCompact];
      _incognitoSearchButton = [NTPQuickActionButton
          incognitoSearchButton:NTPQuickActionButtonLayout::kCompact];

      _buttonStackView = [self createButtonStackWithArrangedSubviews:@[
        _aimButton, _aimImageGenerationButton, _aimAttachImageButton,
        _incognitoSearchButton
      ]];

      _aimImageGenerationButtonWidthConstraint =
          [_aimImageGenerationButton.widthAnchor
              constraintEqualToConstant:kCompactButtonWidth];
      _aimAttachImageButtonWidthConstraint = [_aimAttachImageButton.widthAnchor
          constraintEqualToConstant:kCompactButtonWidth];
      _incognitoSearchButtonWidthConstraint =
          [_incognitoSearchButton.widthAnchor
              constraintEqualToConstant:kCompactButtonWidth];
      break;
    }
    case AimButtonRefactorArm::kAttachImageQuickAction: {
      _aimButton = [NTPQuickActionButton
          aimButtonWithTitle:_aimTitle
                        icon:_aimIcon
          accessibilityLabel:_aimAccessibilityLabel
                  layoutType:NTPQuickActionButtonLayout::kRegular];
      _aimAttachImageButton = [NTPQuickActionButton
          aimAttachImageButton:NTPQuickActionButtonLayout::kCompact];
      _incognitoSearchButton = [NTPQuickActionButton
          incognitoSearchButton:NTPQuickActionButtonLayout::kCompact];

      _buttonStackView = [self createButtonStackWithArrangedSubviews:@[
        _aimButton, _aimAttachImageButton, _incognitoSearchButton
      ]];

      _aimAttachImageButtonWidthConstraint = [_aimAttachImageButton.widthAnchor
          constraintEqualToConstant:kCompactButtonWidth];
      _incognitoSearchButtonWidthConstraint =
          [_incognitoSearchButton.widthAnchor
              constraintEqualToConstant:kCompactButtonWidth];
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
    [self updateButtonVisibility];
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

  BOOL showAllButtonTitles =
      !IsSplitToolbarMode(self) || numberOfVisibleButtons < 3;
  NTPQuickActionButtonLayout dynamicLayoutType =
      showAllButtonTitles ? NTPQuickActionButtonLayout::kRegular
                          : NTPQuickActionButtonLayout::kCompact;

  _aimImageGenerationButtonWidthConstraint.active =
      !showAllButtonTitles && !_aimImageGenerationButton.isHidden;
  _aimAttachImageButtonWidthConstraint.active =
      !showAllButtonTitles && !_aimAttachImageButton.isHidden;
  _incognitoSearchButtonWidthConstraint.active = !showAllButtonTitles;

  _buttonStackView.distribution = showAllButtonTitles
                                      ? UIStackViewDistributionFillEqually
                                      : UIStackViewDistributionFill;
  _aimButton.layoutType = NTPQuickActionButtonLayout::kRegular;
  _aimImageGenerationButton.layoutType = dynamicLayoutType;
  _aimAttachImageButton.layoutType = dynamicLayoutType;
  _incognitoSearchButton.layoutType = dynamicLayoutType;
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
