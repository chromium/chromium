// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Size of the accessory view symbol.
const CGFloat kAccessorySymbolSize = 22;

}  // namespace

@interface FamilyPickerViewController () <UITableViewDataSource,
                                          UITableViewDelegate> {
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
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PICKER_SUBTITLE);
  self.subtitleTextStyle = UIFontTextStyleFootnote;
  [super viewDidLoad];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_recipients[indexPath.row].isEligible) {
    [tableView cellForRowAtIndexPath:indexPath].accessoryView =
        [[UIImageView alloc] initWithImage:[self checkmarkCircleIcon]];
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_recipients[indexPath.row].isEligible) {
    [tableView cellForRowAtIndexPath:indexPath].accessoryView =
        [[UIImageView alloc] initWithImage:[self circleIcon]];
  }
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
  if (_recipients[indexPath.row].isEligible) {
    cell.accessoryView = [[UIImageView alloc] initWithImage:[self circleIcon]];
  } else {
    UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoLight];
    [infoButton setImage:[self infoCircleIcon] forState:UIControlStateNormal];
    [infoButton addTarget:self
                   action:@selector(infoButtonTapped:)
         forControlEvents:UIControlEventTouchUpInside];
    cell.accessoryView = infoButton;
  }

  return cell;
}

#pragma mark - Private

- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  tableView.accessibilityIdentifier = @"FamilyPickerBottomSheetViewId";
  tableView.allowsMultipleSelection = YES;
  [tableView registerClass:SettingsImageDetailTextCell.class
      forCellReuseIdentifier:@"cell"];

  _heightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:[self tableViewEstimatedRowHeight] *
                                _recipients.count];
  _heightConstraint.active = YES;

  return tableView;
}

- (UIImage*)checkmarkCircleIcon {
  return DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                    kAccessorySymbolSize);
}

- (UIImage*)circleIcon {
  return DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize);
}

- (UIImage*)infoCircleIcon {
  return DefaultSymbolWithPointSize(kInfoCircleSymbol, kAccessorySymbolSize);
}

- (void)infoButtonTapped:(UIButton*)button {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_MEMBER_INELIGIBLE);

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    // TODO(crbug.com/1463882): Add HC article link once it's ready.
    NSLinkAttributeName : @"",
  };

  PopoverLabelViewController* popoverViewController =
      [[PopoverLabelViewController alloc]
          initWithPrimaryAttributedString:AttributedStringFromStringWithLink(
                                              text, textAttributes,
                                              linkAttributes)
                secondaryAttributedString:nil];

  popoverViewController.popoverPresentationController.sourceView = button;
  popoverViewController.popoverPresentationController.sourceRect =
      button.bounds;
  popoverViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:popoverViewController
                     animated:YES
                   completion:nil];
}

@end
