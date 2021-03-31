// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeNameHonorificPrefix = kItemTypeEnumZero,
  ItemTypeNameFull,
  ItemTypeCompanyName,
  ItemTypeAddressHomeLine1,
  ItemTypeAddressHomeLine2,
  ItemTypeAddressHomeCity,
  ItemTypeAddressHomeState,
  ItemTypeAddressHomeZip,
  ItemTypeAddressHomeCountry,
  ItemTypePhoneHomeWholeNumber,
  ItemTypeEmailAddress,
  ItemTypeAddressProfileSave
};

@interface InfobarSaveAddressProfileTableViewController () <UITextFieldDelegate>

// InfobarSaveAddressProfileModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarSaveAddressProfileModalDelegate>
    saveAddressProfileModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Item for displaying and editing the name.
@property(nonatomic, copy) NSString* name;
// Item for displaying and editing the address line 1.
@property(nonatomic, copy) NSString* addressline1;
// Item for displaying and editing the address line 2.
@property(nonatomic, copy) NSString* addressline2;
// Item for displaying and editing the city.
@property(nonatomic, copy) NSString* city;
// Item for displaying and editing the state.
@property(nonatomic, copy) NSString* state;
// Item for displaying and editing the country.
@property(nonatomic, copy) NSString* country;
// Item for displaying and editing the zip code.
@property(nonatomic, copy) NSString* zipCode;
// Item for displaying and editing the phone number.
@property(nonatomic, copy) NSString* phoneNumber;
// Item for displaying and editing the email address.
@property(nonatomic, copy) NSString* emailAddress;
// YES if the Address Profile being displayed has been saved.
@property(nonatomic, assign) BOOL currentAddressProfileSaved;
// Item for displaying the save address profile button.
@property(nonatomic, strong)
    TableViewTextButtonItem* saveAddressProfileButtonItem;

@end

@implementation InfobarSaveAddressProfileTableViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _saveAddressProfileModalDelegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveAutofillAddressProfile];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  // TODO(crbug.com/1167062): Update UI when mocks are ready.
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* nameItem = [self textEditItemWithType:ItemTypeNameFull
                                                 textFieldName:@""
                                                textFieldValue:self.name
                                              textFieldEnabled:NO];
  [model addItem:nameItem toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* addressLine1Item =
      [self textEditItemWithType:ItemTypeAddressHomeLine1
                   textFieldName:@""
                  textFieldValue:self.addressline1
                textFieldEnabled:NO];
  [model addItem:addressLine1Item
      toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* addressLine2Item =
      [self textEditItemWithType:ItemTypeAddressHomeLine2
                   textFieldName:@""
                  textFieldValue:self.addressline2
                textFieldEnabled:NO];
  [model addItem:addressLine2Item
      toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* cityItem =
      [self textEditItemWithType:ItemTypeAddressHomeCity
                   textFieldName:@""
                  textFieldValue:self.city
                textFieldEnabled:NO];
  [model addItem:cityItem toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* stateItem =
      [self textEditItemWithType:ItemTypeAddressHomeState
                   textFieldName:@""
                  textFieldValue:self.state
                textFieldEnabled:NO];
  [model addItem:stateItem toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* countryItem =
      [self textEditItemWithType:ItemTypeAddressHomeCountry
                   textFieldName:@""
                  textFieldValue:self.country
                textFieldEnabled:NO];
  [model addItem:countryItem toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* zipItem =
      [self textEditItemWithType:ItemTypeAddressHomeZip
                   textFieldName:@""
                  textFieldValue:self.zipCode
                textFieldEnabled:NO];
  [model addItem:zipItem toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* phoneItem =
      [self textEditItemWithType:ItemTypePhoneHomeWholeNumber
                   textFieldName:@""
                  textFieldValue:self.phoneNumber
                textFieldEnabled:NO];
  [model addItem:phoneItem toSectionWithIdentifier:SectionIdentifierFields];

  TableViewTextEditItem* emailItem =
      [self textEditItemWithType:ItemTypeEmailAddress
                   textFieldName:@""
                  textFieldValue:self.emailAddress
                textFieldEnabled:NO];
  [model addItem:emailItem toSectionWithIdentifier:SectionIdentifierFields];

  self.saveAddressProfileButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeAddressProfileSave];
  self.saveAddressProfileButtonItem.textAlignment = NSTextAlignmentNatural;
  self.saveAddressProfileButtonItem.buttonText = @"Save";
  self.saveAddressProfileButtonItem.enabled = !self.currentAddressProfileSaved;
  self.saveAddressProfileButtonItem.disableButtonIntrinsicWidth = YES;
  [model addItem:self.saveAddressProfileButtonItem
      toSectionWithIdentifier:SectionIdentifierFields];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeAddressProfileSave) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self
                  action:@selector(saveAddressProfileButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - InfobarSaveAddressProfileModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.name = prefs[kNamePrefKey];
  self.addressline1 = prefs[kAddressLine1PrefKey];
  self.addressline2 = prefs[kAddressLine2PrefKey];
  self.city = prefs[kCityPrefKey];
  self.state = prefs[kStatePrefKey];
  self.country = prefs[kCountryPrefKey];
  self.zipCode = prefs[kZipPrefKey];
  self.phoneNumber = prefs[kPhonePrefKey];
  self.emailAddress = prefs[kEmailPrefKey];
  self.currentAddressProfileSaved =
      [prefs[kCurrentAddressProfileSavedPrefKey] boolValue];
  [self.tableView reloadData];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - Private Methods

- (void)saveAddressProfileButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self.saveAddressProfileModalDelegate modalInfobarButtonWasAccepted:self];
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.saveAddressProfileModalDelegate dismissInfobarModal:self];
}

#pragma mark - Helpers

- (TableViewTextEditItem*)textEditItemWithType:(ItemType)type
                                 textFieldName:(NSString*)name
                                textFieldValue:(NSString*)value
                              textFieldEnabled:(BOOL)enabled {
  TableViewTextEditItem* textEditItem =
      [[TableViewTextEditItem alloc] initWithType:type];
  textEditItem.textFieldName = name;
  textEditItem.textFieldValue = value;
  textEditItem.textFieldEnabled = enabled;
  textEditItem.hideIcon = !enabled;
  textEditItem.returnKeyType = UIReturnKeyDone;

  return textEditItem;
}

@end
