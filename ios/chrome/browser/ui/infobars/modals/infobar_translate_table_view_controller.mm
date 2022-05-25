// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSourceLanguage = kItemTypeEnumZero,
  ItemTypeTargetLanguage,
  ItemTypeAlwaysTranslateSource,
  ItemTypeNeverTranslateSource,
  ItemTypeNeverTranslateSite,
  ItemTypeTranslateButton,
  ItemTypeShowOriginalButton,
};

@interface InfobarTranslateTableViewController ()

// InfobarTranslateModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarTranslateModalDelegate>
    infobarModalDelegate;

// Prefs updated by |modalConsumer|.
// The source language from which to translate.
@property(nonatomic, copy) NSString* sourceLanguage;
// The target language to which to translate.
@property(nonatomic, copy) NSString* targetLanguage;
// YES if the pref is set to enable the Translate button.
@property(nonatomic, assign) BOOL enableTranslateActionButton;
// YES if the pref is set to configure the Translate button to trigger
// translateWithNewLanguages().
@property(nonatomic, assign) BOOL updateLanguageBeforeTranslate;
// YES if the pref is set to enable and display the "Show Original" Button.
// Otherwise, hide it.
@property(nonatomic, assign) BOOL enableAndDisplayShowOriginalButton;
// YES if the pref is set to always translate for the source language.
@property(nonatomic, assign) BOOL shouldAlwaysTranslate;
// YES if the pref is set to show the "Never Translate language" button.
@property(nonatomic, assign) BOOL shouldDisplayNeverTranslateLanguageButton;
// NO if the current pref is set to never translate the source language.
@property(nonatomic, assign) BOOL isTranslatableLanguage;
// YES if the pref is set to show the "Never Translate Site" button.
@property(nonatomic, assign) BOOL shouldDisplayNeverTranslateSiteButton;
// YES if the pref is set to never translate the current site.
@property(nonatomic, assign) BOOL isSiteOnNeverPromptList;

@end

@implementation InfobarTranslateTableViewController

- (instancetype)initWithDelegate:
    (id<InfobarTranslateModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _infobarModalDelegate = modalDelegate;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor clearColor];
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

- (void)viewDidDisappear:(BOOL)animated {
  // Only call delegate method if the modal is being dismissed, if this VC is
  // inside a NavigationController we need to check if the NavigationController
  // is being dismissed.
  if ([self.navigationController isBeingDismissed] || [self isBeingDismissed]) {
    [self.infobarModalDelegate modalInfobarWasDismissed:self];
  }
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

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextEditItem* sourceLanguageItem = [self
      textEditItemForType:ItemTypeSourceLanguage
            textFieldName:
                l10n_util::GetNSString(
                    IDS_IOS_TRANSLATE_INFOBAR_MODAL_SOURCE_LANGUAGE_FIELD_NAME)
           textFieldValue:self.sourceLanguage];
  sourceLanguageItem.accessibilityIdentifier =
      kTranslateInfobarModalTranslateSourceLanguageItemAXId;
  [model addItem:sourceLanguageItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextEditItem* targetLanguageItem = [self
      textEditItemForType:ItemTypeTargetLanguage
            textFieldName:
                l10n_util::GetNSString(
                    IDS_IOS_TRANSLATE_INFOBAR_MODAL_TARGET_LANGUAGE_FIELD_NAME)
           textFieldValue:self.targetLanguage];
  targetLanguageItem.accessibilityIdentifier =
      kTranslateInfobarModalTranslateTargetLanguageItemAXId;
  [model addItem:targetLanguageItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* translateButtonItem = [self
      textButtonItemForType:ItemTypeTranslateButton
                 buttonText:l10n_util::GetNSString(
                                IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION)];
  translateButtonItem.disableButtonIntrinsicWidth = YES;
  translateButtonItem.buttonAccessibilityIdentifier =
      kTranslateInfobarModalTranslateButtonAXId;
  if (!self.enableTranslateActionButton) {
    translateButtonItem.buttonBackgroundColor =
        [UIColor colorNamed:kDisabledTintColor];
  }
  [model addItem:translateButtonItem
      toSectionWithIdentifier:SectionIdentifierContent];

  if (self.enableAndDisplayShowOriginalButton) {
    TableViewTextButtonItem* showOriginalButtonItem = [self
        textButtonItemForType:ItemTypeShowOriginalButton
                   buttonText:
                       l10n_util::GetNSString(
                           IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_UNDO_ACTION)];
    showOriginalButtonItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
    showOriginalButtonItem.buttonBackgroundColor = [UIColor clearColor];
    showOriginalButtonItem.buttonAccessibilityIdentifier =
        kTranslateInfobarModalShowOriginalButtonAXId;
    [model addItem:showOriginalButtonItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  TableViewTextButtonItem* alwaysTranslateSourceItem =
      [self textButtonItemForType:ItemTypeAlwaysTranslateSource
                       buttonText:[self shouldAlwaysTranslateButtonText]];
  alwaysTranslateSourceItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
  alwaysTranslateSourceItem.buttonBackgroundColor = [UIColor clearColor];
  alwaysTranslateSourceItem.buttonAccessibilityIdentifier =
      kTranslateInfobarModalAlwaysTranslateButtonAXId;
  [model addItem:alwaysTranslateSourceItem
      toSectionWithIdentifier:SectionIdentifierContent];

  if (self.shouldDisplayNeverTranslateLanguageButton) {
    TableViewTextButtonItem* neverTranslateSourceItem = [self
        textButtonItemForType:ItemTypeNeverTranslateSource
                   buttonText:[self shouldNeverTranslateSourceButtonText]];
    neverTranslateSourceItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
    neverTranslateSourceItem.buttonBackgroundColor = [UIColor clearColor];
    neverTranslateSourceItem.buttonAccessibilityIdentifier =
        kTranslateInfobarModalNeverTranslateButtonAXId;
    [model addItem:neverTranslateSourceItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  if (self.shouldDisplayNeverTranslateSiteButton) {
    TableViewTextButtonItem* neverTranslateSiteItem =
        [self textButtonItemForType:ItemTypeNeverTranslateSite
                         buttonText:[self shouldNeverTranslateSiteButtonText]];
    neverTranslateSiteItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
    neverTranslateSiteItem.buttonBackgroundColor = [UIColor clearColor];
    neverTranslateSiteItem.buttonAccessibilityIdentifier =
        kTranslateInfobarModalNeverTranslateSiteButtonAXId;
    [model addItem:neverTranslateSiteItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }
}

#pragma mark - InfobarTranslateModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.sourceLanguage = prefs[kSourceLanguagePrefKey];
  self.targetLanguage = prefs[kTargetLanguagePrefKey];
  self.enableTranslateActionButton =
      [prefs[kEnableTranslateButtonPrefKey] boolValue];
  self.updateLanguageBeforeTranslate =
      [prefs[kUpdateLanguageBeforeTranslatePrefKey] boolValue];
  self.enableAndDisplayShowOriginalButton =
      [prefs[kEnableAndDisplayShowOriginalButtonPrefKey] boolValue];
  self.shouldAlwaysTranslate = [prefs[kShouldAlwaysTranslatePrefKey] boolValue];
  self.shouldDisplayNeverTranslateLanguageButton =
      [prefs[kDisplayNeverTranslateLanguagePrefKey] boolValue];
  self.shouldDisplayNeverTranslateSiteButton =
      [prefs[kDisplayNeverTranslateSiteButtonPrefKey] boolValue];
  self.isTranslatableLanguage =
      [prefs[kIsTranslatableLanguagePrefKey] boolValue];
  self.isSiteOnNeverPromptList =
      [prefs[kIsSiteOnNeverPromptListPrefKey] boolValue];
  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  TableViewTextButtonCell* tableViewTextButtonCell =
      base::mac::ObjCCast<TableViewTextButtonCell>(cell);
  // Clear the existing targets before adding the new ones.
  [tableViewTextButtonCell.button removeTarget:nil
                                        action:nil
                              forControlEvents:UIControlEventAllEvents];

  switch (itemType) {
    case ItemTypeTranslateButton: {
      DCHECK(tableViewTextButtonCell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(translateButtonWasTapped:)
          forControlEvents:UIControlEventTouchUpInside];
      tableViewTextButtonCell.button.enabled = self.enableTranslateActionButton;
      break;
    }
    case ItemTypeShowOriginalButton: {
      DCHECK(tableViewTextButtonCell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      [tableViewTextButtonCell.button addTarget:self.infobarModalDelegate
                                         action:@selector(showSourceLanguage)
                               forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeAlwaysTranslateSource: {
      DCHECK(tableViewTextButtonCell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      if (self.shouldAlwaysTranslate) {
        [tableViewTextButtonCell.button
                   addTarget:self.infobarModalDelegate
                      action:@selector(undoAlwaysTranslateSourceLanguage)
            forControlEvents:UIControlEventTouchUpInside];
      } else {
        [tableViewTextButtonCell.button
                   addTarget:self.infobarModalDelegate
                      action:@selector(alwaysTranslateSourceLanguage)
            forControlEvents:UIControlEventTouchUpInside];
      }
      break;
    }
    case ItemTypeNeverTranslateSource: {
      DCHECK(tableViewTextButtonCell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      if (self.isTranslatableLanguage) {
        [tableViewTextButtonCell.button
                   addTarget:self.infobarModalDelegate
                      action:@selector(neverTranslateSourceLanguage)
            forControlEvents:UIControlEventTouchUpInside];
      } else {
        [tableViewTextButtonCell.button
                   addTarget:self.infobarModalDelegate
                      action:@selector(undoNeverTranslateSourceLanguage)
            forControlEvents:UIControlEventTouchUpInside];
      }
      break;
    }
    case ItemTypeNeverTranslateSite: {
      DCHECK(tableViewTextButtonCell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      if (self.isSiteOnNeverPromptList) {
        [tableViewTextButtonCell.button
                   addTarget:self.infobarModalDelegate
                      action:@selector(undoNeverTranslateSite)
            forControlEvents:UIControlEventTouchUpInside];
      } else {
        [tableViewTextButtonCell.button addTarget:self.infobarModalDelegate
                                           action:@selector(neverTranslateSite)
                                 forControlEvents:UIControlEventTouchUpInside];
      }
      break;
    }
    case ItemTypeSourceLanguage:
    case ItemTypeTargetLanguage:
      break;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSourceLanguage) {
    [self.infobarModalDelegate showChangeSourceLanguageOptions];
  } else if (itemType == ItemTypeTargetLanguage) {
    [self.infobarModalDelegate showChangeTargetLanguageOptions];
  }
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - Private

- (TableViewTextEditItem*)textEditItemForType:(ItemType)itemType
                                textFieldName:(NSString*)name
                               textFieldValue:(NSString*)value {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:itemType];
  item.textFieldEnabled = NO;
  item.textFieldName = name;
  item.textFieldValue = value;
  return item;
}

- (TableViewTextButtonItem*)textButtonItemForType:(ItemType)itemType
                                       buttonText:(NSString*)buttonText {
  TableViewTextButtonItem* item =
      [[TableViewTextButtonItem alloc] initWithType:itemType];
  item.boldButtonText = NO;
  item.buttonText = buttonText;
  return item;
}

- (void)dismissInfobarModal {
  // TODO(crbug.com/1014959): add metrics
  [self.infobarModalDelegate dismissInfobarModal:self];
}

// Call the appropriate method to trigger Translate depending on if the
// languages need to be updated first.
- (void)translateButtonWasTapped:(UIButton*)sender {
  if (self.updateLanguageBeforeTranslate) {
    [self.infobarModalDelegate translateWithNewLanguages];
  } else {
    [self.infobarModalDelegate modalInfobarButtonWasAccepted:self];
  }
}

// Returns the text of the modal button allowing the user to always translate
// the source language or revert back to offering to translate.
- (NSString*)shouldAlwaysTranslateButtonText {
  NSString* sourceLanguage = self.sourceLanguage;
  if (self.shouldAlwaysTranslate) {
    return l10n_util::GetNSStringF(
        IDS_IOS_TRANSLATE_INFOBAR_OFFER_TRANSLATE_SOURCE_BUTTON_TITLE,
        base::SysNSStringToUTF16(sourceLanguage));
  } else {
    return l10n_util::GetNSStringF(
        IDS_IOS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE_SOURCE_BUTTON_TITLE,
        base::SysNSStringToUTF16(sourceLanguage));
  }
}

// Returns the text of the modal button allowing the user to never translate the
// source language or revert back to offering to translate.
- (NSString*)shouldNeverTranslateSourceButtonText {
  NSString* sourceLanguage = self.sourceLanguage;
  if (self.isTranslatableLanguage) {
    return l10n_util::GetNSStringF(
        IDS_IOS_TRANSLATE_INFOBAR_NEVER_TRANSLATE_SOURCE_BUTTON_TITLE,
        base::SysNSStringToUTF16(sourceLanguage));
  } else {
    return l10n_util::GetNSStringF(
        IDS_IOS_TRANSLATE_INFOBAR_OFFER_TRANSLATE_SOURCE_BUTTON_TITLE,
        base::SysNSStringToUTF16(sourceLanguage));
  }
}

// Returns the text of the modal button allowing the user to never translate the
// site or revert back to offering to translate.
- (NSString*)shouldNeverTranslateSiteButtonText {
  if (self.isSiteOnNeverPromptList) {
    return l10n_util::GetNSString(
        IDS_IOS_TRANSLATE_INFOBAR_OFFER_TRANSLATE_SITE_BUTTON_TITLE);
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_TRANSLATE_INFOBAR_NEVER_TRANSLATE_SITE_BUTTON_TITLE);
  }
}

@end
