// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_header_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

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

@end
