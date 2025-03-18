// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/instruction_view/instructions_half_sheet_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kPreferredCornerRadius = 20;
constexpr CGFloat kDismissSymbolSize = 22;
NSString* const kInstructionsHalfSheetTitleAccessibilityIdentifier =
    @"InstructionsHalfSheetTitleAccessibilityIdentifier";
constexpr CGFloat kInstructionsViewMargin = 17;
}  // namespace

@interface InstructionsHalfSheetViewController ()

@end

@implementation InstructionsHalfSheetViewController {
  // Child view controller to display the alert full-screen.
  ConfirmationAlertViewController* _alertScreen;
  // The label with title.
  UILabel* _titleLabel;
  // The list of steps to complete.
  NSArray<NSString*>* _instructionsList;
  // The action handler for the view controller;
  id<ConfirmationAlertActionHandler> _actionHandler;
}

#pragma mark - Public
- (instancetype)initWithInstructionList:(NSArray<NSString*>*)instructionList
                          actionHandler:(id<ConfirmationAlertActionHandler>)
                                            actionHandler {
  self = [super init];
  if (self) {
    _instructionsList = instructionList;
    _actionHandler = actionHandler;
  }
  return self;
}

#pragma mark - UI View Controller

- (void)viewDidLoad {
  [super viewDidLoad];
  _alertScreen = [self alertScreen];
  [self addChildViewController:_alertScreen];
  [self.view addSubview:_alertScreen.view];
  [_alertScreen didMoveToParentViewController:self];
  [self layoutAlertScreen];
}

#pragma mark - Private
// Configures the alertScreen view.
- (ConfirmationAlertViewController*)alertScreen {
  if (_alertScreen) {
    return _alertScreen;
  }

  if (!self.titleText) {
    self.titleText =
        l10n_util::GetNSString(IDS_IOS_SHOW_ME_HOW_FIRST_RUN_TITLE);
  }

  // Create the instructions view with the given list.
  UIView* instructionView =
      [[InstructionView alloc] initWithList:_instructionsList];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;
  instructionView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  _alertScreen = [[ConfirmationAlertViewController alloc] init];
  _alertScreen.primaryActionString = nil;
  _alertScreen.titleView = [self titleLabel];
  _alertScreen.actionHandler = _actionHandler;
  _alertScreen.underTitleView = instructionView;

  _alertScreen.showDismissBarButton = YES;
  UIImage* xmarkSymbol = SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kDismissSymbolSize),
      @[ [UIColor colorNamed:kGrey600Color] ]);
  _alertScreen.customDismissBarButtonImage = xmarkSymbol;

  _alertScreen.topAlignedLayout = YES;

  [NSLayoutConstraint activateConstraints:@[
    [instructionView.leadingAnchor
        constraintEqualToAnchor:_alertScreen.view.leadingAnchor
                       constant:kInstructionsViewMargin],
    [instructionView.trailingAnchor
        constraintEqualToAnchor:_alertScreen.view.trailingAnchor
                       constant:-kInstructionsViewMargin],
  ]];

  return _alertScreen;
}

// Sets the layout of the alertScreen view when the promo will be
// shown without the animation view (half-screen promo).
- (void)layoutAlertScreen {
  _alertScreen.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _alertScreen.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  presentationController.preferredCornerRadius = kPreferredCornerRadius;
}

- (UILabel*)titleLabel {
  if (_titleLabel) {
    return _titleLabel;
  }

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.numberOfLines = 0;
  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleBody, UIFontWeightSemibold);
  _titleLabel.text = _titleText;
  _titleLabel.textAlignment = NSTextAlignmentCenter;
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.accessibilityIdentifier =
      kInstructionsHalfSheetTitleAccessibilityIdentifier;
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  return _titleLabel;
}

@end
