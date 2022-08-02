// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_header_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kCardUnmaskPromptTableViewAccessibilityID =
    @"CardUnmaskPromptTableViewAccessibilityID";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMain = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
};

}  // namespace

@interface CardUnmaskPromptViewController () {
  // Button displayed on the right side of the navigation bar.
  // Tapping it sends the data in the prompt for verification.
  UIBarButtonItem* _confirmButton;
  // Owns `self`.
  autofill::CardUnmaskPromptViewBridge* _bridge;  // weak
}

@end

@implementation CardUnmaskPromptViewController

- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _bridge = bridge;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      kCardUnmaskPromptTableViewAccessibilityID;

  self.title =
      base::SysUTF16ToNSString(_bridge->GetController()->GetWindowTitle());

  self.navigationItem.leftBarButtonItem = [self createCancelButton];
  _confirmButton = [self createConfirmButton];
  // Disable confirm button by default. It will be enabled after valid data is
  // entered in the prompt.
  _confirmButton.enabled = NO;
  self.navigationItem.rightBarButtonItem = _confirmButton;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierMain];

  [model setHeader:[self createHeaderItem]
      forSectionWithIdentifier:SectionIdentifierMain];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // Notify bridge that UI was dismissed.
  _bridge->NavigationControllerDismissed();
  _bridge = nullptr;
}

#pragma mark - Actions

- (void)onCancelTapped {
  _bridge->PerformClose();
}

- (void)onVerifyTapped {
  NOTIMPLEMENTED();
}

#pragma mark - Private

// Returns a newly created item for the footer of the section.
- (CVCHeaderItem*)createHeaderItem {
  autofill::CardUnmaskPromptController* controller = _bridge->GetController();
  NSString* instructions =
      base::SysUTF16ToNSString(controller->GetInstructionsMessage());

  CVCHeaderItem* header = [[CVCHeaderItem alloc] initWithType:ItemTypeHeader];
  header.instructionsText = instructions;

  return header;
}

// Returns a new cancel button for the navigation bar.
- (UIBarButtonItem*)createCancelButton {
  UIBarButtonItem* cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onCancelTapped)];

  return cancelButton;
}

// Returns a new confirm button for the navigation bar.
- (UIBarButtonItem*)createConfirmButton {
  NSString* confirmButtonText =
      base::SysUTF16ToNSString(_bridge->GetController()->GetOkButtonLabel());
  UIBarButtonItem* confirmButton =
      [[UIBarButtonItem alloc] initWithTitle:confirmButtonText
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onVerifyTapped)];
  [confirmButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]
  }
                               forState:UIControlStateNormal];
  [confirmButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kDisabledTintColor]
  }
                               forState:UIControlStateDisabled];

  return confirmButton;
}

@end
