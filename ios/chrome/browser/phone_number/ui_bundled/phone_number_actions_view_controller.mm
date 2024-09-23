// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/phone_number/ui_bundled/phone_number_actions_view_controller.h"

#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/phone_number/ui_bundled/phone_number_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPhoneNumberActions = kSectionIdentifierEnumZero,
};

// This is used by TableViewModel to differentiate and identify phone number
// actions.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeActionCall = kItemTypeEnumZero,
  ItemTypeActionMessage,
  ItemTypeActionContacts,
  ItemTypeActionFacetime,

};

}  // namespace

@interface PhoneNumberActionsViewController ()

@end

@implementation PhoneNumberActionsViewController {
  // Unformatted phone number.
  NSString* _phoneNumber;

  // Formatted phone number with the call prefix.
  NSString* _phoneNumberCallFormat;

  // Formatted phone number with the message prefix.
  NSString* _phoneNumberMessageFormat;

  // Formatted phone number with the facetime prefix.
  NSString* _phoneNumberFacetimeFormat;

  // Cancel button item in navigation bar.
  UIBarButtonItem* _cancelButton;

  // Displayed unformatted phone number title.
  NSString* _phoneNumberTitle;
}

- (instancetype)initWithPhoneNumber:(NSString*)phoneNumber {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];

  if (self) {
    _phoneNumber = [phoneNumber stringByReplacingOccurrencesOfString:@" "
                                                          withString:@""];
    _phoneNumberCallFormat =
        [NSString stringWithFormat:@"tel:%@", _phoneNumber];
    _phoneNumberMessageFormat =
        [NSString stringWithFormat:@"sms:%@", _phoneNumber];
    _phoneNumberFacetimeFormat =
        [NSString stringWithFormat:@"facetime://%@", _phoneNumber];
    _phoneNumberTitle = phoneNumber;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = _phoneNumberTitle;
  _cancelButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_CANCEL)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(cancelButtonTapped:)];
  self.navigationItem.rightBarButtonItem = _cancelButton;
  self.tableView.accessibilityIdentifier = kPhoneNumberActionsViewIdentifier;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPhoneNumberActions];
  TableViewTextItem* callButton =
      [[TableViewTextItem alloc] initWithType:ItemTypeActionCall];
  TableViewTextItem* messageButton =
      [[TableViewTextItem alloc] initWithType:ItemTypeActionMessage];
  TableViewTextItem* addToContactsbutton =
      [[TableViewTextItem alloc] initWithType:ItemTypeActionContacts];
  TableViewTextItem* facetimeButton =
      [[TableViewTextItem alloc] initWithType:ItemTypeActionFacetime];

  callButton.text = l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_CALL);
  callButton.textColor = [UIColor colorNamed:kBlueColor];
  callButton.accessibilityTraits = UIAccessibilityTraitButton;

  messageButton.text =
      l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_SEND_MESSAGE);
  messageButton.textColor = [UIColor colorNamed:kBlueColor];
  messageButton.accessibilityTraits = UIAccessibilityTraitButton;

  addToContactsbutton.text =
      l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_ADD_TO_CONTACTS);
  addToContactsbutton.textColor = [UIColor colorNamed:kBlueColor];
  addToContactsbutton.accessibilityTraits = UIAccessibilityTraitButton;

  facetimeButton.text = l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_FACETIME);
  facetimeButton.textColor = [UIColor colorNamed:kBlueColor];
  facetimeButton.accessibilityTraits = UIAccessibilityTraitButton;

  [model addItem:callButton
      toSectionWithIdentifier:SectionIdentifierPhoneNumberActions];
  [model addItem:messageButton
      toSectionWithIdentifier:SectionIdentifierPhoneNumberActions];
  [model addItem:addToContactsbutton
      toSectionWithIdentifier:SectionIdentifierPhoneNumberActions];
  [model addItem:facetimeButton
      toSectionWithIdentifier:SectionIdentifierPhoneNumberActions];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
  switch (item.type) {
    case ItemTypeActionCall: {
      [[UIApplication sharedApplication]
                    openURL:[NSURL URLWithString:_phoneNumberCallFormat]
                    options:@{}
          completionHandler:nil];
      break;
    }
    case ItemTypeActionMessage: {
      [[UIApplication sharedApplication]
                    openURL:[NSURL URLWithString:_phoneNumberMessageFormat]
                    options:@{}
          completionHandler:nil];
      break;
    }
    case ItemTypeActionContacts: {
      [self.addContactsHandler presentAddContactsForPhoneNumber:_phoneNumber];
      break;
    }
    case ItemTypeActionFacetime: {
      [[UIApplication sharedApplication]
                    openURL:[NSURL URLWithString:_phoneNumberFacetimeFormat]
                    options:@{}
          completionHandler:nil];
      break;
    }
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
}

- (void)reportBackUserAction {
}

#pragma mark - Helper methods

// Action when the cancel is tapped.
- (void)cancelButtonTapped:(UIBarButtonItem*)item {
  [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

@end
