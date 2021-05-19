// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
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

namespace {
// Height of the space used by header/footer when none is set. Default is
// |estimatedSection{Header|Footer}Height|.
const CGFloat kDefaultHeaderFooterHeight = 10;

}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSaveModalFields = kSectionIdentifierEnumZero,
  SectionIdentifierUpdateModalNewFields,
  SectionIdentifierUpdateModalOldFields,
  SectionIdentifierUpdateButton,
  SectionIdentifierUpdateDescription
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAddress = kItemTypeEnumZero,
  ItemTypePhoneNumber,
  ItemTypeEmailAddress,
  ItemTypeAddressProfileSaveUpdateButton,
  ItemTypeUpdateNew,
  ItemTypeUpdateOld,
  ItemTypeHeader,
  ItemTypeFooter
};

@interface InfobarSaveAddressProfileTableViewController ()

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
// Yes, if the update address profile modal is to be displayed.
@property(nonatomic, assign) BOOL isUpdateModal;
// Contains the content for the update modal.
@property(nonatomic, copy) NSDictionary* profileDataDiff;
// Description of the update modal.
@property(nonatomic, copy) NSString* updateModalDescription;

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
  self.styler.tableViewBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;

  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInsetWithIcon, 0, 0);

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = cancelButton;

  if (!self.currentAddressProfileSaved) {
    UIBarButtonItem* editButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemEdit
                             target:self
                             action:@selector(showEditAddressProfileModal)];
    // TODO(crbug.com/1167062): Add accessibility identifier for the edit
    // button.
    self.navigationItem.rightBarButtonItem = editButton;
  }

  self.navigationController.navigationBar.prefersLargeTitles = NO;

  // TODO(crbug.com/1167062): Replace with proper localized string.
  if (self.isUpdateModal) {
    self.navigationItem.title = @"Update Address";
  } else {
    self.navigationItem.title = @"Save Address";
  }

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

  if (self.isUpdateModal) {
    [self loadUpdateAddressModal];
  } else {
    [self loadSaveAddressModal];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeAddressProfileSaveUpdateButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self
                  action:@selector(saveAddressProfileButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeAddress || itemType == ItemTypeUpdateOld) {
    TableViewImageCell* managedcell =
        base::mac::ObjCCastStrict<TableViewImageCell>(cell);
    managedcell.textLabel.numberOfLines = 0;
    managedcell.imageView.image = [managedcell.imageView.image
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [managedcell.imageView setTintColor:[UIColor colorNamed:kGrey400Color]];
  } else if (itemType == ItemTypePhoneNumber ||
             itemType == ItemTypeEmailAddress) {
    TableViewImageCell* managedcell =
        base::mac::ObjCCastStrict<TableViewImageCell>(cell);
    managedcell.imageView.image = [managedcell.imageView.image
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [managedcell.imageView setTintColor:[UIColor colorNamed:kGrey400Color]];
  } else if (itemType == ItemTypeUpdateNew) {
    TableViewImageCell* managedcell =
        base::mac::ObjCCastStrict<TableViewImageCell>(cell);
    managedcell.textLabel.numberOfLines = 0;
    managedcell.imageView.image = [managedcell.imageView.image
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    // Color is blue.
    [managedcell.imageView setTintColor:[UIColor colorNamed:kBlueColor]];
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
  self.isUpdateModal = [prefs[kIsUpdateModalPrefKey] boolValue];
  self.profileDataDiff = prefs[kProfileDataDiffKey];
  self.updateModalDescription = prefs[kUpdateModalDescriptionKey];
  [self.tableView reloadData];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.tableViewModel headerForSection:section])
    return UITableViewAutomaticDimension;
  return kDefaultHeaderFooterHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel footerForSection:section])
    return UITableViewAutomaticDimension;
  return kDefaultHeaderFooterHeight;
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

- (void)showEditAddressProfileModal {
  [self.saveAddressProfileModalDelegate showEditView];
}

- (void)loadUpdateAddressModal {
  DCHECK([self.profileDataDiff count] > 0);

  // Determines whether the old section is to be shown or not.
  BOOL showOld = NO;
  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][1] length] > 0) {
      showOld = YES;
      break;
    }
  }

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierUpdateDescription];
  [model setFooter:[self updateModalDescriptionFooter]
      forSectionWithIdentifier:SectionIdentifierUpdateDescription];

  // New
  [model addSectionWithIdentifier:SectionIdentifierUpdateModalNewFields];

  if (showOld) {
    // TODO(crbug.com/1167062): Use i18n strings.
    [model setHeader:[self updateHeaderWithText:@"New"]
        forSectionWithIdentifier:SectionIdentifierUpdateModalNewFields];
  }
  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][0] length] > 0) {
      [model addItem:[self detailItemWithType:ItemTypeUpdateNew
                                         text:self.profileDataDiff[type][0]
                                iconImageName:
                                    [self iconForAutofillInputTypeNumber:type]]
          toSectionWithIdentifier:SectionIdentifierUpdateModalNewFields];
    }
  }

  if (showOld) {
    // Old
    [model addSectionWithIdentifier:SectionIdentifierUpdateModalOldFields];

    // TODO(crbug.com/1167062): Use i18n strings.
    [model setHeader:[self updateHeaderWithText:@"Old"]
        forSectionWithIdentifier:SectionIdentifierUpdateModalOldFields];
    for (NSNumber* type in self.profileDataDiff) {
      if ([self.profileDataDiff[type][1] length] > 0) {
        [model addItem:[self
                           detailItemWithType:ItemTypeUpdateOld
                                         text:self.profileDataDiff[type][1]
                                iconImageName:
                                    [self iconForAutofillInputTypeNumber:type]]
            toSectionWithIdentifier:SectionIdentifierUpdateModalOldFields];
      }
    }
  }

  [model addSectionWithIdentifier:SectionIdentifierUpdateButton];
  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierUpdateButton];
}

- (void)loadSaveAddressModal {
  // TODO(crbug.com/1167062): Add image icons for the fields.
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSaveModalFields];

  [model addItem:[self
                     detailItemWithType:ItemTypeAddress
                                   text:self.address
                          iconImageName:
                              [self iconForAutofillUIType:
                                        AutofillUITypeProfileHomeAddressStreet]]
      toSectionWithIdentifier:SectionIdentifierSaveModalFields];

  if ([self.emailAddress length]) {
    [model addItem:[self detailItemWithType:ItemTypeEmailAddress
                                       text:self.emailAddress
                              iconImageName:
                                  [self iconForAutofillUIType:
                                            AutofillUITypeProfileEmailAddress]]
        toSectionWithIdentifier:SectionIdentifierSaveModalFields];
  }
  if ([self.phoneNumber length]) {
    [model addItem:
               [self
                   detailItemWithType:ItemTypePhoneNumber
                                 text:self.phoneNumber
                        iconImageName:
                            [self
                                iconForAutofillUIType:
                                    AutofillUITypeProfileHomePhoneWholeNumber]]
        toSectionWithIdentifier:SectionIdentifierSaveModalFields];
  }

  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierSaveModalFields];
}

- (TableViewTextButtonItem*)saveUpdateButton {
  TableViewTextButtonItem* saveUpdateButton = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeAddressProfileSaveUpdateButton];
  saveUpdateButton.textAlignment = NSTextAlignmentNatural;

  // TODO(crbug.com/1167062): Use i18n strings.
  if (self.isUpdateModal) {
    saveUpdateButton.buttonText = @"Update";
  } else {
    saveUpdateButton.buttonText = @"Save";
  }

  saveUpdateButton.enabled = !self.currentAddressProfileSaved;
  saveUpdateButton.disableButtonIntrinsicWidth = YES;
  return saveUpdateButton;
}

- (TableViewTextHeaderFooterItem*)updateHeaderWithText:(NSString*)text {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = text;
  return header;
}

- (TableViewHeaderFooterItem*)updateModalDescriptionFooter {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = self.updateModalDescription;
  return footer;
}

- (NSString*)iconForAutofillUIType:(AutofillUIType)type {
  switch (type) {
    case AutofillUITypeNameFullWithHonorificPrefix:
      return @"infobar_profile_icon";
    case AutofillUITypeAddressHomeAddress:
    case AutofillUITypeProfileHomeAddressStreet:
      return @"infobar_autofill_address_icon";
    case AutofillUITypeProfileEmailAddress:
      return @"infobar_email_icon";
    case AutofillUITypeProfileHomePhoneWholeNumber:
      return @"infobar_phone_icon";
    default:
      NOTREACHED();
      return @"";
  }
}

- (NSString*)iconForAutofillInputTypeNumber:(NSNumber*)val {
  return [self iconForAutofillUIType:(AutofillUIType)[val intValue]];
}

#pragma mark Item Constructors

- (TableViewImageItem*)detailItemWithType:(NSInteger)type
                                     text:(NSString*)text
                            iconImageName:(NSString*)iconImageName {
  TableViewImageItem* detailItem =
      [[TableViewImageItem alloc] initWithType:type];
  detailItem.title = text;
  detailItem.enabled = NO;
  detailItem.useCustomSeparator = YES;
  if ([iconImageName length]) {
    detailItem.image = [UIImage imageNamed:iconImageName];
  }

  return detailItem;
}

@end
