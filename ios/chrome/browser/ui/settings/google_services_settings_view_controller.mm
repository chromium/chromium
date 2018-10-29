// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_collapsible_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_local_commands.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller_model_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constants used to convert NSIndexPath into a tag. Used as:
// item + section * kSectionOffset
constexpr NSInteger kSectionOffset = 1000;

}  // namespace

@implementation GoogleServicesSettingsViewController

@synthesize presentationDelegate = _presentationDelegate;
@synthesize modelDelegate = _modelDelegate;
@synthesize serviceDelegate = _serviceDelegate;
@synthesize localDispatcher = _localDispatcher;

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style {
  self = [super initWithLayout:layout style:style];
  if (self) {
    self.collectionViewAccessibilityIdentifier =
        @"google_services_settings_view_controller";
    self.title = l10n_util::GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE);
  }
  return self;
}

// Collapse/expand a section at |sectionIndex|.
- (void)toggleSectionWithIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0, indexPath.row);
  NSMutableArray* cellIndexPathsToDeleteOrInsert = [NSMutableArray array];
  CollectionViewModel* model = self.collectionViewModel;
  NSInteger sectionIdentifier =
      [model sectionIdentifierForSection:indexPath.section];
  NSEnumerator* itemEnumerator =
      [[model itemsInSectionWithIdentifier:sectionIdentifier] objectEnumerator];
  // The first item is the title item that does the collapse/expand. This item
  // should always be visible and should be skipped.
  [itemEnumerator nextObject];
  for (ListItem* item in itemEnumerator) {
    NSIndexPath* tabIndexPath = [model indexPathForItem:item];
    [cellIndexPathsToDeleteOrInsert addObject:tabIndexPath];
  }

  BOOL shouldCollapse = ![model sectionIsCollapsed:sectionIdentifier];
  void (^tableUpdates)(void) = ^{
    if (!shouldCollapse) {
      [model setSection:sectionIdentifier collapsed:NO];
      [self.collectionView
          insertItemsAtIndexPaths:cellIndexPathsToDeleteOrInsert];
    } else {
      [model setSection:sectionIdentifier collapsed:YES];
      [self.collectionView
          deleteItemsAtIndexPaths:cellIndexPathsToDeleteOrInsert];
    }
  };
  [self.collectionView performBatchUpdates:tableUpdates completion:nil];

  SettingsCollapsibleItem* item = [model itemAtIndexPath:indexPath];
  item.collapsed = shouldCollapse;
  SettingsCollapsibleCell* cell = (SettingsCollapsibleCell*)[self.collectionView
      cellForItemAtIndexPath:indexPath];
  [cell setCollapsed:shouldCollapse animated:YES];
}
#pragma mark - Private

- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath {
  return indexPath.item + indexPath.section * kSectionOffset;
}

- (NSIndexPath*)indexPathForTag:(NSInteger)tag {
  NSInteger section = tag / kSectionOffset;
  NSInteger item = tag - (section * kSectionOffset);
  return [NSIndexPath indexPathForItem:item inSection:section];
}

- (void)switchAction:(UISwitch*)sender {
  NSIndexPath* indexPath = [self indexPathForTag:sender.tag];
  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);
  BOOL isOn = sender.isOn;
  GoogleServicesSettingsCommandID commandID =
      static_cast<GoogleServicesSettingsCommandID>(syncSwitchItem.commandID);
  switch (commandID) {
    case GoogleServicesSettingsCommandIDToggleSyncEverything:
      [self.serviceDelegate toggleSyncEverythingWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleDataTypeSync:
      [self.serviceDelegate toggleSyncDataSync:syncSwitchItem.dataType
                                     withValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDAutocompleteWalletService:
      [self.serviceDelegate toggleAutocompleteWalletServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
      [self.serviceDelegate toggleAutocompleteSearchesServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
      [self.serviceDelegate togglePreloadPagesServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleImproveChromeService:
      [self.serviceDelegate toggleImproveChromeServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
      [self.serviceDelegate toggleBetterSearchAndBrowsingServiceWithValue:isOn];
      break;
    case GoogleServicesSettingsCommandIDRestartAuthenticationFlow:
    case GoogleServicesSettingsReauthDialogAsSyncIsInAuthError:
    case GoogleServicesSettingsCommandIDShowPassphraseDialog:
    case GoogleServicesSettingsCommandIDNoOp:
    case GoogleServicesSettingsCommandIDOpenGoogleActivityControlsDialog:
    case GoogleServicesSettingsCommandIDOpenEncryptionDialog:
    case GoogleServicesSettingsCommandIDOpenManageSyncedDataWebPage:
      // Command ID not related with switch action.
      NOTREACHED();
      break;
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  if ([cell isKindOfClass:[SyncSwitchCell class]]) {
    SyncSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SyncSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    switchCell.switchView.tag = [self tagForIndexPath:indexPath];
  }
  return cell;
}

#pragma mark - GoogleServicesSettingsConsumer

- (void)insertSections:(NSIndexSet*)sections {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.collectionView insertSections:sections];
}

- (void)deleteSections:(NSIndexSet*)sections {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.collectionView deleteSections:sections];
}

- (void)reloadSections:(NSIndexSet*)sections {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.collectionView reloadSections:sections];
}

- (void)reloadItem:(CollectionViewItem*)item {
  if (!self.collectionViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  NSIndexPath* indexPath = [self.collectionViewModel indexPathForItem:item];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate googleServicesSettingsViewControllerLoadModel:self];
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self reloadData];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        googleServicesSettingsViewControllerDidRemove:self];
  }
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  CGFloat width =
      CGRectGetWidth(collectionView.bounds) - inset.left - inset.right;
  return [item.cellClass cr_preferredHeightForWidth:width forItem:item];
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHighlightItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView
      shouldHighlightItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[SyncSwitchItem class]]) {
    return NO;
  } else if ([item isKindOfClass:[SettingsCollapsibleItem class]] ||
             [item isKindOfClass:[SettingsImageDetailTextItem class]]) {
    return YES;
  } else if ([item isKindOfClass:[CollectionViewTextItem class]]) {
    CollectionViewTextItem* textItem =
        base::mac::ObjCCast<CollectionViewTextItem>(item);
    return textItem.enabled;
  }
  // The highlight of an item should be explicitly defined. If the item can be
  // highlighted, then a command ID should be defined in
  // -[GoogleServicesSettingsViewController collectionView:
  // didSelectItemAtIndexPath:].
  NOTREACHED();
  return NO;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[SettingsCollapsibleItem class]]) {
    [self toggleSectionWithIndexPath:indexPath];
    return;
  }
  GoogleServicesSettingsCommandID commandID =
      GoogleServicesSettingsCommandIDNoOp;
  if ([item isKindOfClass:[CollectionViewTextItem class]]) {
    CollectionViewTextItem* textItem =
        base::mac::ObjCCast<CollectionViewTextItem>(item);
    commandID =
        static_cast<GoogleServicesSettingsCommandID>(textItem.commandID);
  } else if ([item isKindOfClass:[SettingsImageDetailTextItem class]]) {
    SettingsImageDetailTextItem* imageDetailTextItem =
        base::mac::ObjCCast<SettingsImageDetailTextItem>(item);
    commandID = static_cast<GoogleServicesSettingsCommandID>(
        imageDetailTextItem.commandID);
  } else {
    // A command ID should be defined when the cell is selected.
    NOTREACHED();
  }
  switch (commandID) {
    case GoogleServicesSettingsCommandIDRestartAuthenticationFlow:
      [self.localDispatcher restartAuthenticationFlow];
      break;
    case GoogleServicesSettingsReauthDialogAsSyncIsInAuthError:
      [self.localDispatcher openReauthDialogAsSyncIsInAuthError];
      break;
    case GoogleServicesSettingsCommandIDShowPassphraseDialog:
      [self.localDispatcher openPassphraseDialog];
      break;
    case GoogleServicesSettingsCommandIDOpenGoogleActivityControlsDialog:
      [self.localDispatcher openGoogleActivityControlsDialog];
      break;
    case GoogleServicesSettingsCommandIDOpenEncryptionDialog:
      [self.localDispatcher openEncryptionDialog];
      break;
    case GoogleServicesSettingsCommandIDOpenManageSyncedDataWebPage:
      [self.localDispatcher openManageSyncedDataWebPage];
      break;
    case GoogleServicesSettingsCommandIDNoOp:
    case GoogleServicesSettingsCommandIDToggleSyncEverything:
    case GoogleServicesSettingsCommandIDToggleDataTypeSync:
    case GoogleServicesSettingsCommandIDAutocompleteWalletService:
    case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
    case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
    case GoogleServicesSettingsCommandIDToggleImproveChromeService:
    case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
      // Command ID not related with cell selection.
      NOTREACHED();
      break;
  }
}

@end
