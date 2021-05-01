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
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
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
  ItemTypeAddress = kItemTypeEnumZero,
  ItemTypePhoneNumber,
  ItemTypeEmailAddress,
  ItemTypeAddressProfileSave,
};

@interface InfobarSaveAddressProfileTableViewController () <UITextFieldDelegate>

// InfobarSaveAddressProfileModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarSaveAddressProfileModalDelegate>
    saveAddressProfileModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Item for displaying and editing the address.
@property(nonatomic, copy) NSString* address;
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
  UIImage* settingsImage = [[UIImage imageNamed:@"infobar_settings_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIBarButtonItem* settingsButton = [[UIBarButtonItem alloc]
      initWithImage:settingsImage
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(presentAddressProfileSettings)];
  // TODO(crbug.com/1167062): Replace with proper localized string.
  settingsButton.accessibilityLabel = @"Access profile settings";
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = settingsButton;
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

  // TODO(crbug.com/1167062): Add image icons for the fields.
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierFields];

  TableViewImageItem* addressImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeAddress];
  addressImageItem.title = self.address;
  [model addItem:addressImageItem
      toSectionWithIdentifier:SectionIdentifierFields];

  TableViewImageItem* emailImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeEmailAddress];
  emailImageItem.title = self.emailAddress;
  [model addItem:emailImageItem
      toSectionWithIdentifier:SectionIdentifierFields];

  TableViewImageItem* phoneImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypePhoneNumber];
  phoneImageItem.title = self.phoneNumber;
  [model addItem:phoneImageItem
      toSectionWithIdentifier:SectionIdentifierFields];

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
  } else if (itemType == ItemTypeAddress) {
    TableViewImageCell* managedcell =
        base::mac::ObjCCastStrict<TableViewImageCell>(cell);
    managedcell.textLabel.numberOfLines =
        [[self.address componentsSeparatedByString:@"\n"] count];
  }
  return cell;
}

#pragma mark - InfobarSaveAddressProfileModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.address = prefs[kAddressPrefKey];
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

- (void)presentAddressProfileSettings {
  base::RecordAction(base::UserMetricsAction("MobileMessagesModalSettings"));
  [self.saveAddressProfileModalDelegate presentAddressProfileSettings];
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
