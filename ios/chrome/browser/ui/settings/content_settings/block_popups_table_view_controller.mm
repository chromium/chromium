// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/block_popups_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMainSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierExceptions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMainSwitch = kItemTypeEnumZero,
  ItemTypeManaged,
  ItemTypeHeader,
  ItemTypeException,
  ItemTypeExceptionByPolicy,
};

}  // namespace

@interface BlockPopupsTableViewController () <
    BooleanObserver,
    PopoverLabelViewControllerDelegate> {
  raw_ptr<ProfileIOS> _profile;  // weak

  // List of url patterns that are allowed to display popups.
  base::Value::List _exceptions;

  // List of url patterns set by policy that are allowed to display popups.
  base::Value::List _allowPopupsByPolicy;

  // The observable boolean that binds to the "Disable Popups" setting state.
  ContentSettingBackedBoolean* _disablePopupsSetting;

  // The item related to the switch for the "Disable Popups" setting.
  TableViewSwitchItem* _blockPopupsItem;

  // The managed item for the "Disable Popups" setting.
  TableViewInfoButtonItem* _blockPopupsManagedItem;
}

@end

@implementation BlockPopupsTableViewController

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _profile = profile;
    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForProfile(_profile);
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

  if (_profile->GetPrefs()->IsManagedPreference(
          prefs::kManagedDefaultPopupsSetting)) {
    _blockPopupsManagedItem = [self blockPopupsManagedItem];
    [model addItem:_blockPopupsManagedItem
        toSectionWithIdentifier:SectionIdentifierMainSwitch];
  } else {
    _blockPopupsItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeMainSwitch];
    _blockPopupsItem.text = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
    _blockPopupsItem.on = [_disablePopupsSetting value];
    _blockPopupsItem.accessibilityIdentifier = @"blockPopupsContentView_switch";
    [model addItem:_blockPopupsItem
        toSectionWithIdentifier:SectionIdentifierMainSwitch];
  }

  if ([self popupsCurrentlyBlocked] &&
      (_exceptions.size() || _allowPopupsByPolicy.size())) {
    [self populateExceptionsItems];
  }
}

- (BOOL)shouldShowEditButton {
  return [self popupsCurrentlyBlocked];
}

- (BOOL)editButtonEnabled {
  return _exceptions.size() > 0;
}

// Override.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Do not call super as this is also delete the section if it is empty.
  [self deleteItemAtIndexPaths:indexPaths];
}

#pragma mark - LoadModel Helpers

- (TableViewInfoButtonItem*)blockPopupsManagedItem {
  TableViewInfoButtonItem* blockPopupsManagedItem =
      [[TableViewInfoButtonItem alloc] initWithType:ItemTypeManaged];
  blockPopupsManagedItem.text = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  blockPopupsManagedItem.statusText =
      [_disablePopupsSetting value]
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  blockPopupsManagedItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  blockPopupsManagedItem.accessibilityIdentifier =
      @"blockPopupsContentView_managed";
  return blockPopupsManagedItem;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeHeader:
    case ItemTypeException:
      break;
    case ItemTypeMainSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(blockPopupsSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeManaged: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only when items are in SectionIdentifierExceptions and are not set by the
  // policy are editable.
  return
      [self.tableViewModel
          sectionIdentifierForSectionIndex:indexPath.section] ==
          SectionIdentifierExceptions &&
      [self.tableViewModel itemAtIndexPath:indexPath].type == ItemTypeException;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;
  [self deleteItemAtIndexPaths:@[ indexPath ]];
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierExceptions] ||
      !_exceptions.size()) {
    self.navigationItem.rightBarButtonItem.enabled = NO;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _disablePopupsSetting);
  if (_blockPopupsItem) {
    if (_blockPopupsItem.on == [_disablePopupsSetting value])
      return;

    // Update the item.
    _blockPopupsItem.on = [_disablePopupsSetting value];

    // Update the cell.
    [self reconfigureCellsForItems:@[ _blockPopupsItem ]];
  } else {
    _blockPopupsManagedItem.statusText =
        [_disablePopupsSetting value]
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }
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

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

#pragma mark - Private

// Deletes the item at the `indexPaths`. Removes the SectionIdentifierExceptions
// if it is now empty.
- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  NSSortDescriptor* sortDescriptor =
      [[NSSortDescriptor alloc] initWithKey:@"item" ascending:NO];
  indexPaths = [indexPaths sortedArrayUsingDescriptors:@[ sortDescriptor ]];

  for (NSIndexPath* indexPath in indexPaths) {
    size_t urlIndex = indexPath.item;
    std::string urlToRemove;
    if (urlIndex < _exceptions.size() && _exceptions[urlIndex].is_string()) {
      urlToRemove = _exceptions[urlIndex].GetString();
    }

    // Remove the exception for the site by resetting its popup setting to the
    // default.
    ios::HostContentSettingsMapFactory::GetForProfile(_profile)
        ->SetContentSettingCustomScope(
            ContentSettingsPattern::FromString(urlToRemove),
            ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS,
            CONTENT_SETTING_DEFAULT);

    // Remove the site from `_exceptions`.
    _exceptions.erase(_exceptions.begin() + urlIndex);
  }
  [self.tableView
      performBatchUpdates:^{
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
          [self.tableView
              deleteRowsAtIndexPaths:indexPaths
                    withRowAnimation:UITableViewRowAnimationAutomatic];
        }
      }
               completion:nil];
}

// Returns YES if popups are currently blocked by default, NO otherwise.
- (BOOL)popupsCurrentlyBlocked {
  return [_disablePopupsSetting value];
}

// Fetch the urls that can display popups and
// add items set by the user to `_exceptions`,
// add items set by the policy to `_allowPopupsByPolicy`.
- (void)populateExceptionsList {
  // The body of this method was mostly copied from
  // chrome/browser/ui/webui/options/content_settings_handler.cc and simplified
  // to only deal with urls/patterns that allow popups.
  ContentSettingsForOneType entries =
      ios::HostContentSettingsMapFactory::GetForProfile(_profile)
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);
  for (size_t i = 0; i < entries.size(); ++i) {
    // Skip default settings from extensions and policy, and the default content
    // settings; all of them will affect the default setting UI.
    if (entries[i].primary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].source != content_settings::ProviderType::kPrefProvider) {
      continue;
    }
    // The content settings UI does not support secondary content settings
    // pattern yet. For content settings set through the content settings UI the
    // secondary pattern is by default a wildcard pattern. Hence users are not
    // able to modify content settings with a secondary pattern other than the
    // wildcard pattern. So only show settings that the user is able to modify.
    if (entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].GetContentSetting() == CONTENT_SETTING_ALLOW) {
      if (entries[i].source ==
          content_settings::ProviderType::kPolicyProvider) {
        // Add the urls to `_allowPopupsByPolicy` if the allowed urls are set by
        // the policy.
        _allowPopupsByPolicy.Append(entries[i].primary_pattern.ToString());
      } else {
        _exceptions.Append(entries[i].primary_pattern.ToString());
      }
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

  // Populate the exception items set by the user.
  for (const base::Value& exception : _exceptions) {
    std::string allowed_url;
    if (exception.is_string())
      allowed_url = exception.GetString();
    TableViewDetailTextItem* item =
        [[TableViewDetailTextItem alloc] initWithType:ItemTypeException];
    item.text = base::SysUTF8ToNSString(allowed_url);
    [model addItem:item toSectionWithIdentifier:SectionIdentifierExceptions];
  }

  // Populate the allowed popup items set by the policy.
  for (const base::Value& l : _allowPopupsByPolicy) {
    std::string allowed_url_by_policy;
    if (l.is_string())
      allowed_url_by_policy = l.GetString();
    TableViewDetailTextItem* item = [[TableViewDetailTextItem alloc]
        initWithType:ItemTypeExceptionByPolicy];
    item.text = base::SysUTF8ToNSString(allowed_url_by_policy);
    [model addItem:item toSectionWithIdentifier:SectionIdentifierExceptions];
  }
}

- (void)layoutSections:(BOOL)blockPopupsIsOn {
  BOOL hasExceptions = _exceptions.size() || _allowPopupsByPolicy.size();
  BOOL exceptionsListShown = [self.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierExceptions];

  if (blockPopupsIsOn && !exceptionsListShown && hasExceptions) {
    // Animate in the list of exceptions. Animation looks much better if the
    // section is added at once, rather than row-by-row as each object is added.
    __weak BlockPopupsTableViewController* weakSelf = self;
    [self.tableView
        performBatchUpdates:^{
          BlockPopupsTableViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;
          [strongSelf populateExceptionsItems];
          NSUInteger index = [[strongSelf tableViewModel]
              sectionForSectionIdentifier:SectionIdentifierExceptions];
          [strongSelf.tableView
                insertSections:[NSIndexSet indexSetWithIndex:index]
              withRowAnimation:UITableViewRowAnimationNone];
        }
                 completion:nil];
  } else if (!blockPopupsIsOn && exceptionsListShown) {
    // Make sure the exception section is not shown.
    __weak BlockPopupsTableViewController* weakSelf = self;
    [self.tableView
        performBatchUpdates:^{
          BlockPopupsTableViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;
          NSUInteger index = [[strongSelf tableViewModel]
              sectionForSectionIdentifier:SectionIdentifierExceptions];
          [[strongSelf tableViewModel]
              removeSectionWithIdentifier:SectionIdentifierExceptions];
          [strongSelf.tableView
                deleteSections:[NSIndexSet indexSetWithIndex:index]
              withRowAnimation:UITableViewRowAnimationNone];
        }
                 completion:nil];
  }
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

@end
