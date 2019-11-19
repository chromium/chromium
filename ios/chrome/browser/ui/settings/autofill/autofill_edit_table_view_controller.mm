// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller+protected.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillEditTableViewController () <FormInputAccessoryViewDelegate> {
  TableViewTextEditCell* _currentEditingCell;
}

// The accessory view when editing any of text fields.
@property(nonatomic, strong) FormInputAccessoryView* formInputAccessoryView;

@end

@implementation AutofillEditTableViewController

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle {
  self = [super initWithTableViewStyle:style appBarStyle:appBarStyle];
  if (!self) {
    return nil;
  }

  _formInputAccessoryView = [[FormInputAccessoryView alloc] init];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.formInputAccessoryView setUpWithLeadingView:nil
                                 navigationDelegate:self];
  [self setShouldHideDoneButton:YES];
  [self updateUIForEditState];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidShow)
             name:UIKeyboardDidShowNotification
           object:nil];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIKeyboardDidShowNotification
              object:nil];
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  return YES;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !self.tableView.editing;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  TableViewTextEditCell* cell = [self autofillEditCellForTextField:textField];
  _currentEditingCell = cell;
  [textField setInputAccessoryView:self.formInputAccessoryView];
  [self updateAccessoryViewButtonState];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  TableViewTextEditCell* cell = [self autofillEditCellForTextField:textField];
  DCHECK(_currentEditingCell == cell);
  [textField setInputAccessoryView:nil];
  _currentEditingCell = nil;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK([_currentEditingCell textField] == textField);
  [self moveToAnotherCellWithOffset:1];
  return NO;
}

#pragma mark - FormInputAccessoryViewDelegate

- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender {
  [self moveToAnotherCellWithOffset:1];
}

- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender {
  [self moveToAnotherCellWithOffset:-1];
}

- (void)formInputAccessoryViewDidTapCloseButton:
    (FormInputAccessoryView*)sender {
  [[_currentEditingCell textField] resignFirstResponder];
}

#pragma mark - Helper methods

// Returns the cell containing |textField|.
- (TableViewTextEditCell*)autofillEditCellForTextField:(UITextField*)textField {
  TableViewTextEditCell* settingsCell = nil;
  for (UIView* view = textField; view; view = [view superview]) {
    TableViewTextEditCell* cell =
        base::mac::ObjCCast<TableViewTextEditCell>(view);
    if (cell) {
      settingsCell = cell;
      break;
    }
  }

  // There should be a cell associated with this text field.
  DCHECK(settingsCell);
  return settingsCell;
}

- (NSIndexPath*)indexForCellPathWithOffset:(NSInteger)offset
                                  fromPath:(NSIndexPath*)cellPath {
  if (!cellPath || !offset)
    return nil;

  NSInteger cellSection = [cellPath section];
  NSInteger nextCellRow = [cellPath row] + offset;

  if (nextCellRow >= 0 && nextCellRow < [self.tableView
                                            numberOfRowsInSection:cellSection])
    return [NSIndexPath indexPathForRow:nextCellRow inSection:cellSection];

  NSInteger nextCellSection = cellSection + offset;
  if (nextCellSection >= 0 &&
      nextCellSection < [self.tableView numberOfSections])
    return [NSIndexPath indexPathForRow:0 inSection:nextCellSection];

  return nil;
}

- (NSIndexPath*)indexPathForCurrentTextField {
  DCHECK(_currentEditingCell);
  return [self.tableView indexPathForCell:_currentEditingCell];
}

- (void)moveToAnotherCellWithOffset:(NSInteger)offset {
  NSIndexPath* cellPath = [self indexPathForCurrentTextField];
  DCHECK(cellPath);
  NSIndexPath* nextCellPath = [self indexForCellPathWithOffset:offset
                                                      fromPath:cellPath];

  if (!nextCellPath) {
    [[_currentEditingCell textField] resignFirstResponder];
  } else {
    TableViewTextEditCell* nextCell =
        base::mac::ObjCCastStrict<TableViewTextEditCell>(
            [self.tableView cellForRowAtIndexPath:nextCellPath]);
    [nextCell.textField becomeFirstResponder];
  }
}

- (void)updateAccessoryViewButtonState {
  NSIndexPath* currentPath = [self indexPathForCurrentTextField];
  NSIndexPath* nextPath = [self indexForCellPathWithOffset:1
                                                  fromPath:currentPath];
  NSIndexPath* previousPath = [self indexForCellPathWithOffset:-1
                                                      fromPath:currentPath];

  BOOL isValidPreviousPath =
      previousPath && [[self.tableView cellForRowAtIndexPath:previousPath]
                          isKindOfClass:TableViewTextEditCell.class];
  self.formInputAccessoryView.previousButton.enabled = isValidPreviousPath;

  BOOL isValidNextPath =
      nextPath && [[self.tableView cellForRowAtIndexPath:nextPath]
                      isKindOfClass:TableViewTextEditCell.class];
  self.formInputAccessoryView.nextButton.enabled = isValidNextPath;
}

#pragma mark - Keyboard handling

- (void)keyboardDidShow {
  [self.tableView scrollRectToVisible:[_currentEditingCell frame] animated:YES];
}

@end
