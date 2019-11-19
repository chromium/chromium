// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/autofill/card_name_fix_flow_view_bridge.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kCellReuseID = @"ConfirmNameTableViewTextEditCell";
CGFloat const kMainSection = 0;
CGFloat const kNumberOfRowsInMainSection = 1;

namespace autofill {

#pragma mark CardNameFixFlowViewBridge

CardNameFixFlowViewBridge::CardNameFixFlowViewBridge(
    CardNameFixFlowController* controller,
    UIViewController* presenting_view_controller)
    : controller_(controller),
      presenting_view_controller_(presenting_view_controller) {
  DCHECK(controller_);
  DCHECK(presenting_view_controller_);
}

CardNameFixFlowViewBridge::~CardNameFixFlowViewBridge() {
  if (controller_)
    controller_->OnConfirmNameDialogClosed();
}

void CardNameFixFlowViewBridge::Show() {
  // Wrap CardNameFixFlowViewController with navigation controller to include
  // a navigation bar.
  view_controller_ = [[UINavigationController alloc]
      initWithRootViewController:[[CardNameFixFlowViewController alloc]
                                     initWithBridge:this]];
  [presenting_view_controller_ presentViewController:view_controller_
                                            animated:YES
                                          completion:nil];
}

void CardNameFixFlowViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

CardNameFixFlowController* CardNameFixFlowViewBridge::GetController() {
  return controller_;
}

void CardNameFixFlowViewBridge::OnConfirmedName(
    const base::string16& confirmed_name) {
  controller_->OnNameAccepted(confirmed_name);
  PerformClose();
}

void CardNameFixFlowViewBridge::OnDismissed() {
  controller_->OnDismissed();
  PerformClose();
}

void CardNameFixFlowViewBridge::PerformClose() {
  base::WeakPtr<CardNameFixFlowViewBridge> weakSelf =
      weak_ptr_factory_.GetWeakPtr();
  [view_controller_ dismissViewControllerAnimated:YES
                                       completion:^{
                                         weakSelf->DeleteSelf();
                                       }];
}

void CardNameFixFlowViewBridge::DeleteSelf() {
  delete this;
}

}  // namespace autofill

@interface CardNameFixFlowViewController () <UITextFieldDelegate,
                                             UITableViewDelegate> {
  UIBarButtonItem* _confirmButton;
  NSString* _confirmedName;
  TableViewTextHeaderFooterView* _footerView;

  // Owns |self|.
  autofill::CardNameFixFlowViewBridge* _bridge;  // weak
}

@end

@implementation CardNameFixFlowViewController

- (instancetype)initWithBridge:(autofill::CardNameFixFlowViewBridge*)bridge {
  DCHECK(bridge);

  if (self = [super initWithStyle:UITableViewStyleGrouped]) {
    _bridge = bridge;
    self.title =
        base::SysUTF16ToNSString(_bridge->GetController()->GetTitleText());
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.delegate = self;

  autofill::CardNameFixFlowController* controller = _bridge->GetController();

  NSString* cancelButtonLabel =
      base::SysUTF16ToNSString(controller->GetCancelButtonLabel());
  self.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithTitle:cancelButtonLabel
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onCancel:)];
  NSString* saveButtonLabel =
      base::SysUTF16ToNSString(controller->GetSaveButtonLabel());
  _confirmButton =
      [[UIBarButtonItem alloc] initWithTitle:saveButtonLabel
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onConfirm:)];
  self.navigationItem.rightBarButtonItem = _confirmButton;

  _confirmedName =
      base::SysUTF16ToNSString(controller->GetInferredCardholderName());

  [self updateSaveButtonEnabledStatus];

  NSString* inferredNameTooltipText =
      (_confirmedName.length > 0)
          ? base::SysUTF16ToNSString(controller->GetInferredNameTooltipText())
          : nil;

  _footerView = [[TableViewTextHeaderFooterView alloc] init];
  _footerView.subtitleLabel.textColor =
      [UIColor colorNamed:kTextSecondaryColor];
  _footerView.subtitleLabel.text = inferredNameTooltipText;

  [self.tableView registerClass:[TableViewTextEditCell class]
         forCellReuseIdentifier:kCellReuseID];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  DCHECK(section == kMainSection);
  return kNumberOfRowsInMainSection;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath.section == kMainSection);

  UITableViewCell* cellAtIndex =
      [self.tableView dequeueReusableCellWithIdentifier:kCellReuseID
                                           forIndexPath:indexPath];

  DCHECK([cellAtIndex isKindOfClass:[TableViewTextEditCell class]]);
  autofill::CardNameFixFlowController* controller = _bridge->GetController();

  TableViewTextEditCell* confirmNameCell = (TableViewTextEditCell*)cellAtIndex;
  confirmNameCell.selectionStyle = UITableViewCellSelectionStyleNone;
  confirmNameCell.useCustomSeparator = NO;
  confirmNameCell.textLabel.text =
      base::SysUTF16ToNSString(controller->GetInputLabel());

  UITextField* textField = confirmNameCell.textField;
  textField.placeholder =
      base::SysUTF16ToNSString(controller->GetInputPlaceholderText());

  textField.text = _confirmedName;
  textField.textColor = [UIColor colorNamed:kBlueColor];
  textField.delegate = self;

  return confirmNameCell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  DCHECK(section == kMainSection);

  return _footerView;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  [self didChangeConfirmedName:[textField.text
                                   stringByReplacingCharactersInRange:range
                                                           withString:string]];
  return YES;
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  [self didChangeConfirmedName:@""];
  return YES;
}

#pragma mark - Private

- (void)onCancel:(id)sender {
  _bridge->OnDismissed();
}

- (void)onConfirm:(id)sender {
  DCHECK(_confirmedName.length > 0);
  _bridge->OnConfirmedName(base::SysNSStringToUTF16(_confirmedName));
}

- (void)updateSaveButtonEnabledStatus {
  _confirmButton.enabled = _confirmedName.length > 0;
}

- (void)didChangeConfirmedName:(NSString*)confirmedName {
  confirmedName = [confirmedName
      stringByTrimmingCharactersInSet:NSCharacterSet
                                          .whitespaceAndNewlineCharacterSet];

  // After an edit, refresh footer text to no longer include name tooltip.
  _footerView.subtitleLabel.text = nil;

  _confirmedName = confirmedName;
  [self updateSaveButtonEnabledStatus];
}

@end
