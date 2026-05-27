// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_date_picker_input_view.h"
#import "ui/base/device_form_factor.h"

namespace {

// Width and height of the date picker presented inside a popover.
const CGFloat kDatePickerPopoverWidth = 320.0;
const CGFloat kDatePickerPopoverHeight = 290.0;

// The ratio of the date picker popover anchor width relative to the full
// width of the focused text field (one quarter).
const CGFloat kDatePickerPopoverAnchorWidthRatio = 0.25;

}  // namespace

@implementation AutofillAIEntityEditDateItem

@synthesize attributeType = _attributeType;
@synthesize hasValidValueStatus = _hasValidValueStatus;
@dynamic delegate;

- (void)setEditingEnabled:(BOOL)editingEnabled {
  _editingEnabled = editingEnabled;
  self.textFieldEnabled = editingEnabled;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewTextEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if (self.editingEnabled &&
      (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET)) {
    cell.textField.inputView = [self createCustomInputView];
  } else {
    cell.textField.inputView = nil;
  }
}

#pragma mark - AutofillAIEntityFieldItem

- (void)setHasValidValueStatus:(BOOL)hasValidValueStatus {
  _hasValidValueStatus = hasValidValueStatus;
  [self setHasValidText:hasValidValueStatus];
}

- (UIViewController*)createCustomInputPopoverWithSourceView:
    (UIView*)sourceView {
  UIViewController* popoverContentController =
      [self createCustomInputPopoverView];

  UIPopoverPresentationController* popover =
      popoverContentController.popoverPresentationController;
  popover.sourceView = sourceView;

  CGRect sourceRect = sourceView.bounds;
  CGFloat anchorWidth =
      CGRectGetWidth(sourceView.bounds) * kDatePickerPopoverAnchorWidthRatio;
  BOOL isRTL = [sourceView effectiveUserInterfaceLayoutDirection] ==
               UIUserInterfaceLayoutDirectionRightToLeft;
  if (isRTL) {
    sourceRect.origin.x = CGRectGetMinX(sourceView.bounds);
  } else {
    sourceRect.origin.x = CGRectGetMaxX(sourceView.bounds) - anchorWidth;
  }
  sourceRect.size.width = anchorWidth;

  popover.sourceRect = sourceRect;
  popover.permittedArrowDirections = UIPopoverArrowDirectionAny;

  return popoverContentController;
}

#pragma mark - Private

// Creates the custom input view containing a navigation bar and a date picker.
- (UIView*)createCustomInputView {
  return [[AutofillAIDatePickerInputView alloc]
           initWithDate:self.dateValue
                  title:self.fieldNameLabelText
                 target:self
             dateAction:@selector(dateChanged:)
      clearButtonAction:@selector(clearDate)
       doneButtonAction:@selector(dismissPicker)];
}

// Creates the custom date picker input view contained in a popover view.
- (UIViewController*)createCustomInputPopoverView {
  UIViewController* popoverContentController = [[UIViewController alloc] init];
  popoverContentController.view = [self createCustomInputView];
  popoverContentController.preferredContentSize =
      CGSizeMake(kDatePickerPopoverWidth, kDatePickerPopoverHeight);
  popoverContentController.modalPresentationStyle = UIModalPresentationPopover;
  return popoverContentController;
}

// Handles the value changed event for the date picker.
- (void)dateChanged:(UIDatePicker*)sender {
  [self.delegate didChangeDate:sender.date forItem:self];
}

// Handles the tap on the Clear button to clear the date.
- (void)clearDate {
  [self.delegate didChangeDate:nil forItem:self];
}

// Handles the tap on the Done button to dismiss the picker.
- (void)dismissPicker {
  [self.delegate didDismissDateItem:self];
}

@end
