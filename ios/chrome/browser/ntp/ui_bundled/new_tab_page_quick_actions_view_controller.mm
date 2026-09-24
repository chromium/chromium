// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_button_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

using ntp_tiles::AimButtonRefactorArm;

// The spacing in points between the buttons.
constexpr CGFloat kButtonStackViewSpacing = 8.0;

// The height for the quick actions button row.
constexpr CGFloat kQuickActionsHeight = 44.0;
constexpr CGFloat kQuickActionsHeightUICleanup = 50.0;

// Width ratio of the leading quick action button to the total width of the
// quick actions row when there are three (3) buttons in the row.
constexpr CGFloat kLeadingActionWidthFactor = 0.5;

// The horizontal inset margin for the button stack view in a regular x regular
// size class.
constexpr CGFloat kHorizontalInsetRegularXRegular = 36.0;

// Returns the leading margin for the button stack based on the window's size
// class.
CGFloat HorizontalInsetForQuickActions(
    id<UITraitEnvironment> trait_environment) {
  if (!IsNewTabPageUICleanupEnabled()) {
    return 0.0;
  }
  return IsRegularXRegularSizeClass(trait_environment)
             ? kHorizontalInsetRegularXRegular
             : 0.0;
}

}  // namespace

@implementation NewTabPageQuickActionsViewController {
  // The stack view containing the quick actions buttons.
  UIStackView* _buttonStackView;

  // Quick action buttons.
  UIButton* _aimButton;
  UIButton* _aimImageGenerationButton;
  UIButton* _aimAttachImageButton;
  UIButton* _incognitoSearchButton;

  // Constraints for the leading and trailing edges of the `_buttonStackView`.
  NSLayoutConstraint* _stackViewLeadingConstraint;
  NSLayoutConstraint* _stackViewTrailingConstraint;
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
  return CGSizeMake(super.preferredContentSize.width,
                    IsNewTabPageUICleanupEnabled()
                        ? kQuickActionsHeightUICleanup
                        : kQuickActionsHeight);
}

#pragma mark - Private

// Creates the subviews for the Quick Actions row.
- (void)createSubviews {
  switch (ntp_tiles::GetAimButtonRefactorArm()) {
    case AimButtonRefactorArm::kFocusComposeboxAimQuickAction:
    case AimButtonRefactorArm::kDisabled: {
      _buttonStackView = [self createButtonStackView];
      _aimButton = [NewTabPageQuickActionsButtonFactory aimButtonWithTitle:YES];
      _incognitoSearchButton = [NewTabPageQuickActionsButtonFactory
          incognitoSearchButtonWithTitle:YES];
      [_buttonStackView addArrangedSubview:_aimButton];
      [_buttonStackView addArrangedSubview:_incognitoSearchButton];
      _buttonStackView.distribution = UIStackViewDistributionFillEqually;
      break;
    }
    case AimButtonRefactorArm::kImageGenerationQuickAction: {
      _buttonStackView = [self createButtonStackView];
      _aimButton = [NewTabPageQuickActionsButtonFactory aimButtonWithTitle:YES];
      _aimImageGenerationButton =
          [NewTabPageQuickActionsButtonFactory aimImageGenerationButton];
      _incognitoSearchButton = [NewTabPageQuickActionsButtonFactory
          incognitoSearchButtonWithTitle:NO];
      [_buttonStackView addArrangedSubview:_aimButton];
      [_buttonStackView addArrangedSubview:_aimImageGenerationButton];
      [_buttonStackView addArrangedSubview:_incognitoSearchButton];
      _buttonStackView.distribution = UIStackViewDistributionFill;
      [NSLayoutConstraint activateConstraints:@[
        [_aimButton.widthAnchor
            constraintEqualToAnchor:_buttonStackView.widthAnchor
                         multiplier:kLeadingActionWidthFactor],
        [_aimImageGenerationButton.widthAnchor
            constraintEqualToAnchor:_incognitoSearchButton.widthAnchor],
      ]];
      break;
    }
    case AimButtonRefactorArm::kAttachImageQuickAction: {
      _buttonStackView = [self createButtonStackView];
      _aimButton = [NewTabPageQuickActionsButtonFactory aimButtonWithTitle:YES];
      _aimAttachImageButton =
          [NewTabPageQuickActionsButtonFactory aimAttachImageButton];
      _incognitoSearchButton = [NewTabPageQuickActionsButtonFactory
          incognitoSearchButtonWithTitle:NO];
      [_buttonStackView addArrangedSubview:_aimButton];
      [_buttonStackView addArrangedSubview:_aimAttachImageButton];
      [_buttonStackView addArrangedSubview:_incognitoSearchButton];
      _buttonStackView.distribution = UIStackViewDistributionFill;
      [NSLayoutConstraint activateConstraints:@[
        [_aimButton.widthAnchor
            constraintEqualToAnchor:_buttonStackView.widthAnchor
                         multiplier:kLeadingActionWidthFactor],
        [_aimAttachImageButton.widthAnchor
            constraintEqualToAnchor:_incognitoSearchButton.widthAnchor],
      ]];
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

  CGFloat inset = HorizontalInsetForQuickActions(self);

  _stackViewLeadingConstraint = [_buttonStackView.leadingAnchor
      constraintEqualToAnchor:self.view.leadingAnchor
                     constant:inset];
  _stackViewTrailingConstraint = [_buttonStackView.trailingAnchor
      constraintEqualToAnchor:self.view.trailingAnchor
                     constant:-inset];

  [NSLayoutConstraint activateConstraints:@[
    [_buttonStackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_buttonStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_buttonStackView.heightAnchor
        constraintEqualToConstant:IsNewTabPageUICleanupEnabled()
                                      ? kQuickActionsHeightUICleanup
                                      : kQuickActionsHeight],
    _stackViewLeadingConstraint,
    _stackViewTrailingConstraint,
  ]];

  if (IsNewTabPageUICleanupEnabled()) {
    [self registerForTraitChanges:@[
      UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class
    ]
                       withAction:@selector(updateButtonStackConstraints)];
  }
}

// Creates a horizontal stack view for the Quick Action buttons.
- (UIStackView*)createButtonStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kButtonStackViewSpacing;
  return stackView;
}

// Updates the horizontal constraints for the button stack view based on the
// layout environment.
- (void)updateButtonStackConstraints {
  CHECK(IsNewTabPageUICleanupEnabled());
  if (!_stackViewLeadingConstraint && !_stackViewTrailingConstraint) {
    return;
  }
  CGFloat inset = HorizontalInsetForQuickActions(self);
  _stackViewLeadingConstraint.constant = inset;
  _stackViewTrailingConstraint.constant = -inset;
}

#pragma mark - Actions

- (void)didTapAIMButton {
  [self.NTPShortcutsHandler openAIM];
}

- (void)didTapAIMImageGenerationButton {
  CHECK_EQ(ntp_tiles::GetAimButtonRefactorArm(),
           AimButtonRefactorArm::kImageGenerationQuickAction);
  [self.NTPShortcutsHandler openAIMImageGeneration];
}

- (void)didTapAIMAttachImageButton {
  CHECK_EQ(ntp_tiles::GetAimButtonRefactorArm(),
           AimButtonRefactorArm::kAttachImageQuickAction);
  [self.NTPShortcutsHandler openAIMAttachImage];
}

- (void)didTapIncognitoSearchButton {
  [self.NTPShortcutsHandler openIncognitoSearch];
}

@end
