// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/whats_new/ui/whats_new_instructions_view_controller.h"

#import "base/values.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
NSString* const kWhatsNewInstructionsLabelAccessibilityIdentifier =
    @"WhatsNewTitleAccessibilityIdentifier";
}  // namespace

@interface WhatsNewInstructionsViewController ()

// Child view controller used to display the alert full-screen.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;
// What's New item.
@property(nonatomic, strong) WhatsNewItem* item;

@end

@implementation WhatsNewInstructionsViewController

- (instancetype)initWithWhatsNewItem:(WhatsNewItem*)item {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _item = item;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self addChildViewController:self.alertScreen];
  [self.view addSubview:self.alertScreen.view];
  [self.alertScreen didMoveToParentViewController:self];
  [self layoutAlertScreen];
}

#pragma mark - Private

// Configures the alertScreen view.
- (ConfirmationAlertViewController*)alertScreen {
  if (_alertScreen) {
    return _alertScreen;
  }

  UIView* instructionView =
      [[InstructionView alloc] initWithList:self.item.instructionSteps];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;
  instructionView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString = self.item.primaryActionTitle;
  if (self.item.learnMoreURL.is_valid()) {
    configuration.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_WHATS_NEW_LEARN_MORE_ACTION_TITLE);
  }
  _alertScreen = [[ConfirmationAlertViewController alloc]
      initWithConfiguration:configuration];
  _alertScreen.underTitleView = instructionView;
  _alertScreen.actionHandler = self.actionHandler;

  self.title =
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_SHOW_INSTRUCTIONS_TITLE);

  _alertScreen.topAlignedLayout = YES;

  [NSLayoutConstraint activateConstraints:@[
    [instructionView.leadingAnchor
        constraintEqualToAnchor:_alertScreen.view.leadingAnchor
                       constant:17],
    [instructionView.trailingAnchor
        constraintEqualToAnchor:_alertScreen.view.trailingAnchor
                       constant:-17],
  ]];

  return _alertScreen;
}

// Sets the layout of the alertScreen view when the promo will be
// shown without the animation view (half-screen promo).
- (void)layoutAlertScreen {
  self.alertScreen.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.alertScreen.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  self.alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.alertScreen.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.alertScreen.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

@end
