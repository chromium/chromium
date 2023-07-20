// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FamilyPickerViewController () <UITableViewDataSource> {
  // Height constraint for the bottom sheet.
  NSLayoutConstraint* _heightConstraint;

  // List of password sharing recipients that the user can pick from.
  NSArray<RecipientInfo*>* _recipients;
}

@end

@implementation FamilyPickerViewController

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _recipients.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)theTableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  SettingsImageDetailTextCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell"];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  cell.textLabel.text = _recipients[indexPath.row].fullName;
  cell.detailTextLabel.text = _recipients[indexPath.row].email;
  // TODO(crbug.com/1463882): Replace with the actual image of the recipient.
  cell.image = DefaultSymbolTemplateWithPointSize(
      kPersonCropCircleSymbol, kAccountProfilePhotoDimension);

  cell.userInteractionEnabled = YES;

  return cell;
}

#pragma mark - Private

- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  tableView.accessibilityIdentifier = @"FamilyPickerBottomSheetViewId";
  [tableView registerClass:SettingsImageDetailTextCell.class
      forCellReuseIdentifier:@"cell"];

  _heightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:[self tableViewEstimatedRowHeight] *
                                _recipients.count];
  _heightConstraint.active = YES;

  return tableView;
}

@end
