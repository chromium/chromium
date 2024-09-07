// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_expiration_date_fix_flow_view_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/expiration_date_picker.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

CGFloat const kMainSection = 0;
CGFloat const kNumberOfRowsInMainSection = 1;

namespace autofill {

#pragma mark CardExpirationDateFixFlowViewBridge

CardExpirationDateFixFlowViewBridge::CardExpirationDateFixFlowViewBridge(
    CardExpirationDateFixFlowController* controller,
    UIViewController* presenting_view_controller)
    : controller_(controller),
      presenting_view_controller_(presenting_view_controller) {
  DCHECK(controller_);
  DCHECK(presenting_view_controller_);
}

CardExpirationDateFixFlowViewBridge::~CardExpirationDateFixFlowViewBridge() {
  if (controller_)
    controller_->OnDialogClosed();
}

void CardExpirationDateFixFlowViewBridge::Show() {
  // Wrap CardExpirationDateFixFlowViewController with navigation controller to
  // include a navigation bar.
  view_controller_ = [[UINavigationController alloc]
      initWithRootViewController:[[CardExpirationDateFixFlowViewController
                                     alloc] initWithBridge:this]];
  [presenting_view_controller_ presentViewController:view_controller_
                                            animated:YES
                                          completion:nil];
}

void CardExpirationDateFixFlowViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

CardExpirationDateFixFlowController*
CardExpirationDateFixFlowViewBridge::GetController() {
  return controller_;
}

void CardExpirationDateFixFlowViewBridge::OnConfirmedExpirationDate(
    const std::u16string& month,
    const std::u16string& year) {
  controller_->OnAccepted(month, year);
  PerformClose();
}

void CardExpirationDateFixFlowViewBridge::OnDismissed() {
  controller_->OnDismissed();
  PerformClose();
}

void CardExpirationDateFixFlowViewBridge::PerformClose() {
  base::WeakPtr<CardExpirationDateFixFlowViewBridge> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [view_controller_ dismissViewControllerAnimated:YES
                                       completion:^{
                                         if (weak_this) {
                                           weak_this->DeleteSelf();
                                         }
                                       }];
}

void CardExpirationDateFixFlowViewBridge::DeleteSelf() {
  delete this;
}

}  // namespace autofill

@interface CardExpirationDateFixFlowViewController () <UITextFieldDelegate,
                                                       UITableViewDelegate> {
  UIBarButtonItem* _confirmButton;
  NSString* _expirationDateMonth;
  NSString* _expirationDateYear;
  ExpirationDatePicker* _expirationDatePicker;
  TableViewTextEditCell* _confirmExpirationDateCell;
  TableViewTextHeaderFooterView* _footerView;
  raw_ptr<autofill::CardExpirationDateFixFlowViewBridge> _bridge;  // weak
}

@end

@implementation CardExpirationDateFixFlowViewController

- (instancetype)initWithBridge:
    (autofill::CardExpirationDateFixFlowViewBridge*)bridge {
  DCHECK(bridge);

  if ((self = [super initWithStyle:UITableViewStyleGrouped])) {
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

  autofill::CardExpirationDateFixFlowController* controller =
      _bridge->GetController();

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

  _expirationDatePicker =
      [[ExpirationDatePicker alloc] initWithFrame:CGRectZero];
  _expirationDatePicker.backgroundColor = [UIColor clearColor];
  __weak CardExpirationDateFixFlowViewController* weakSelf = self;
  _expirationDatePicker.onDateSelected = ^(NSString* month, NSString* year) {
    CardExpirationDateFixFlowViewController* strongSelf = weakSelf;
    [strongSelf didSelectMonth:month year:year];
  };

  _confirmExpirationDateCell = [[TableViewTextEditCell alloc] init];
  _confirmExpirationDateCell.selectionStyle = UITableViewCellSelectionStyleNone;
  _confirmExpirationDateCell.useCustomSeparator = NO;
  _confirmExpirationDateCell.textLabel.text =
      base::SysUTF16ToNSString(controller->GetInputLabel());

  UITextField* textField = _confirmExpirationDateCell.textField;
  textField.inputView = _expirationDatePicker;
  textField.textColor = [UIColor colorNamed:kBlueColor];
  textField.clearButtonMode = UITextFieldViewModeNever;
  textField.delegate = self;

  _footerView = [[TableViewTextHeaderFooterView alloc] init];

  // Set initial value.
  [self didSelectMonth:_expirationDatePicker.month
                  year:_expirationDatePicker.year];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_confirmExpirationDateCell.textField becomeFirstResponder];
}

#pragma mark - UITableViewDelegate

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  DCHECK(section == kMainSection);

  return base::SysUTF16ToNSString(_bridge->GetController()->GetCardLabel());
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  DCHECK(section == kMainSection);

  return _footerView;
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

  return _confirmExpirationDateCell;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  return NO; /* Prevent any input from outside the date picker. */
}

#pragma mark - Private

- (void)onCancel:(id)sender {
  _bridge->OnDismissed();
}

- (void)onConfirm:(id)sender {
  DCHECK(_expirationDateMonth.length > 0);
  DCHECK(_expirationDateYear.length > 0);

  _bridge->OnConfirmedExpirationDate(
      base::SysNSStringToUTF16(_expirationDateMonth),
      base::SysNSStringToUTF16(_expirationDateYear));
}

- (void)didSelectMonth:(NSString*)month year:(NSString*)year {
  _expirationDateYear = year;
  _expirationDateMonth = month;

  NSString* dateSeparator =
      base::SysUTF16ToNSString(_bridge->GetController()->GetDateSeparator());
  _confirmExpirationDateCell.textField.text =
      [NSString stringWithFormat:@"%@%@%@", month, dateSeparator, year];

  [self updateSaveButtonEnabledStatus];
}

- (void)updateSaveButtonEnabledStatus {
  NSCalendar* calendar = NSCalendar.currentCalendar;
  NSDateComponents* calendarComponents =
      [calendar components:NSCalendarUnitMonth | NSCalendarUnitYear
                  fromDate:[NSDate date]];
  NSInteger currentYear = [calendarComponents year];
  NSInteger currentMonth = [calendarComponents month];

  if ([_expirationDateYear intValue] < currentYear ||
      ([_expirationDateYear intValue] == currentYear &&
       [_expirationDateMonth intValue] < currentMonth)) {
    [_footerView
        setSubtitle:base::SysUTF16ToNSString(
                        _bridge->GetController()->GetInvalidDateError())
          withColor:[UIColor colorNamed:kRedColor]];
    _confirmButton.enabled = NO;
  } else {
    [_footerView setSubtitle:nil];
    _confirmButton.enabled = YES;
  }
}

@end
