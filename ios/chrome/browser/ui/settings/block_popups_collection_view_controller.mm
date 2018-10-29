// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/block_popups_collection_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMainSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierExceptions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMainSwitch = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeException,
};

}  // namespace

@interface BlockPopupsCollectionViewController ()<BooleanObserver> {
  ios::ChromeBrowserState* _browserState;  // weak

  // List of url patterns that are allowed to display popups.
  base::ListValue _exceptions;

  // The observable boolean that binds to the "Disable Popups" setting state.
  ContentSettingBackedBoolean* _disablePopupsSetting;

  // The item related to the switch for the "Disable Popups" setting.
  LegacySettingsSwitchItem* _blockPopupsItem;
}

// Fetch the urls that can display popups and add them to |_exceptions|.
- (void)populateExceptionsList;

// Returns YES if popups are currently blocked by default, NO otherwise.
- (BOOL)popupsCurrentlyBlocked;

@end

@implementation BlockPopupsCollectionViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _browserState = browserState;
    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState);
    _disablePopupsSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:CONTENT_SETTINGS_TYPE_POPUPS
                              inverted:YES];
    [_disablePopupsSetting setObserver:self];
    self.title = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
    self.collectionViewAccessibilityIdentifier =
        @"block_popups_settings_view_controller";

    // TODO(crbug.com/764578): Instance methods should not be called from
    // initializer.
    [self populateExceptionsList];
    [self updateEditButton];
    [self loadModel];
  }
  return self;
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];

  CollectionViewModel* model = self.collectionViewModel;

  // Block popups switch.
  [model addSectionWithIdentifier:SectionIdentifierMainSwitch];

  _blockPopupsItem =
      [[LegacySettingsSwitchItem alloc] initWithType:ItemTypeMainSwitch];
  _blockPopupsItem.text = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  _blockPopupsItem.on = [_disablePopupsSetting value];
  _blockPopupsItem.accessibilityIdentifier = @"blockPopupsContentView_switch";
  [model addItem:_blockPopupsItem
      toSectionWithIdentifier:SectionIdentifierMainSwitch];

  if ([self popupsCurrentlyBlocked] && _exceptions.GetSize()) {
    [self populateExceptionsItems];
  }
}

- (BOOL)shouldShowEditButton {
  return [self popupsCurrentlyBlocked];
}

- (BOOL)editButtonEnabled {
  return _exceptions.GetSize() > 0;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  return YES;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeMainSwitch) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(blockPopupsSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Any item in SectionIdentifierExceptions is editable.
  return [self.collectionViewModel
             sectionIdentifierForSection:indexPath.section] ==
         SectionIdentifierExceptions;
}

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  for (NSIndexPath* indexPath in indexPaths) {
    size_t urlIndex = indexPath.item;
    std::string urlToRemove;
    _exceptions.GetString(urlIndex, &urlToRemove);

    // Remove the exception for the site by resetting its popup setting to the
    // default.
    ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState)
        ->SetContentSettingCustomScope(
            ContentSettingsPattern::FromString(urlToRemove),
            ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_POPUPS,
            std::string(), CONTENT_SETTING_DEFAULT);

    // Remove the site from |_exceptions|.
    _exceptions.Remove(urlIndex, NULL);
  }

  // Update the edit button appearance, in case all exceptions were removed.
  [self updateEditButton];

  // Must call super at the end of the child implementation.
  [super collectionView:collectionView willDeleteItemsAtIndexPaths:indexPaths];
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // The only editable section is the block popups exceptions section.
  if ([self.collectionViewModel
          hasSectionForSectionIdentifier:SectionIdentifierExceptions]) {
    NSInteger exceptionsSectionIndex = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierExceptions];
    if ([collectionView numberOfItemsInSection:exceptionsSectionIndex] == 0) {
      __weak BlockPopupsCollectionViewController* weakSelf = self;
      [self.collectionView performBatchUpdates:^{
        BlockPopupsCollectionViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        NSInteger section = [strongSelf.collectionViewModel
            sectionForSectionIdentifier:SectionIdentifierExceptions];
        [strongSelf.collectionViewModel
            removeSectionWithIdentifier:SectionIdentifierExceptions];
        [strongSelf.collectionView
            deleteSections:[NSIndexSet indexSetWithIndex:section]];
      }
                                    completion:nil];
    }
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeMainSwitch:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _disablePopupsSetting);

  // Update the item.
  _blockPopupsItem.on = [_disablePopupsSetting value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ _blockPopupsItem ]];

  // Update the rest of the UI.
  [self.editor setEditing:NO];
  [self updateEditButton];
  [self layoutSections:[_disablePopupsSetting value]];
}

#pragma mark - Actions

- (void)blockPopupsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [_disablePopupsSetting setValue:switchView.on];

  // Update the item.
  _blockPopupsItem.on = [_disablePopupsSetting value];

  // Update the rest of the UI.
  [self.editor setEditing:NO];
  [self updateEditButton];
  [self layoutSections:switchView.on];
}

#pragma mark - Private

- (BOOL)popupsCurrentlyBlocked {
  return [_disablePopupsSetting value];
}

- (void)populateExceptionsList {
  // The body of this method was mostly copied from
  // chrome/browser/ui/webui/options/content_settings_handler.cc and simplified
  // to only deal with urls/patterns that allow popups.
  ContentSettingsForOneType entries;
  ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState)
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
                              &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    // Skip default settings from extensions and policy, and the default content
    // settings; all of them will affect the default setting UI.
    if (entries[i].primary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].source != "preference") {
      continue;
    }
    // The content settings UI does not support secondary content settings
    // pattern yet. For content settings set through the content settings UI the
    // secondary pattern is by default a wildcard pattern. Hence users are not
    // able to modify content settings with a secondary pattern other than the
    // wildcard pattern. So only show settings that the user is able to modify.
    if (entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].GetContentSetting() == CONTENT_SETTING_ALLOW) {
      _exceptions.AppendString(entries[i].primary_pattern.ToString());
    } else {
      LOG(ERROR) << "Secondary content settings patterns are not "
                 << "supported by the content settings UI";
    }
  }
}

- (void)populateExceptionsItems {
  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:SectionIdentifierExceptions];

  SettingsTextItem* header =
      [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_POPUPS_ALLOWED);
  header.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:header forSectionWithIdentifier:SectionIdentifierExceptions];

  for (size_t i = 0; i < _exceptions.GetSize(); ++i) {
    std::string allowed_url;
    _exceptions.GetString(i, &allowed_url);
    SettingsTextItem* item =
        [[SettingsTextItem alloc] initWithType:ItemTypeException];
    item.text = base::SysUTF8ToNSString(allowed_url);
    [model addItem:item toSectionWithIdentifier:SectionIdentifierExceptions];
  }
}

- (void)layoutSections:(BOOL)blockPopupsIsOn {
  BOOL hasExceptions = _exceptions.GetSize();
  BOOL exceptionsListShown = [self.collectionViewModel
      hasSectionForSectionIdentifier:SectionIdentifierExceptions];

  if (blockPopupsIsOn && !exceptionsListShown && hasExceptions) {
    // Animate in the list of exceptions. Animation looks much better if the
    // section is added at once, rather than row-by-row as each object is added.
    __weak BlockPopupsCollectionViewController* weakSelf = self;
    [self.collectionView performBatchUpdates:^{
      BlockPopupsCollectionViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      [strongSelf populateExceptionsItems];
      NSUInteger index = [[strongSelf collectionViewModel]
          sectionForSectionIdentifier:SectionIdentifierExceptions];
      [[strongSelf collectionView]
          insertSections:[NSIndexSet indexSetWithIndex:index]];
    }
                                  completion:nil];
  } else if (!blockPopupsIsOn && exceptionsListShown) {
    // Make sure the exception section is not shown.
    __weak BlockPopupsCollectionViewController* weakSelf = self;
    [self.collectionView performBatchUpdates:^{
      BlockPopupsCollectionViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      NSUInteger index = [[strongSelf collectionViewModel]
          sectionForSectionIdentifier:SectionIdentifierExceptions];
      [[strongSelf collectionViewModel]
          removeSectionWithIdentifier:SectionIdentifierExceptions];
      [[strongSelf collectionView]
          deleteSections:[NSIndexSet indexSetWithIndex:index]];
    }
                                  completion:nil];
  }
}

@end
