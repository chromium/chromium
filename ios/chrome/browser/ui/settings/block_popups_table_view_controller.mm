// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/block_popups_table_view_controller.h"

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
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
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

@interface BlockPopupsTableViewController ()<BooleanObserver> {
  ios::ChromeBrowserState* _browserState;  // weak

  // List of url patterns that are allowed to display popups.
  base::ListValue _exceptions;

  // The observable boolean that binds to the "Disable Popups" setting state.
  ContentSettingBackedBoolean* _disablePopupsSetting;

  // The item related to the switch for the "Disable Popups" setting.
  SettingsSwitchItem* _blockPopupsItem;
}

@end

@implementation BlockPopupsTableViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _browserState = browserState;
    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState);
    _disablePopupsSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::POPUPS
                              inverted:YES];
    [_disablePopupsSetting setObserver:self];
    self.title = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      @"block_popups_settings_view_controller";

  [self populateExceptionsList];
  [self updateUIForEditState];
  [self loadModel];
  self.tableView.allowsSelection = NO;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Block popups switch.
  [model addSectionWithIdentifier:SectionIdentifierMainSwitch];

  _blockPopupsItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeMainSwitch];
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

// Override.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Do not call super as this is also delete the section if it is empty.
  [self deleteItemAtIndexPaths:indexPaths];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeMainSwitch) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(blockPopupsSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Any item in SectionIdentifierExceptions is editable.
  return [self.tableViewModel sectionIdentifierForSection:indexPath.section] ==
         SectionIdentifierExceptions;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;
  [self deleteItemAtIndexPaths:@[ indexPath ]];
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierExceptions]) {
    self.navigationItem.rightBarButtonItem.enabled = NO;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _disablePopupsSetting);

  if (_blockPopupsItem.on == [_disablePopupsSetting value])
    return;

  // Update the item.
  _blockPopupsItem.on = [_disablePopupsSetting value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ _blockPopupsItem ]];

  // Update the rest of the UI.
  [self setEditing:NO animated:YES];
  [self updateUIForEditState];
  [self layoutSections:[_disablePopupsSetting value]];
}

#pragma mark - Actions

- (void)blockPopupsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [_disablePopupsSetting setValue:switchView.on];

  // Update the item.
  _blockPopupsItem.on = [_disablePopupsSetting value];

  // Update the rest of the UI.
  [self setEditing:NO animated:YES];
  [self updateUIForEditState];
  [self layoutSections:switchView.on];
}

#pragma mark - Private

// Deletes the item at the |indexPaths|. Removes the SectionIdentifierExceptions
// if it is now empty.
- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  for (NSIndexPath* indexPath in indexPaths) {
    size_t urlIndex = indexPath.item;
    std::string urlToRemove;
    _exceptions.GetString(urlIndex, &urlToRemove);

    // Remove the exception for the site by resetting its popup setting to the
    // default.
    ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState)
        ->SetContentSettingCustomScope(
            ContentSettingsPattern::FromString(urlToRemove),
            ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS,
            std::string(), CONTENT_SETTING_DEFAULT);

    // Remove the site from |_exceptions|.
    _exceptions.Remove(urlIndex, NULL);
  }
  [self.tableView performBatchUpdates:^{
    NSInteger exceptionsSection = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierExceptions];
    NSUInteger numberOfExceptions =
        [self.tableViewModel numberOfItemsInSection:exceptionsSection];
    if (indexPaths.count == numberOfExceptions) {
      [self.tableViewModel
          removeSectionWithIdentifier:SectionIdentifierExceptions];
      [self.tableView
            deleteSections:[NSIndexSet indexSetWithIndex:exceptionsSection]
          withRowAnimation:UITableViewRowAnimationAutomatic];
    } else {
      [self removeFromModelItemAtIndexPaths:indexPaths];
      [self.tableView deleteRowsAtIndexPaths:indexPaths
                            withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  }
                           completion:nil];
}

// Returns YES if popups are currently blocked by default, NO otherwise.
- (BOOL)popupsCurrentlyBlocked {
  return [_disablePopupsSetting value];
}

// Fetch the urls that can display popups and add them to |_exceptions|.
- (void)populateExceptionsList {
  // The body of this method was mostly copied from
  // chrome/browser/ui/webui/options/content_settings_handler.cc and simplified
  // to only deal with urls/patterns that allow popups.
  ContentSettingsForOneType entries;
  ios::HostContentSettingsMapFactory::GetForBrowserState(_browserState)
      ->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
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
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierExceptions];

  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_POPUPS_ALLOWED);
  [model setHeader:header forSectionWithIdentifier:SectionIdentifierExceptions];

  for (size_t i = 0; i < _exceptions.GetSize(); ++i) {
    std::string allowed_url;
    _exceptions.GetString(i, &allowed_url);
    TableViewDetailTextItem* item =
        [[TableViewDetailTextItem alloc] initWithType:ItemTypeException];
    item.text = base::SysUTF8ToNSString(allowed_url);
    [model addItem:item toSectionWithIdentifier:SectionIdentifierExceptions];
  }
}

- (void)layoutSections:(BOOL)blockPopupsIsOn {
  BOOL hasExceptions = _exceptions.GetSize();
  BOOL exceptionsListShown = [self.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierExceptions];

  if (blockPopupsIsOn && !exceptionsListShown && hasExceptions) {
    // Animate in the list of exceptions. Animation looks much better if the
    // section is added at once, rather than row-by-row as each object is added.
    __weak BlockPopupsTableViewController* weakSelf = self;
    [self.tableView performBatchUpdates:^{
      BlockPopupsTableViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      [strongSelf populateExceptionsItems];
      NSUInteger index = [[strongSelf tableViewModel]
          sectionForSectionIdentifier:SectionIdentifierExceptions];
      [strongSelf.tableView insertSections:[NSIndexSet indexSetWithIndex:index]
                          withRowAnimation:UITableViewRowAnimationNone];
    }
                             completion:nil];
  } else if (!blockPopupsIsOn && exceptionsListShown) {
    // Make sure the exception section is not shown.
    __weak BlockPopupsTableViewController* weakSelf = self;
    [self.tableView performBatchUpdates:^{
      BlockPopupsTableViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      NSUInteger index = [[strongSelf tableViewModel]
          sectionForSectionIdentifier:SectionIdentifierExceptions];
      [[strongSelf tableViewModel]
          removeSectionWithIdentifier:SectionIdentifierExceptions];
      [strongSelf.tableView deleteSections:[NSIndexSet indexSetWithIndex:index]
                          withRowAnimation:UITableViewRowAnimationNone];
    }
                             completion:nil];
  }
}

@end
