// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"

#include "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_table_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewPasswordViewController () <UITableViewDataSource>
@end

@implementation NewPasswordViewController

- (instancetype)init {
  UITableViewStyle style = UITableViewStyleInsetGrouped;
  self = [super initWithStyle:style];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIColor* backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.view.backgroundColor = backgroundColor;
  self.navigationController.navigationBar.translucent = NO;
  self.navigationController.navigationBar.backgroundColor = backgroundColor;

  self.title =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_TITLE",
                        @"Title for add new password screen");
  self.navigationItem.leftBarButtonItem = [self navigationCancelButton];
  self.navigationItem.rightBarButtonItem = [self navigationSaveButton];

  [self.tableView registerClass:[NewPasswordTableCell class]
         forCellReuseIdentifier:NewPasswordTableCell.reuseID];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 44.0;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return NewPasswordTableCellTypeNumRows;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  NewPasswordTableCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NewPasswordTableCell.reuseID];

  NewPasswordTableCellType cellType;
  switch (indexPath.row) {
    case NewPasswordTableCellTypeUsername:
      cellType = NewPasswordTableCellTypeUsername;
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case NewPasswordTableCellTypePassword:
      cellType = NewPasswordTableCellTypePassword;
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case NewPasswordTableCellTypeSuggestStrongPassword:
      cellType = NewPasswordTableCellTypeSuggestStrongPassword;
      cell.selectionStyle = UITableViewCellSelectionStyleDefault;
      cell.accessibilityTraits |= UIAccessibilityTraitButton;
      break;
    default:
      NOTREACHED();
      cellType = NewPasswordTableCellTypeSuggestStrongPassword;
      break;
  }

  [cell setCellType:cellType];

  return cell;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForFooterInSection:(NSInteger)section {
  NSString* baseLocalizedString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_FOOTER",
      @"Disclaimer telling users what will happen to their passwords");
  return [baseLocalizedString
      stringByReplacingOccurrencesOfString:@"$1"
                                withString:@"example@google.com"];
}

#pragma mark - UITableViewDelegate

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  switch (indexPath.row) {
    case NewPasswordTableCellTypeUsername:
    case NewPasswordTableCellTypePassword:
      return nil;
    case NewPasswordTableCellTypeSuggestStrongPassword:
      return indexPath;
    default:
      return indexPath;
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // There is no need to check which cell has been selected because all the
  // other cells are unselectable from |-tableView:willSelectRowAtIndexPath:|.
  NSIndexPath* passwordIndexPath =
      [NSIndexPath indexPathForRow:NewPasswordTableCellTypePassword
                         inSection:0];
  NewPasswordTableCell* passwordCell =
      [tableView cellForRowAtIndexPath:passwordIndexPath];

  // TODO(crbug.com/1224986): Generate password and fill it in.
  passwordCell.textField.text = @"";

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Private

// Action for cancel button.
- (void)cancelButtonWasPressed {
  [self.delegate
      navigationCancelButtonWasPressedInNewPasswordViewController:self];
}

// Returns a new cancel button for the navigation bar.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonWasPressed)];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  return cancelButton;
}

// Returns a new save button for the navigation bar.
- (UIBarButtonItem*)navigationSaveButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                           target:nil
                           action:nil];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  return cancelButton;
}

@end
