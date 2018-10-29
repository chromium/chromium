// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_observer.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/elements/chrome_activity_overlay_coordinator.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_manager.h"
#import "ios/chrome/browser/ui/settings/time_range_selector_collection_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using OnBrowsingDataRemovedBlock = void (^)(BrowsingDataRemoveMask mask);

// BrowsingDataRemoverObserverWrapper observes a BrowsingDataRemover and
// invokes a block when browsing data removal completes.
class BrowsingDataRemoverObserverWrapper : public BrowsingDataRemoverObserver {
 public:
  explicit BrowsingDataRemoverObserverWrapper(
      OnBrowsingDataRemovedBlock on_browsing_data_removed_block);
  ~BrowsingDataRemoverObserverWrapper() override;

  // BrowsingDataRemoverObserver implementation.
  void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                             BrowsingDataRemoveMask mask) override;

 private:
  OnBrowsingDataRemovedBlock on_browsing_data_removed_block_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverObserverWrapper);
};

BrowsingDataRemoverObserverWrapper::BrowsingDataRemoverObserverWrapper(
    OnBrowsingDataRemovedBlock on_browsing_data_removed_block)
    : on_browsing_data_removed_block_(on_browsing_data_removed_block) {
  DCHECK(on_browsing_data_removed_block_);
}

BrowsingDataRemoverObserverWrapper::~BrowsingDataRemoverObserverWrapper() =
    default;

void BrowsingDataRemoverObserverWrapper::OnBrowsingDataRemoved(
    BrowsingDataRemover* remover,
    BrowsingDataRemoveMask mask) {
  on_browsing_data_removed_block_(mask);
}

}  // namespace

@interface ClearBrowsingDataCollectionViewController () {
  ios::ChromeBrowserState* _browserState;  // weak

  // Observer for browsing data removal events and associated ScopedObserver
  // used to track registration with BrowsingDataRemover. They both may be
  // null if the new Clear Browser Data UI is disabled.
  std::unique_ptr<BrowsingDataRemoverObserver> observer_;
  std::unique_ptr<
      ScopedObserver<BrowsingDataRemover, BrowsingDataRemoverObserver>>
      scoped_observer_;
}

// TODO(crbug.com/850699): remove direct dependency and replace with
// delegate.
@property(nonatomic, readonly, strong) ClearBrowsingDataManager* dataManager;

// Coordinator that managers an action sheet to clear browsing data.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Coordinator for displaying a modal overlay with native activity indicator to
// prevent the user from interacting with the page.
@property(nonatomic, strong)
    ChromeActivityOverlayCoordinator* chromeActivityOverlayCoordinator;

// Restarts the counters for data types specified in the mask.
- (void)restartCounters:(BrowsingDataRemoveMask)mask;

@end

@implementation ClearBrowsingDataCollectionViewController
@synthesize actionSheetCoordinator = _actionSheetCoordinator;
@synthesize dataManager = _dataManager;

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _browserState = browserState;
    _dataManager = [[ClearBrowsingDataManager alloc]
        initWithBrowserState:browserState
                    listType:ClearBrowsingDataListType::
                                 kListTypeCollectionView];
    _dataManager.linkDelegate = self;
    _dataManager.consumer = self;

    self.title = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
    self.collectionViewAccessibilityIdentifier =
        kClearBrowsingDataCollectionViewAccessibilityIdentifier;

    if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {

      __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
      observer_ = std::make_unique<BrowsingDataRemoverObserverWrapper>(
          ^(BrowsingDataRemoveMask mask) {
            [weakSelf restartCounters:mask];
          });

      scoped_observer_ = std::make_unique<
          ScopedObserver<BrowsingDataRemover, BrowsingDataRemoverObserver>>(
          observer_.get());
      scoped_observer_->Add(
          BrowsingDataRemoverFactory::GetForBrowserState(browserState));
    }

    [self restartCounters:BrowsingDataRemoveMask::REMOVE_ALL];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self loadModel];
}

#pragma mark CollectionViewController

- (void)loadModel {
  [super loadModel];
  [self.dataManager loadModel:self.collectionViewModel];
}

#pragma mark ClearBrowsingDataConsumer

- (void)updateCellsForItem:(ListItem*)item {
  [self reconfigureCellsForItems:@[ item ]];

  // Relayout the cells to adapt to the new contents height.
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)updateCounter:(NSInteger)itemType detailText:(NSString*)detailText {
  CollectionViewModel* model = self.collectionViewModel;
  if (!model)
    return;

  NSIndexPath* indexPath =
      [model indexPathForItemType:itemType
                sectionIdentifier:SectionIdentifierDataTypes];

  ClearBrowsingDataItem* clearDataItem =
      base::mac::ObjCCastStrict<ClearBrowsingDataItem>(
          [model itemAtIndexPath:indexPath]);

  // Do nothing if the text has not changed.
  if ([detailText isEqualToString:clearDataItem.detailText])
    return;

  clearDataItem.detailText = detailText;
  [self reconfigureCellsForItems:@[ clearDataItem ]];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)showBrowsingHistoryRemovedDialog {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_DESCRIPTION);

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
  UIAlertAction* openMyActivityAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OPEN_HISTORY_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakSelf openMyActivityLink];
              }];

  UIAlertAction* okAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK_BUTTON)
                style:UIAlertActionStyleCancel
              handler:nil];
  [alertController addAction:openMyActivityAction];
  [alertController addAction:okAction];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)removeBrowsingDataForBrowserState:(ios::ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  base::RecordAction(
      base::UserMetricsAction("MobileClearBrowsingDataTriggeredFromLegacyUI"));

  // Show activity indicator modal while removal is happening.
  self.chromeActivityOverlayCoordinator =
      [[ChromeActivityOverlayCoordinator alloc]
          initWithBaseViewController:self.navigationController];
  self.chromeActivityOverlayCoordinator.messageText =
      l10n_util::GetNSStringWithFixup(
          IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
  [self.chromeActivityOverlayCoordinator start];

  __weak ClearBrowsingDataCollectionViewController* weakSelf = self;
  dispatch_time_t timeOneSecondLater =
      dispatch_time(DISPATCH_TIME_NOW, (1 * NSEC_PER_SEC));
  void (^removeBrowsingDidFinishCompletionBlock)(void) = ^void() {
    ClearBrowsingDataCollectionViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    // Sometimes clear browsing data is really short
    // (<1sec), so ensure that overlay displays for at
    // least 1 second instead of looking like a glitch.
    dispatch_after(timeOneSecondLater, dispatch_get_main_queue(), ^{
      [self.chromeActivityOverlayCoordinator stop];
      if (completionBlock)
        completionBlock();
    });

  };
  [self.dispatcher
      removeBrowsingDataForBrowserState:browserState
                             timePeriod:timePeriod
                             removeMask:removeMask
                        completionBlock:removeBrowsingDidFinishCompletionBlock];
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeTimeRange: {
      UIViewController* controller =
          [[TimeRangeSelectorCollectionViewController alloc]
              initWithPrefs:_browserState->GetPrefs()
                   delegate:self.dataManager];
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeDataTypeBrowsingHistory:
    case ItemTypeDataTypeCookiesSiteData:
    case ItemTypeDataTypeCache:
    case ItemTypeDataTypeSavedPasswords:
    case ItemTypeDataTypeAutofill: {
      // Toggle the checkmark.
      // TODO(crbug.com/631486): Custom checkmark animation to be implemented.
      ClearBrowsingDataItem* clearDataItem =
          base::mac::ObjCCastStrict<ClearBrowsingDataItem>(
              [self.collectionViewModel itemAtIndexPath:indexPath]);
      if (clearDataItem.accessoryType == MDCCollectionViewCellAccessoryNone) {
        clearDataItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
        _browserState->GetPrefs()->SetBoolean(clearDataItem.prefName, true);
        if (itemType == ItemTypeDataTypeCookiesSiteData &&
            experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
          [self updateCounter:itemType
                   detailText:l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER)];
        }
      } else {
        clearDataItem.accessoryType = MDCCollectionViewCellAccessoryNone;
        _browserState->GetPrefs()->SetBoolean(clearDataItem.prefName, false);
        if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
          [self updateCounter:itemType detailText:@""];
        }
      }
      [self reconfigureCellsForItems:@[ clearDataItem ]];
      break;
    }
    case ItemTypeClearBrowsingDataButton:
      UICollectionViewCell* cell =
          [collectionView cellForItemAtIndexPath:indexPath];
      [self presentClearBrowsingDataConfirmationDialog:cell];
      break;
  }
}

#pragma mark Clear browsing data

// Displays an action sheet to the user confirming the clearing of user data. If
// the clearing is confirmed, clears the data.
- (void)presentClearBrowsingDataConfirmationDialog:(UICollectionViewCell*)cell {
  BrowsingDataRemoveMask dataTypeMaskToRemove =
      BrowsingDataRemoveMask::REMOVE_NOTHING;
  NSArray* dataTypeItems = [self.collectionViewModel
      itemsInSectionWithIdentifier:SectionIdentifierDataTypes];
  for (ClearBrowsingDataItem* dataTypeItem in dataTypeItems) {
    DCHECK([dataTypeItem isKindOfClass:[ClearBrowsingDataItem class]]);
    if (dataTypeItem.accessoryType == MDCCollectionViewCellAccessoryCheckmark) {
      dataTypeMaskToRemove = dataTypeMaskToRemove | dataTypeItem.dataTypeMask;
    }
  }
  self.actionSheetCoordinator = [self.dataManager
      actionSheetCoordinatorWithDataTypesToRemove:dataTypeMaskToRemove
                               baseViewController:self
                                       sourceRect:CGRectMake(
                                                      CGRectGetMidX(cell.frame),
                                                      CGRectGetMidY(cell.frame),
                                                      1, 1)
                                       sourceView:self.collectionView];
  if (self.actionSheetCoordinator) {
    [self.actionSheetCoordinator start];
  }
}

- (void)openMyActivityLink {
  OpenNewTabCommand* openMyActivityCommand =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kGoogleMyAccountURL)];
  [self.dispatcher closeSettingsUIAndOpenURL:openMyActivityCommand];
}

- (void)restartCounters:(BrowsingDataRemoveMask)mask {
  CollectionViewModel* model = self.collectionViewModel;
  if (!self.collectionViewModel)
    return;

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeDataTypeBrowsingHistory
           sectionIdentifier:SectionIdentifierDataTypes];
    ClearBrowsingDataItem* historyItem =
        base::mac::ObjCCastStrict<ClearBrowsingDataItem>(
            [model itemAtIndexPath:indexPath]);
    [historyItem restartCounter];
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_PASSWORDS)) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeDataTypeSavedPasswords
           sectionIdentifier:SectionIdentifierDataTypes];
    ClearBrowsingDataItem* passwordsItem =
        base::mac::ObjCCastStrict<ClearBrowsingDataItem>(
            [model itemAtIndexPath:indexPath]);
    [passwordsItem restartCounter];
  }

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_FORM_DATA)) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeDataTypeAutofill
           sectionIdentifier:SectionIdentifierDataTypes];
    ClearBrowsingDataItem* autofillItem =
        base::mac::ObjCCastStrict<ClearBrowsingDataItem>(
            [model itemAtIndexPath:indexPath]);
    [autofillItem restartCounter];
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeFooterSavedSiteData:
    case ItemTypeFooterGoogleAccount:
    case ItemTypeFooterGoogleAccountAndMyActivity:
    case ItemTypeFooterClearSyncAndSavedSiteData:
      return YES;
    default:
      return NO;
  }
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierClearSyncAndSavedSiteData:
    case SectionIdentifierSavedSiteData:
      // Display the footer in the default style with no "card" UI and no
      // section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierClearSyncAndSavedSiteData:
    case SectionIdentifierSavedSiteData:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierClearSyncAndSavedSiteData:
    case SectionIdentifierSavedSiteData: {
      CollectionViewItem* item =
          [self.collectionViewModel itemAtIndexPath:indexPath];
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    }
    case SectionIdentifierDataTypes: {
      ClearBrowsingDataItem* clearDataItem =
          base::mac::ObjCCastStrict<ClearBrowsingDataItem>(
              [self.collectionViewModel itemAtIndexPath:indexPath]);
      return (clearDataItem.detailText.length > 0)
                 ? MDCCellDefaultTwoLineHeight
                 : MDCCellDefaultOneLineHeight;
    }
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

@end
