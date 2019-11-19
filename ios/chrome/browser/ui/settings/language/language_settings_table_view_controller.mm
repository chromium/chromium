// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_settings_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/language/add_language_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_commands.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_data_source.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_histograms.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_ui_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLanguages = kSectionIdentifierEnumZero,
  SectionIdentifierTranslate,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeLanguage,  // This is a repeating type.
  ItemTypeAddLanguage,
  ItemTypeTranslateSwitch
};

}  // namespace

@interface LanguageSettingsTableViewController () <
    AddLanguageTableViewControllerDelegate,
    LanguageDetailsTableViewControllerDelegate>

// The data source passed to this instance.
@property(nonatomic, strong) id<LanguageSettingsDataSource> dataSource;

// The command handler passed to this instance.
@property(nonatomic, weak) id<LanguageSettingsCommands> commandHandler;

// A reference to the Add language item for quick access.
@property(nonatomic, weak) TableViewTextItem* addLanguageItem;

// A reference to the Translate switch item for quick access.
@property(nonatomic, weak) SettingsSwitchItem* translateSwitchItem;

// A reference to the presented AddLanguageTableViewController, if any.
@property(nonatomic, weak)
    AddLanguageTableViewController* addLanguageTableViewController;

@end

@implementation LanguageSettingsTableViewController

- (instancetype)initWithDataSource:(id<LanguageSettingsDataSource>)dataSource
                    commandHandler:
                        (id<LanguageSettingsCommands>)commandHandler {
  DCHECK(dataSource);
  DCHECK(commandHandler);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _dataSource = dataSource;
    _commandHandler = commandHandler;

    UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsPageImpressionHistogram,
                              LanguageSettingsPages::PAGE_MAIN);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_TITLE);
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier =
      kLanguageSettingsTableViewAccessibilityIdentifier;

  [self loadModel];
  [self updateUIForEditState];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSection];

  // Translate switch item.
  [model addSectionWithIdentifier:SectionIdentifierTranslate];
  SettingsSwitchItem* translateSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeTranslateSwitch];
  self.translateSwitchItem = translateSwitchItem;
  translateSwitchItem.accessibilityIdentifier =
      kTranslateSwitchAccessibilityIdentifier;
  translateSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_TRANSLATE_SWITCH_TITLE);
  translateSwitchItem.detailText = l10n_util::GetNSString(
      IDS_IOS_LANGUAGE_SETTINGS_TRANSLATE_SWITCH_SUBTITLE);
  translateSwitchItem.on = [self.dataSource translateEnabled];
  [model addItem:translateSwitchItem
      toSectionWithIdentifier:SectionIdentifierTranslate];
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  return [self.tableViewModel hasItemForItemType:ItemTypeLanguage
                               sectionIdentifier:SectionIdentifierLanguages];
}

- (void)updateUIForEditState {
  [super updateUIForEditState];

  // The following two lines cause the table view to refresh the cell heights
  // with animation without reloading the cells. This is needed for language
  // cells that can be significantly taller in edit mode.
  [self.tableView beginUpdates];
  [self.tableView endUpdates];

  [self setAddLanguageItemEnabled:!self.isEditing];
  [self setTranslateSwitchItemEnabled:!self.isEditing];
}

#pragma mark - SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  [self.dataSource stopObservingModel];
}

#pragma mark - UITableViewDelegate

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;

  // Only language items are removable.
  TableViewItem* item = [model itemAtIndexPath:indexPath];
  if (item.type != ItemTypeLanguage)
    return UITableViewCellEditingStyleNone;

  // The last Translate-blocked language cannot be deleted.
  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(item);
  return ([languageItem isBlocked] && [self numberOfBlockedLanguages] <= 1)
             ? UITableViewCellEditingStyleNone
             : UITableViewCellEditingStyleDelete;
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (![super tableView:tableView willSelectRowAtIndexPath:indexPath])
    return nil;

  // Ignore selection of language items when Translate is disabled.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return (itemType != ItemTypeLanguage || [self.dataSource translateEnabled])
             ? indexPath
             : nil;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  ItemType itemType =
      (ItemType)[self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeLanguage: {
      LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
      languageItem.canOfferTranslate =
          [self canOfferTranslateForLanguage:languageItem];
      LanguageDetailsTableViewController* viewController =
          [[LanguageDetailsTableViewController alloc]
              initWithLanguageItem:languageItem
                          delegate:self];
      [self.navigationController pushViewController:viewController
                                           animated:YES];
      break;
    }
    case ItemTypeAddLanguage: {
      UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                                LanguageSettingsActions::CLICK_ON_ADD_LANGUAGE);

      AddLanguageTableViewController* viewController =
          [[AddLanguageTableViewController alloc]
              initWithDataSource:self.dataSource
                        delegate:self];
      [self.navigationController pushViewController:viewController
                                           animated:YES];
      self.addLanguageTableViewController = viewController;
      break;
    }
    case ItemTypeHeader:
    case ItemTypeTranslateSwitch:
      // Not handled.
      break;
  }
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    targetIndexPathForMoveFromRowAtIndexPath:(NSIndexPath*)sourceIndexPath
                         toProposedIndexPath:
                             (NSIndexPath*)proposedDestinationIndexPath {
  // Allow language items to move in their own section. Also prevent moving to
  // the last row of the section which is reserved for the add language item.
  NSInteger lastRowIndex =
      [self.tableViewModel numberOfItemsInSection:sourceIndexPath.section] - 1;
  NSInteger lastValidRowIndex = lastRowIndex - 1;
  if (sourceIndexPath.section != proposedDestinationIndexPath.section) {
    NSUInteger rowInSourceSection =
        (sourceIndexPath.section > proposedDestinationIndexPath.section)
            ? 0
            : lastValidRowIndex;
    return [NSIndexPath indexPathForRow:rowInSourceSection
                              inSection:sourceIndexPath.section];
  } else if (proposedDestinationIndexPath.row >= lastRowIndex) {
    return [NSIndexPath indexPathForRow:lastValidRowIndex
                              inSection:sourceIndexPath.section];
  }
  // Allow the proposed destination.
  return proposedDestinationIndexPath;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only language items are editable.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeLanguage;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(editingStyle, UITableViewCellEditingStyleDelete);

  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);

  // Update the model and the table view.
  [self deleteItems:[NSArray arrayWithObject:indexPath]];

  // Inform the command handler.
  [self.commandHandler removeLanguage:languageItem.languageCode];
}

- (BOOL)tableView:(UITableView*)tableView
    canMoveRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only language items can be reordered.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeLanguage;
}

- (void)tableView:(UITableView*)tableView
    moveRowAtIndexPath:(NSIndexPath*)sourceIndexPath
           toIndexPath:(NSIndexPath*)destinationIndexPath {
  if (sourceIndexPath.row == destinationIndexPath.row) {
    return;
  }

  // Update the model.
  TableViewModel* model = self.tableViewModel;
  LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(
      [model itemAtIndexPath:sourceIndexPath]);
  [model removeItemWithType:ItemTypeLanguage
      fromSectionWithIdentifier:SectionIdentifierLanguages
                        atIndex:sourceIndexPath.row];
  [model insertItem:languageItem
      inSectionWithIdentifier:SectionIdentifierLanguages
                      atIndex:destinationIndexPath.row];

  // Inform the command handler.
  BOOL downward = sourceIndexPath.row < destinationIndexPath.row;
  NSUInteger offset = abs(sourceIndexPath.row - destinationIndexPath.row);
  [self.commandHandler moveLanguage:languageItem.languageCode
                           downward:downward
                         withOffset:offset];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType =
      (ItemType)[self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeTranslateSwitch: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(translateSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeHeader:
    case ItemTypeLanguage:
    case ItemTypeAddLanguage:
      // Not handled.
      break;
  }
  return cell;
}

#pragma mark - AddLanguageTableViewControllerDelegate

- (void)addLanguageTableViewController:
            (AddLanguageTableViewController*)tableViewController
                 didSelectLanguageCode:(const std::string&)languageCode {
  // Inform the command handler.
  [self.commandHandler addLanguage:languageCode];

  // Update the model and the table view.
  [self updateLanguagesSection];

  [self.navigationController popViewControllerAnimated:YES];
  self.addLanguageTableViewController = nil;
}

#pragma mark - LanguageDetailsTableViewControllerDelegate

- (void)languageDetailsTableViewController:
            (LanguageDetailsTableViewController*)tableViewController
                   didSelectOfferTranslate:(BOOL)offerTranslate
                              languageCode:(const std::string&)languageCode {
  // Inform the command handler.
  if (offerTranslate) {
    [self.commandHandler unblockLanguage:languageCode];
  } else {
    [self.commandHandler blockLanguage:languageCode];
  }

  // Update the model and the table view.
  [self updateLanguagesSection];

  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark - LanguageSettingsConsumer

- (void)translateEnabled:(BOOL)enabled {
  // Ignore pref changes while in edit mode.
  if (self.isEditing)
    return;

  // Update the model and the table view.
  [self setTranslateSwitchItemOn:enabled];
  [self updateLanguagesSection];
}

- (void)languagePrefsChanged {
  // Ignore pref changes while in edit mode.
  if (self.isEditing)
    return;

  // Inform the presented AddLanguageTableViewController to update itself.
  [self.addLanguageTableViewController supportedLanguagesListChanged];

  // Update the model and the table view.
  [self updateLanguagesSection];
}

#pragma mark - Helper methods

- (void)populateLanguagesSection {
  TableViewModel* model = self.tableViewModel;

  // Header item.
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text = l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_HEADER);
  [model setHeader:headerItem
      forSectionWithIdentifier:SectionIdentifierLanguages];

  // Languages items.
  [[self.dataSource acceptLanguagesItems]
      enumerateObjectsUsingBlock:^(LanguageItem* item, NSUInteger index,
                                   BOOL* stop) {
        item.type = ItemTypeLanguage;
        [model addItem:item toSectionWithIdentifier:SectionIdentifierLanguages];
      }];

  // Add language item.
  TableViewTextItem* addLanguageItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeAddLanguage];
  self.addLanguageItem = addLanguageItem;
  addLanguageItem.text = l10n_util::GetNSString(
      IDS_IOS_LANGUAGE_SETTINGS_ADD_LANGUAGE_BUTTON_TITLE);
  addLanguageItem.textColor = [UIColor colorNamed:kBlueColor];
  addLanguageItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [self.tableViewModel addItem:addLanguageItem
       toSectionWithIdentifier:SectionIdentifierLanguages];
}

- (void)updateLanguagesSection {
  // Update the model.
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSection];

  // Update the table view.
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierLanguages];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)setAddLanguageItemEnabled:(BOOL)enabled {
  // Update the model.
  self.addLanguageItem.enabled = enabled;
  self.addLanguageItem.textColor = self.isEditing
                                       ? UIColor.cr_secondaryLabelColor
                                       : [UIColor colorNamed:kBlueColor];

  // Update the table view.
  [self reconfigureCellsForItems:@[ self.addLanguageItem ]];
}

- (void)setTranslateSwitchItemEnabled:(BOOL)enabled {
  // Update the model.
  self.translateSwitchItem.enabled = enabled;

  // Update the table view.
  [self reconfigureCellsForItems:@[ self.translateSwitchItem ]];
}

- (void)setTranslateSwitchItemOn:(BOOL)on {
  // Update the model.
  self.translateSwitchItem.on = on;

  // Update the table view.
  [self reconfigureCellsForItems:@[ self.translateSwitchItem ]];
}

// Returns whether Translate can be offered for the language (it can be
// unblocked).
- (BOOL)canOfferTranslateForLanguage:(LanguageItem*)languageItem {
  // Cannot offer Translate for languages not supported by the Translate server.
  if (!languageItem.supportsTranslate)
    return NO;

  // Cannot offer Translate for the last Translate-blocked language.
  if ([languageItem isBlocked] && [self numberOfBlockedLanguages] <= 1) {
    return NO;
  }

  // Cannot offer Translate for the Translate target language.
  return ![languageItem isTargetLanguage];
}

// Returns the number of Translate-blocked languages currently in the model.
- (NSUInteger)numberOfBlockedLanguages {
  __block NSUInteger numberOfBlockedLanguages = 0;
  NSArray<TableViewItem*>* languageItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierLanguages];
  [languageItems enumerateObjectsUsingBlock:^(TableViewItem* item,
                                              NSUInteger idx, BOOL* stop) {
    if (item.type != ItemTypeLanguage)
      return;
    LanguageItem* languageItem = base::mac::ObjCCastStrict<LanguageItem>(item);
    if ([languageItem isBlocked])
      numberOfBlockedLanguages++;
  }];
  return numberOfBlockedLanguages;
}

#pragma mark - Actions

- (void)translateSwitchChanged:(UISwitch*)switchView {
  // Inform the command handler.
  [self.commandHandler setTranslateEnabled:switchView.isOn];

  // Update the model and the table view.
  [self translateEnabled:switchView.isOn];
}

@end
