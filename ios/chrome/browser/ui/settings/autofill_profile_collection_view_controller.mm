// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill_profile_collection_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/autofill_profile_edit_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/autofill_data_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierSubtitle,
  SectionIdentifierProfiles,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAutofillAddressSwitch = kItemTypeEnumZero,
  ItemTypeAutofillAddressSwitchSubtitle,
  ItemTypeAddress,
  ItemTypeHeader,
};

}  // namespace

#pragma mark - AutofillProfileCollectionViewController

@interface AutofillProfileCollectionViewController ()<
    PersonalDataManagerObserver> {
  autofill::PersonalDataManager* _personalDataManager;

  ios::ChromeBrowserState* _browserState;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;

  // Deleting profiles updates PersonalDataManager resulting in an observer
  // callback, which handles general data updates with a reloadData.
  // It is better to handle user-initiated changes with more specific actions
  // such as inserting or removing items/sections. This boolean is used to
  // stop the observer callback from acting on user-initiated changes.
  BOOL _deletionInProgress;
}

@property(nonatomic, getter=isAutofillProfileEnabled)
    BOOL autofillProfileEnabled;

@end

@implementation AutofillProfileCollectionViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    self.collectionViewAccessibilityIdentifier = @"kAutofillCollectionViewId";
    self.title = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
    self.shouldHideDoneButton = YES;
    _browserState = browserState;
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(_browserState);
    _observer.reset(new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_observer.get());

    // TODO(crbug.com/764578): -updateEditButton and -loadModel should not be
    // called from initializer.
    [self updateEditButton];
    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  _personalDataManager->RemoveObserver(_observer.get());
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];
  [model addItem:[self addressSwitchItem]
      toSectionWithIdentifier:SectionIdentifierSwitches];

  [model addSectionWithIdentifier:SectionIdentifierSubtitle];
  [model addItem:[self addressSwitchSubtitleItem]
      toSectionWithIdentifier:SectionIdentifierSubtitle];

  [self populateProfileSection];
}

#pragma mark - LoadModel Helpers

// Populates profile section using personalDataManager.
- (void)populateProfileSection {
  CollectionViewModel* model = self.collectionViewModel;
  const std::vector<autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->GetProfiles();
  if (!autofillProfiles.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierProfiles];
    [model setHeader:[self profileSectionHeader]
        forSectionWithIdentifier:SectionIdentifierProfiles];
    for (autofill::AutofillProfile* autofillProfile : autofillProfiles) {
      DCHECK(autofillProfile);
      [model addItem:[self itemForProfile:*autofillProfile]
          toSectionWithIdentifier:SectionIdentifierProfiles];
    }
  }
}

- (CollectionViewItem*)addressSwitchItem {
  LegacySettingsSwitchItem* switchItem = [[LegacySettingsSwitchItem alloc]
      initWithType:ItemTypeAutofillAddressSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL);
  switchItem.on = [self isAutofillProfileEnabled];
  switchItem.accessibilityIdentifier = @"addressItem_switch";
  return switchItem;
}

- (CollectionViewItem*)addressSwitchSubtitleItem {
  CollectionViewTextItem* textItem = [[CollectionViewTextItem alloc]
      initWithType:ItemTypeAutofillAddressSwitchSubtitle];
  textItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL);
  textItem.textFont = [UIFont systemFontOfSize:kUIKitMultilineDetailFontSize];
  textItem.textColor = UIColorFromRGB(kUIKitMultilineDetailTextColor);
  textItem.numberOfTextLines = 0;
  return textItem;
}

- (CollectionViewItem*)profileSectionHeader {
  SettingsTextItem* header = [self genericHeader];
  header.text = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES);
  return header;
}

- (SettingsTextItem*)genericHeader {
  SettingsTextItem* header =
      [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
  header.textColor = [[MDCPalette greyPalette] tint500];
  return header;
}

- (CollectionViewItem*)itemForProfile:
    (const autofill::AutofillProfile&)autofillProfile {
  std::string guid(autofillProfile.guid());
  NSString* title = base::SysUTF16ToNSString(
      autofillProfile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                              GetApplicationContext()->GetApplicationLocale()));
  NSString* subTitle = base::SysUTF16ToNSString(autofillProfile.GetInfo(
      autofill::AutofillType(autofill::ADDRESS_HOME_LINE1),
      GetApplicationContext()->GetApplicationLocale()));
  bool isServerProfile = autofillProfile.record_type() ==
                         autofill::AutofillProfile::SERVER_PROFILE;

  AutofillDataItem* item =
      [[AutofillDataItem alloc] initWithType:ItemTypeAddress];
  item.text = title;
  item.leadingDetailText = subTitle;
  item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  item.accessibilityIdentifier = title;
  item.GUID = guid;
  item.deletable = !isServerProfile;
  if (isServerProfile) {
    item.trailingDetailText =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_SERVER_NAME);
  }
  return item;
}

- (BOOL)localProfilesExist {
  return !_personalDataManager->GetProfiles().empty();
}

#pragma mark - SettingsRootCollectionViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  return [self localProfilesExist];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.collectionViewModel itemTypeForIndexPath:indexPath]);
  if (itemType == ItemTypeAutofillAddressSwitch) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(autofillAddressSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

#pragma mark - Switch Callbacks

- (void)autofillAddressSwitchChanged:(UISwitch*)switchView {
  [self setSwitchItemOn:[switchView isOn]
               itemType:ItemTypeAutofillAddressSwitch];
  [self setAutofillProfileEnabled:[switchView isOn]];
}

#pragma mark - Switch Helpers

// Sets switchItem's state to |on|. It is important that there is only one item
// of |switchItemType| in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.collectionViewModel indexPathForItemType:switchItemType
                                   sectionIdentifier:SectionIdentifierSwitches];
  LegacySettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<LegacySettingsSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
}

// Sets switchItem's enabled status to |enabled| and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// |switchItemType| in SectionIdentifierSwitches.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  CollectionViewModel* model = self.collectionViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:SectionIdentifierSwitches]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:switchItemType
                sectionIdentifier:SectionIdentifierSwitches];
  LegacySettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<LegacySettingsSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                         forItem:item];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  return sectionIdentifier == SectionIdentifierSwitches ||
         sectionIdentifier == SectionIdentifierSubtitle;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  return sectionIdentifier == SectionIdentifierSubtitle;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  // Edit mode is the state where the user can select and delete entries. In
  // edit mode, selection is handled by the superclass. When not in edit mode
  // selection presents the editing controller for the selected entry.
  if ([self.editor isEditing]) {
    return;
  }

  CollectionViewModel* model = self.collectionViewModel;
  if ([model itemTypeForIndexPath:indexPath] != ItemTypeAddress)
    return;

  const std::vector<autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->GetProfiles();
  AutofillProfileEditCollectionViewController* controller =
      [AutofillProfileEditCollectionViewController
          controllerWithProfile:*autofillProfiles[indexPath.item]
            personalDataManager:_personalDataManager];
  controller.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  return YES;
}

- (void)collectionViewWillBeginEditing:(UICollectionView*)collectionView {
  [super collectionViewWillBeginEditing:collectionView];

  [self setSwitchItemEnabled:NO itemType:ItemTypeAutofillAddressSwitch];
}

- (void)collectionViewWillEndEditing:(UICollectionView*)collectionView {
  [super collectionViewWillEndEditing:collectionView];

  [self setSwitchItemEnabled:YES itemType:ItemTypeAutofillAddressSwitch];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Only autofill data cells are editable.
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[AutofillDataItem class]]) {
    AutofillDataItem* autofillItem =
        base::mac::ObjCCastStrict<AutofillDataItem>(item);
    return [autofillItem isDeletable];
  }
  return NO;
}

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  _deletionInProgress = YES;
  for (NSIndexPath* indexPath in indexPaths) {
    AutofillDataItem* item = base::mac::ObjCCastStrict<AutofillDataItem>(
        [self.collectionViewModel itemAtIndexPath:indexPath]);
    _personalDataManager->RemoveByGUID([item GUID]);
  }
  // Must call super at the end of the child implementation.
  [super collectionView:collectionView willDeleteItemsAtIndexPaths:indexPaths];
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // If there are no index paths, return early. This can happen if the user
  // presses the Delete button twice in quick succession.
  if (![indexPaths count])
    return;

  // TODO(crbug.com/650390) Generalize removing empty sections
  [self removeSectionIfEmptyForSectionWithIdentifier:SectionIdentifierProfiles];
}

// Remove the section from the model and collectionView if there are no more
// items in the section.
- (void)removeSectionIfEmptyForSectionWithIdentifier:
    (SectionIdentifier)sectionIdentifier {
  if (![self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    return;
  }
  NSInteger section =
      [self.collectionViewModel sectionForSectionIdentifier:sectionIdentifier];
  if ([self.collectionView numberOfItemsInSection:section] == 0) {
    // Avoid reference cycle in block.
    __weak AutofillProfileCollectionViewController* weakSelf = self;
    [self.collectionView performBatchUpdates:^{
      // Obtain strong reference again.
      AutofillProfileCollectionViewController* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }

      // Remove section from model and collectionView.
      [[strongSelf collectionViewModel]
          removeSectionWithIdentifier:sectionIdentifier];
      [[strongSelf collectionView]
          deleteSections:[NSIndexSet indexSetWithIndex:section]];
    }
        completion:^(BOOL finished) {
          // Obtain strong reference again.
          AutofillProfileCollectionViewController* strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }

          // Turn off edit mode if there is nothing to edit.
          if (![strongSelf localProfilesExist] &&
              [strongSelf.editor isEditing]) {
            [[strongSelf editor] setEditing:NO];
          }
          [strongSelf updateEditButton];
          strongSelf->_deletionInProgress = NO;
        }];
  }
}

#pragma mark PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  if (_deletionInProgress)
    return;

  if (![self localProfilesExist] && [self.editor isEditing]) {
    // Turn off edit mode if there exists nothing to edit.
    [self.editor setEditing:NO];
  }

  [self updateEditButton];
  [self reloadData];
}

#pragma mark - Getters and Setter

- (BOOL)isAutofillProfileEnabled {
  return autofill::prefs::IsProfileAutofillEnabled(_browserState->GetPrefs());
}

- (void)setAutofillProfileEnabled:(BOOL)isEnabled {
  return autofill::prefs::SetProfileAutofillEnabled(_browserState->GetPrefs(),
                                                    isEnabled);
}

@end
