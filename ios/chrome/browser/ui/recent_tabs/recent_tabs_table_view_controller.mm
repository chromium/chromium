// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#include "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_image_data_source.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller_delegate.h"
#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_presenter.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_accessory_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_activity_indicator_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_disclosure_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierRecentlyClosedTabs = kSectionIdentifierEnumZero,
  SectionIdentifierOtherDevices,
  // The first SessionsSectionIdentifier index.
  kFirstSessionSectionIdentifier,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeRecentlyClosedHeader = kItemTypeEnumZero,
  ItemTypeRecentlyClosed,
  ItemTypeOtherDevicesHeader,
  ItemTypeOtherDevicesSyncOff,
  ItemTypeOtherDevicesNoSessions,
  ItemTypeOtherDevicesSigninPromo,
  ItemTypeOtherDevicesSyncInProgressHeader,
  ItemTypeSessionHeader,
  ItemTypeSessionTabData,
  ItemTypeShowFullHistory,
};

// Key for saving whether the Other Device section is collapsed.
NSString* const kOtherDeviceCollapsedKey = @"OtherDevicesCollapsed";
// Key for saving whether the Recently Closed section is collapsed.
NSString* const kRecentlyClosedCollapsedKey = @"RecentlyClosedCollapsed";
// There are 2 static sections before the first SessionSection.
int const kNumberOfSectionsBeforeSessions = 1;
// Estimated Table Row height.
const CGFloat kEstimatedRowHeight = 56;
// Separation space between sections.
const CGFloat kSeparationSpaceBetweenSections = 9;
// Section index for recently closed tabs.
const int kRecentlyClosedTabsSectionIndex = 0;

}  // namespace

@interface RecentTabsTableViewController ()<SigninPromoViewConsumer,
                                            SigninPresenter,
                                            SyncPresenter,
                                            UIGestureRecognizerDelegate> {
  std::unique_ptr<synced_sessions::SyncedSessions> _syncedSessions;
}
// There is no need to update the table view when other view controllers
// are obscuring the table view. Bookkeeping is based on |-viewWillAppear:|
// and |-viewWillDisappear methods. Note that the |Did| methods are not reliably
// called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL updatesTableView;
// The service that manages the recently closed tabs
@property(nonatomic, assign) sessions::TabRestoreService* tabRestoreService;
// The sync state.
@property(nonatomic, assign) SessionsSyncUserState sessionState;
// Handles displaying the context menu for all form factors.
@property(nonatomic, strong) ContextMenuCoordinator* contextMenuCoordinator;
@property(nonatomic, strong) SigninPromoViewMediator* signinPromoViewMediator;
@end

@implementation RecentTabsTableViewController : ChromeTableViewController
@synthesize browserState = _browserState;
@synthesize contextMenuCoordinator = _contextMenuCoordinator;
@synthesize delegate = delegate_;
@synthesize dispatcher = _dispatcher;
@synthesize presentationDelegate = _presentationDelegate;
@synthesize imageDataSource = _imageDataSource;
@synthesize loader = _loader;
@synthesize sessionState = _sessionState;
@synthesize signinPromoViewMediator = _signinPromoViewMediator;
@synthesize updatesTableView = _updatesTableView;
@synthesize tabRestoreService = _tabRestoreService;

#pragma mark - Public Interface

- (instancetype)init {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _sessionState = SessionsSyncUserState::USER_SIGNED_OUT;
    _syncedSessions.reset(new synced_sessions::SyncedSessions());
  }
  return self;
}

- (void)dealloc {
  [_signinPromoViewMediator signinPromoViewRemoved];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kRecentTabsTableViewControllerAccessibilityIdentifier;
  [self.tableView setDelegate:self];
  self.tableView.cellLayoutMarginsFollowReadableWidth = NO;
  self.tableView.estimatedRowHeight = kEstimatedRowHeight;
  if (@available(iOS 11.0, *))
    self.tableView.estimatedSectionHeaderHeight = kEstimatedRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionFooterHeight = 0.0;
  self.title = l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.updatesTableView = YES;
  // The table view might get stale while hidden, so we need to forcibly refresh
  // it here.
  [self loadModel];
  [self.tableView reloadData];
}

- (void)viewWillDisappear:(BOOL)animated {
  self.updatesTableView = NO;
  [super viewWillDisappear:animated];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  [self addRecentlyClosedSection];

  if (self.sessionState ==
      SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) {
    [self addSessionSections];
  } else {
    [self addOtherDevicesSectionForState:self.sessionState];
  }
}

#pragma mark Recently Closed Section

- (void)addRecentlyClosedSection {
  TableViewModel* model = self.tableViewModel;

  // Recently Closed Section.
  [model insertSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs
                             atIndex:kRecentlyClosedTabsSectionIndex];
  [model setSectionIdentifier:SectionIdentifierRecentlyClosedTabs
                 collapsedKey:kRecentlyClosedCollapsedKey];
  TableViewDisclosureHeaderFooterItem* header =
      [[TableViewDisclosureHeaderFooterItem alloc]
          initWithType:ItemTypeRecentlyClosedHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_RECENT_TABS_RECENTLY_CLOSED);
  [model setHeader:header
      forSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
  header.collapsed = [self.tableViewModel
      sectionIsCollapsed:SectionIdentifierRecentlyClosedTabs];

  // Add Recently Closed Tabs Cells.
  [self addRecentlyClosedTabItems];

  // Add show full history item last.
  TableViewAccessoryItem* historyItem =
      [[TableViewAccessoryItem alloc] initWithType:ItemTypeShowFullHistory];
  historyItem.title = l10n_util::GetNSString(IDS_HISTORY_SHOWFULLHISTORY_LINK);
  historyItem.image = [UIImage imageNamed:@"show_history"];
  historyItem.cellAccessibilityIdentifier =
      kRecentTabsShowFullHistoryCellAccessibilityIdentifier;
  [model addItem:historyItem
      toSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
}

// Iterates through all the TabRestoreService entries and adds items to the
// recently closed tabs section. This method performs no UITableView operations.
- (void)addRecentlyClosedTabItems {
  if (!self.tabRestoreService)
    return;
  for (auto iter = self.tabRestoreService->entries().begin();
       iter != self.tabRestoreService->entries().end(); ++iter) {
    const sessions::TabRestoreService::Entry* entry = iter->get();
    DCHECK(entry);
    DCHECK_EQ(sessions::TabRestoreService::TAB, entry->type);
    const sessions::TabRestoreService::Tab* tab =
        static_cast<const sessions::TabRestoreService::Tab*>(entry);
    const sessions::SerializedNavigationEntry& navigationEntry =
        tab->navigations[tab->current_navigation_index];

    // Configure and add the Item.
    TableViewURLItem* recentlyClosedTab =
        [[TableViewURLItem alloc] initWithType:ItemTypeRecentlyClosed];
    recentlyClosedTab.title = base::SysUTF16ToNSString(navigationEntry.title());
    recentlyClosedTab.URL = navigationEntry.virtual_url();
    [self.tableViewModel addItem:recentlyClosedTab
         toSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
  }
}

// Updates the recently closed tabs section by clobbering and reinserting
// section. Needs to be called inside a [UITableView beginUpdates] block on
// iOS10, or performBatchUpdates on iOS11+.
- (void)updateRecentlyClosedSection {
  [self.tableViewModel
      removeSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
  [self addRecentlyClosedSection];
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierRecentlyClosedTabs];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark Sessions Section

// Cleans up the model in order to update the Session sections. Needs to be
// called inside a [UITableView beginUpdates] block on iOS10, or
// performBatchUpdates on iOS11+.
- (void)updateSessionSections {
  SessionsSyncUserState previousState = self.sessionState;
  if (previousState !=
      SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) {
    // The previous state was one of the OtherDevices states, remove it.
    [self.tableView deleteSections:[self otherDevicesSectionIndexSet]
                  withRowAnimation:UITableViewRowAnimationNone];
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierOtherDevices];
  }

  // Clean up any previously added SessionSections.
  [self removeSessionSections];

  // Re-Add the session sections to |self.tableViewModel| and insert them into
  // |self.tableView|.
  [self addSessionSections];
  [self.tableView insertSections:[self sessionSectionIndexSet]
                withRowAnimation:UITableViewRowAnimationNone];
}

// Adds all the Remote Sessions sections with its respective items.
- (void)addSessionSections {
  TableViewModel* model = self.tableViewModel;
  for (NSUInteger i = 0; i < [self numberOfSessions]; i++) {
    synced_sessions::DistantSession const* session =
        _syncedSessions->GetSession(i);
    NSInteger sessionIdentifier = [self sectionIdentifierForSession:session];
    [model addSectionWithIdentifier:sessionIdentifier];
    NSString* sessionCollapsedKey = base::SysUTF8ToNSString(session->tag);
    [model setSectionIdentifier:sessionIdentifier
                   collapsedKey:sessionCollapsedKey];
    TableViewDisclosureHeaderFooterItem* header =
        [[TableViewDisclosureHeaderFooterItem alloc]
            initWithType:ItemTypeSessionHeader];
    header.text = base::SysUTF8ToNSString(session->name);
    header.subtitleText = l10n_util::GetNSStringF(
        IDS_IOS_OPEN_TABS_LAST_USED,
        base::SysNSStringToUTF16([self lastSyncStringForSesssion:session]));
    header.collapsed = [model sectionIsCollapsed:sessionIdentifier];
    [model setHeader:header forSectionWithIdentifier:sessionIdentifier];
    [self addItemsForSession:session];
  }
}

- (void)addItemsForSession:(synced_sessions::DistantSession const*)session {
  TableViewModel* model = self.tableViewModel;
  NSInteger numberOfTabs = base::checked_cast<NSInteger>(session->tabs.size());
  for (int i = 0; i < numberOfTabs; i++) {
    synced_sessions::DistantTab const* sessionTab = session->tabs[i].get();
    NSString* title = base::SysUTF16ToNSString(sessionTab->title);

    TableViewURLItem* sessionTabItem =
        [[TableViewURLItem alloc] initWithType:ItemTypeSessionTabData];
    sessionTabItem.title = title;
    sessionTabItem.URL = sessionTab->virtual_url;
    [model addItem:sessionTabItem
        toSectionWithIdentifier:[self sectionIdentifierForSession:session]];
  }
}

// Remove all SessionSections from |self.tableViewModel| and |self.tableView|
// Needs to be called inside a [UITableView beginUpdates] block on iOS10, or
// performBatchUpdates on iOS11+.
- (void)removeSessionSections {
  // |_syncedSessions| has been updated by now, that means that
  // |self.tableViewModel| does not reflect |_syncedSessions| data.
  NSInteger sectionIdentifierToRemove = kFirstSessionSectionIdentifier;
  NSInteger sectionToDelete = kNumberOfSectionsBeforeSessions;
  while ([self.tableViewModel numberOfSections] >
         kNumberOfSectionsBeforeSessions) {
    // A SectionIdentifier could've been deleted previously, do not rely on
    // these being in sequential order at this point.
    if ([self.tableViewModel
            hasSectionForSectionIdentifier:sectionIdentifierToRemove]) {
      [self.tableView
            deleteSections:[NSIndexSet indexSetWithIndex:sectionToDelete]
          withRowAnimation:UITableViewRowAnimationNone];
      sectionToDelete++;
      [self.tableViewModel
          removeSectionWithIdentifier:sectionIdentifierToRemove];
    }
    sectionIdentifierToRemove++;
  }
}

#pragma mark Other Devices Section

// Cleans up the model in order to update the Other devices section. Needs to be
// called inside a [UITableView beginUpdates] block on iOS10, or
// performBatchUpdates on iOS11+.
- (void)updateOtherDevicesSectionForState:(SessionsSyncUserState)newState {
  DCHECK_NE(newState,
            SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS);
  TableViewModel* model = self.tableViewModel;
  SessionsSyncUserState previousState = self.sessionState;
  if (previousState ==
      SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) {
    // There were previously one or more session sections, but now they will be
    // removed and replaced with a single OtherDevices section.
    [self removeSessionSections];
    [self addOtherDevicesSectionForState:newState];
    // This is a special situation where the tableview operation is an insert
    // rather than reload because the section was deleted.
    [self.tableView insertSections:[self otherDevicesSectionIndexSet]
                  withRowAnimation:UITableViewRowAnimationNone];
    return;
  }
  // For all other previous states, the tableview operation is a reload since
  // there is already an OtherDevices section that can be updated.
  [model removeSectionWithIdentifier:SectionIdentifierOtherDevices];
  [self addOtherDevicesSectionForState:newState];
  [self.tableView reloadSections:[self otherDevicesSectionIndexSet]
                withRowAnimation:UITableViewRowAnimationNone];
}

// Adds Other Devices Section and its header.
- (void)addOtherDevicesSectionForState:(SessionsSyncUserState)state {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierOtherDevices];
  [model setSectionIdentifier:SectionIdentifierOtherDevices
                 collapsedKey:kOtherDeviceCollapsedKey];
  // If user is not signed in, show disclosure view section header so that they
  // know they can collapse the signin prompt section
  if (state == SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS) {
    TableViewActivityIndicatorHeaderFooterItem* header =
        [[TableViewActivityIndicatorHeaderFooterItem alloc]
            initWithType:ItemTypeOtherDevicesSyncInProgressHeader];
    header.text = l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES);
    header.subtitleText =
        l10n_util::GetNSString(IDS_IOS_RECENT_TABS_SYNC_IN_PROGRESS);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierOtherDevices];
    return;
  } else {
    TableViewDisclosureHeaderFooterItem* header =
        [[TableViewDisclosureHeaderFooterItem alloc]
            initWithType:ItemTypeRecentlyClosedHeader];
    header.text = l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierOtherDevices];
    header.collapsed =
        [self.tableViewModel sectionIsCollapsed:SectionIdentifierOtherDevices];
  }

  // Adds Other Devices item for |state|.
  TableViewTextItem* dummyCell = nil;
  switch (state) {
    case SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS:
      NOTREACHED();
      return;
    case SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF:
      [self addUserSignedSyncOffItem];
      return;
    case SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS:
      dummyCell = [[TableViewTextItem alloc]
          initWithType:ItemTypeOtherDevicesNoSessions];
      dummyCell.text =
          l10n_util::GetNSString(IDS_IOS_OPEN_TABS_NO_SESSION_INSTRUCTIONS);
      break;
    case SessionsSyncUserState::USER_SIGNED_OUT:
      [self addSigninPromoViewItem];
      return;
    case SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS:
      // Informational text in section header. No need for a cell in the
      // section.
      NOTREACHED();
      return;
  }
  [self.tableViewModel addItem:dummyCell
       toSectionWithIdentifier:SectionIdentifierOtherDevices];
}

- (void)addUserSignedSyncOffItem {
  TableViewTextButtonItem* signinSyncOffItem = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeOtherDevicesSyncOff];
  signinSyncOffItem.text =
      l10n_util::GetNSString(IDS_IOS_OPEN_TABS_SYNC_IS_OFF_MOBILE);
  signinSyncOffItem.buttonText =
      l10n_util::GetNSString(IDS_IOS_OPEN_TABS_ENABLE_SYNC_MOBILE);
  [self.tableViewModel addItem:signinSyncOffItem
       toSectionWithIdentifier:SectionIdentifierOtherDevices];
}

- (void)addSigninPromoViewItem {
  // Init|_signinPromoViewMediator| if nil.
  if (!self.signinPromoViewMediator && self.browserState) {
    self.signinPromoViewMediator = [[SigninPromoViewMediator alloc]
        initWithBrowserState:self.browserState
                 accessPoint:signin_metrics::AccessPoint::
                                 ACCESS_POINT_RECENT_TABS
                   presenter:self];
    self.signinPromoViewMediator.consumer = self;
  }

  // Configure and add a TableViewSigninPromoItem to the model.
  TableViewSigninPromoItem* signinPromoItem = [[TableViewSigninPromoItem alloc]
      initWithType:ItemTypeOtherDevicesSigninPromo];
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    signinPromoItem.text =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_RECENT_TABS_WITH_UNITY);
  } else {
    signinPromoItem.text =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_RECENT_TABS);
  }
  signinPromoItem.delegate = self.signinPromoViewMediator;
  signinPromoItem.configurator =
      [self.signinPromoViewMediator createConfigurator];
  [self.tableViewModel addItem:signinPromoItem
       toSectionWithIdentifier:SectionIdentifierOtherDevices];
}

#pragma mark - TableViewModel Helpers

// Ordered array of all section identifiers.
- (NSArray*)allSessionSectionIdentifiers {
  NSMutableArray* allSessionSectionIdentifiers = [[NSMutableArray alloc] init];
  for (NSUInteger i = 0; i < [self numberOfSessions]; i++) {
    [allSessionSectionIdentifiers
        addObject:@(i + kFirstSessionSectionIdentifier)];
  }
  return allSessionSectionIdentifiers;
}

// Returns the TableViewModel SectionIdentifier for |distantSession|. Returns -1
// if |distantSession| doesn't exists.
- (NSInteger)sectionIdentifierForSession:
    (synced_sessions::DistantSession const*)distantSession {
  for (NSUInteger i = 0; i < [self numberOfSessions]; i++) {
    synced_sessions::DistantSession const* session =
        _syncedSessions->GetSession(i);
    if (session->tag == distantSession->tag)
      return i + kFirstSessionSectionIdentifier;
  }
  NOTREACHED();
  return -1;
}

// Returns an IndexSet containing the Other Devices Section.
- (NSIndexSet*)otherDevicesSectionIndexSet {
  NSUInteger otherDevicesSection = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierOtherDevices];
  return [NSIndexSet indexSetWithIndex:otherDevicesSection];
}

// Returns an IndexSet containing the all the Session Sections.
- (NSIndexSet*)sessionSectionIndexSet {
  // Create a range of all Session Sections.
  NSRange rangeOfSessionSections =
      NSMakeRange(kNumberOfSectionsBeforeSessions, [self numberOfSessions]);
  NSIndexSet* sessionSectionIndexes =
      [NSIndexSet indexSetWithIndexesInRange:rangeOfSessionSections];
  return sessionSectionIndexes;
}

// Returns YES if |sectionIdentifier| is a Sessions sectionIdentifier.
- (BOOL)isSessionSectionIdentifier:(NSInteger)sectionIdentifier {
  NSArray* sessionSectionIdentifiers = [self allSessionSectionIdentifiers];
  NSNumber* sectionIdentifierObject = @(sectionIdentifier);
  return [sessionSectionIdentifiers containsObject:sectionIdentifierObject];
}

#pragma mark - Consumer Protocol

- (void)refreshUserState:(SessionsSyncUserState)newSessionState {
  if ((newSessionState == self.sessionState &&
       self.sessionState !=
           SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) ||
      self.signinPromoViewMediator.isSigninInProgress) {
    // No need to refresh the sections since all states other than
    // USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS only have static content. This means
    // that if the previous State is the same as the new one the static content
    // won't change.
    return;
  }
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  _syncedSessions.reset(new synced_sessions::SyncedSessions(syncService));

  if (self.updatesTableView) {
    // Update the TableView and TableViewModel sections to match the new
    // sessionState.
    // Turn Off animations since UITableViewRowAnimationNone still animates.
    [UIView setAnimationsEnabled:NO];
    // If iOS11+ use performBatchUpdates: instead of begin/endUpdates.
    if (@available(iOS 11, *)) {
      if (newSessionState ==
          SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) {
        [self.tableView performBatchUpdates:^{
          [self updateSessionSections];
        }
                                 completion:nil];
      } else {
        [self.tableView performBatchUpdates:^{
          [self updateOtherDevicesSectionForState:newSessionState];
        }
                                 completion:nil];
      }
    } else {
      [self.tableView beginUpdates];
      if (newSessionState ==
          SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) {
        [self updateSessionSections];
      } else {
        [self updateOtherDevicesSectionForState:newSessionState];
      }
      [self.tableView endUpdates];
    }
    [UIView setAnimationsEnabled:YES];
  }

  // Table updates must happen before |sessionState| gets updated, since some
  // table updates rely on knowing the previous state.
  self.sessionState = newSessionState;
  if (self.sessionState != SessionsSyncUserState::USER_SIGNED_OUT) {
    [self.signinPromoViewMediator signinPromoViewRemoved];
    self.signinPromoViewMediator.consumer = nil;
    self.signinPromoViewMediator = nil;
  }
}

- (void)refreshRecentlyClosedTabs {
  if (!self.updatesTableView)
    return;

  if (@available(iOS 11, *)) {
    [self.tableView performBatchUpdates:^{
      [self updateRecentlyClosedSection];
    }
                             completion:nil];
  } else {
    [self.tableView beginUpdates];
    [self updateRecentlyClosedSection];
    [self.tableView endUpdates];
  }
}

- (void)setTabRestoreService:(sessions::TabRestoreService*)tabRestoreService {
  _tabRestoreService = tabRestoreService;
}

- (void)dismissModals {
  [self.contextMenuCoordinator stop];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  NSInteger itemTypeSelected =
      [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemTypeSelected) {
    case ItemTypeRecentlyClosed:
      [self
          openTabWithTabRestoreEntry:[self
                                         tabRestoreEntryAtIndexPath:indexPath]];
      break;
    case ItemTypeSessionTabData:
      [self
          openTabWithContentOfDistantTab:[self
                                             distantTabAtIndexPath:indexPath]];
      break;
    case ItemTypeShowFullHistory:
      [tableView deselectRowAtIndexPath:indexPath animated:NO];
      [self.presentationDelegate showHistoryFromRecentTabs];
      break;
    case ItemTypeOtherDevicesSyncOff:
    case ItemTypeOtherDevicesNoSessions:
    case ItemTypeOtherDevicesSigninPromo:
      break;
  }
}

// TODO(crbug.com/850814): Use only dynamic sizing once we stop supporting
// iOS10.
- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  DCHECK_EQ(tableView, self.tableView);
  if (@available(iOS 11, *)) {
    return UITableViewAutomaticDimension;
  } else {
    TableViewHeaderFooterItem* header =
        [self.tableViewModel headerForSection:section];
    return [header headerHeightForWidth:self.view.bounds.size.width];
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // If section is collapsed there's no need to add a separation space.
  return [self.tableViewModel
             sectionIsCollapsed:[self.tableViewModel
                                    sectionIdentifierForSection:section]]
             ? 1.0
             : kSeparationSpaceBetweenSections;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  NSInteger itemTypeSelected =
      [self.tableViewModel itemTypeForIndexPath:indexPath];
  // If SigninPromo will be shown, |self.signinPromoViewMediator| must know.
  if (itemTypeSelected == ItemTypeOtherDevicesSigninPromo) {
    [self.signinPromoViewMediator signinPromoViewVisible];
  }
  // Retrieve favicons for closed tabs and remote sessions.
  if (itemTypeSelected == ItemTypeRecentlyClosed ||
      itemTypeSelected == ItemTypeSessionTabData) {
    [self loadFaviconForCell:cell indexPath:indexPath];
  }
  // ItemTypeOtherDevicesNoSessions should not be selectable.
  if (itemTypeSelected == ItemTypeOtherDevicesNoSessions) {
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  // Set button action method for ItemTypeOtherDevicesSyncOff.
  if (itemTypeSelected == ItemTypeOtherDevicesSyncOff) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button addTarget:self
                                       action:@selector(updateSyncState)
                             forControlEvents:UIControlEventTouchUpInside];
  }

  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* header = [super tableView:tableView viewForHeaderInSection:section];
  // Set the header tag as the sectionIdentifer in order to recognize which
  // header was tapped.
  header.tag = [self.tableViewModel sectionIdentifierForSection:section];
  // Remove all existing gestureRecognizers since the header might be reused.
  for (UIGestureRecognizer* recognizer in header.gestureRecognizers) {
    [header removeGestureRecognizer:recognizer];
  }
  // Gesture recognizer for long press context menu.
  UILongPressGestureRecognizer* longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [header addGestureRecognizer:longPress];
  // Gesture recognizer for header collapsing/expanding.
  UITapGestureRecognizer* tapGesture =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  [header addGestureRecognizer:tapGesture];
  return header;
}

#pragma mark - Recently closed tab helpers

- (NSInteger)numberOfRecentlyClosedTabs {
  if (!self.tabRestoreService)
    return 0;
  return base::checked_cast<NSInteger>(
      self.tabRestoreService->entries().size());
}

- (const sessions::TabRestoreService::Entry*)tabRestoreEntryAtIndexPath:
    (NSIndexPath*)indexPath {
  DCHECK_EQ([self.tableViewModel sectionIdentifierForSection:indexPath.section],
            SectionIdentifierRecentlyClosedTabs);
  NSInteger index = indexPath.row;
  DCHECK_LE(index, [self numberOfRecentlyClosedTabs]);
  if (!self.tabRestoreService)
    return nullptr;

  // Advance the entry iterator to the correct index.
  // Note that std:list<> can only be accessed sequentially, which is
  // suboptimal when using Cocoa table APIs. This list doesn't appear
  // to get very long, so it probably won't matter for perf.
  sessions::TabRestoreService::Entries::const_iterator iter =
      self.tabRestoreService->entries().begin();
  std::advance(iter, index);
  CHECK(*iter);
  return iter->get();
}

// Retrieves favicon from FaviconLoader and sets image in URLCell.
- (void)loadFaviconForCell:(UITableViewCell*)cell
                 indexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  DCHECK(cell);

  TableViewURLItem* URLItem = base::mac::ObjCCastStrict<TableViewURLItem>(item);
  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);

  NSString* itemIdentifier = URLItem.uniqueIdentifier;
  FaviconAttributes* cachedAttributes = [self.imageDataSource
      faviconForURL:URLItem.URL
         completion:^(FaviconAttributes* attributes) {
           // Only set favicon if the cell hasn't been reused.
           if ([URLCell.cellUniqueIdentifier isEqualToString:itemIdentifier]) {
             DCHECK(attributes);
             [URLCell.faviconView configureWithAttributes:attributes];
           }
         }];
  DCHECK(cachedAttributes);
  [URLCell.faviconView configureWithAttributes:cachedAttributes];
}

#pragma mark - Distant Sessions helpers

- (NSUInteger)numberOfSessions {
  if (!_syncedSessions)
    return 0;
  return _syncedSessions->GetSessionCount();
}

// Returns the Session Index for a given Session Tab |indexPath|.
- (size_t)indexOfSessionForTabAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ([self.tableViewModel itemTypeForIndexPath:indexPath],
            ItemTypeSessionTabData);
  // Get the sectionIdentifier for |indexPath|,
  NSNumber* sectionIdentifierForIndexPath =
      @([self.tableViewModel sectionIdentifierForSection:indexPath.section]);
  // Get the index of this sectionIdentifier.
  size_t indexOfSession = [[self allSessionSectionIdentifiers]
      indexOfObject:sectionIdentifierForIndexPath];
  DCHECK_LT(indexOfSession, _syncedSessions->GetSessionCount());
  return indexOfSession;
}

- (synced_sessions::DistantSession const*)sessionForTabAtIndexPath:
    (NSIndexPath*)indexPath {
  return _syncedSessions->GetSession(
      [self indexOfSessionForTabAtIndexPath:indexPath]);
}

- (synced_sessions::DistantSession const*)sessionForSection:(NSInteger)section {
  NSInteger sectionIdentifer =
      [self.tableViewModel sectionIdentifierForSection:section];
  DCHECK([self isSessionSectionIdentifier:sectionIdentifer]);
  return _syncedSessions->GetSession(section - kNumberOfSectionsBeforeSessions);
}

- (synced_sessions::DistantTab const*)distantTabAtIndexPath:
    (NSIndexPath*)indexPath {
  DCHECK_EQ([self.tableViewModel itemTypeForIndexPath:indexPath],
            ItemTypeSessionTabData);
  size_t indexOfDistantTab = indexPath.row;
  synced_sessions::DistantSession const* session =
      [self sessionForTabAtIndexPath:indexPath];
  DCHECK_LT(indexOfDistantTab, session->tabs.size());
  return session->tabs[indexOfDistantTab].get();
}

- (NSString*)lastSyncStringForSesssion:
    (synced_sessions::DistantSession const*)session {
  base::Time time = session->modified_time;
  NSDate* lastUsedDate = [NSDate dateWithTimeIntervalSince1970:time.ToTimeT()];
  NSString* dateString =
      [NSDateFormatter localizedStringFromDate:lastUsedDate
                                     dateStyle:NSDateFormatterShortStyle
                                     timeStyle:NSDateFormatterNoStyle];

  NSString* timeString;
  base::TimeDelta last_used_delta;
  if (base::Time::Now() > time)
    last_used_delta = base::Time::Now() - time;

  if (last_used_delta.InMicroseconds() < base::Time::kMicrosecondsPerMinute) {
    timeString = l10n_util::GetNSString(IDS_IOS_OPEN_TABS_RECENTLY_SYNCED);
    // This will return something similar to "Seconds ago"
    return [NSString stringWithFormat:@"%@", timeString];
  }

  NSDate* date = [NSDate dateWithTimeIntervalSince1970:time.ToTimeT()];
  timeString =
      [NSDateFormatter localizedStringFromDate:date
                                     dateStyle:NSDateFormatterNoStyle
                                     timeStyle:NSDateFormatterShortStyle];

  NSInteger today = [[NSCalendar currentCalendar] component:NSCalendarUnitDay
                                                   fromDate:[NSDate date]];
  NSInteger dateDay =
      [[NSCalendar currentCalendar] component:NSCalendarUnitDay fromDate:date];

  if (today == dateDay) {
    timeString = base::SysUTF16ToNSString(
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                               ui::TimeFormat::LENGTH_SHORT, last_used_delta));
    // This will return something similar to "1 min/hour ago"
    return [NSString stringWithFormat:@"%@", timeString];
  }

  if (today - dateDay == 1) {
    dateString = l10n_util::GetNSString(IDS_IOS_OPEN_TABS_SYNCED_YESTERDAY);
    // This will return something similar to "H:MM Yesterday"
    return [NSString stringWithFormat:@"%@ %@", timeString, dateString];
  }

  // This will return something similar to "H:MM mm/dd/yy"
  return [NSString stringWithFormat:@"%@ %@", timeString, dateString];
}

#pragma mark - Navigation helpers

- (void)openTabWithContentOfDistantTab:
    (synced_sessions::DistantTab const*)distantTab {
  sync_sessions::OpenTabsUIDelegate* openTabs =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState)
          ->GetOpenTabsUIDelegate();
  const sessions::SessionTab* toLoad = nullptr;
  if (openTabs->GetForeignTab(distantTab->session_tag, distantTab->tab_id,
                              &toLoad)) {
    base::RecordAction(base::UserMetricsAction(
        "MobileRecentTabManagerTabFromOtherDeviceOpened"));
    new_tab_page_uma::RecordAction(
        self.browserState, new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);
    [self.loader loadSessionTab:toLoad];
  }
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

- (void)openTabWithTabRestoreEntry:
    (const sessions::TabRestoreService::Entry*)entry {
  // Only TAB type is handled.
  DCHECK_EQ(entry->type, sessions::TabRestoreService::TAB);
  base::RecordAction(
      base::UserMetricsAction("MobileRecentTabManagerRecentTabOpened"));
  new_tab_page_uma::RecordAction(
      self.browserState, new_tab_page_uma::ACTION_OPENED_RECENTLY_CLOSED_ENTRY);
  [self.loader restoreTabWithSessionID:entry->id];
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

#pragma mark - Collapse/Expand sections

- (void)handleTap:(UITapGestureRecognizer*)sender {
  UIView* headerTapped = sender.view;
  NSInteger tappedHeaderSectionIdentifier = headerTapped.tag;

  if (sender.state == UIGestureRecognizerStateEnded) {
    [self toggleExpansionOfSectionIdentifier:tappedHeaderSectionIdentifier];

    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:tappedHeaderSectionIdentifier];
    UITableViewHeaderFooterView* headerView =
        [self.tableView headerViewForSection:section];
    ListItem* headerItem = [self.tableViewModel headerForSection:section];
    // Highlight and collapse the section header being tapped.
    // Don't for the Loading Other Devices section header.
    if (headerItem.type == ItemTypeRecentlyClosedHeader ||
        headerItem.type == ItemTypeSessionHeader) {
      TableViewDisclosureHeaderFooterView* disclosureHeaderView =
          base::mac::ObjCCastStrict<TableViewDisclosureHeaderFooterView>(
              headerView);
      TableViewDisclosureHeaderFooterItem* disclosureItem =
          base::mac::ObjCCastStrict<TableViewDisclosureHeaderFooterItem>(
              headerItem);
      BOOL collapsed = [self.tableViewModel
          sectionIsCollapsed:[self.tableViewModel
                                 sectionIdentifierForSection:section]];
      DisclosureDirection direction =
          collapsed ? DisclosureDirectionUp : DisclosureDirectionDown;

      [disclosureHeaderView animateHighlightAndRotateToDirection:direction];
      disclosureItem.collapsed = collapsed;
    }
  }
}

- (void)toggleExpansionOfSectionIdentifier:(NSInteger)sectionIdentifier {
  NSMutableArray* cellIndexPathsToDeleteOrInsert = [NSMutableArray array];
  NSInteger sectionIndex =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  NSArray* items =
      [self.tableViewModel itemsInSectionWithIdentifier:sectionIdentifier];
  for (NSUInteger i = 0; i < [items count]; i++) {
    NSIndexPath* tabIndexPath =
        [NSIndexPath indexPathForRow:i inSection:sectionIndex];
    [cellIndexPathsToDeleteOrInsert addObject:tabIndexPath];
  }

  void (^tableUpdates)(void) = ^{
    if ([self.tableViewModel sectionIsCollapsed:sectionIdentifier]) {
      [self.tableViewModel setSection:sectionIdentifier collapsed:NO];
      [self.tableView insertRowsAtIndexPaths:cellIndexPathsToDeleteOrInsert
                            withRowAnimation:UITableViewRowAnimationFade];
    } else {
      [self.tableViewModel setSection:sectionIdentifier collapsed:YES];
      [self.tableView deleteRowsAtIndexPaths:cellIndexPathsToDeleteOrInsert
                            withRowAnimation:UITableViewRowAnimationFade];
    }
  };

  // If iOS11+ use performBatchUpdates: instead of beginUpdates/endUpdates.
  if (@available(iOS 11, *)) {
    [self.tableView performBatchUpdates:tableUpdates completion:nil];
  } else {
    [self.tableView beginUpdates];
    tableUpdates();
    [self.tableView endUpdates];
  }
}

#pragma mark - Long press and context menus

- (void)handleLongPress:(UILongPressGestureRecognizer*)sender {
  if (sender.state != UIGestureRecognizerStateBegan)
    return;
  UIView* headerTapped = sender.view;
  NSInteger tappedHeaderSectionIdentifier = headerTapped.tag;
  NSInteger sectionIdentifier = tappedHeaderSectionIdentifier;
  // Only handle LongPress for SessionHeaders.
  if (![self isSessionSectionIdentifier:sectionIdentifier])
    return;

  // Highlight the section header being long pressed.
  NSInteger section = [self.tableViewModel
      sectionForSectionIdentifier:tappedHeaderSectionIdentifier];
  ListItem* headerItem = [self.tableViewModel headerForSection:section];
  UITableViewHeaderFooterView* headerView =
      [self.tableView headerViewForSection:section];
  if (headerItem.type == ItemTypeRecentlyClosedHeader ||
      headerItem.type == ItemTypeSessionHeader) {
    TableViewDisclosureHeaderFooterView* textHeaderView =
        base::mac::ObjCCastStrict<TableViewDisclosureHeaderFooterView>(
            headerView);
    [textHeaderView animateHighlight];
  }

  web::ContextMenuParams params;
  // Get view coordinates in local space.
  CGPoint viewCoordinate = [sender locationInView:self.tableView];
  params.location = viewCoordinate;
  params.view = self.tableView;

  // Present sheet/popover using controller that is added to view hierarchy.
  // TODO(crbug.com/754642): Remove TopPresentedViewController().
  UIViewController* topController =
      top_view_controller::TopPresentedViewController();

  self.contextMenuCoordinator =
      [[ContextMenuCoordinator alloc] initWithBaseViewController:topController
                                                          params:params];

  // Fill the sheet/popover with buttons.
  __weak RecentTabsTableViewController* weakSelf = self;

  // "Open all tabs" button.
  NSString* openAllButtonLabel =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OPEN_ALL_MENU_OPTION);
  [self.contextMenuCoordinator
      addItemWithTitle:openAllButtonLabel
                action:^{
                  [weakSelf
                      openTabsFromSessionSectionIdentifier:sectionIdentifier];
                }];

  // "Hide for now" button.
  NSString* hideButtonLabel =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION);
  [self.contextMenuCoordinator
      addItemWithTitle:hideButtonLabel
                action:^{
                  [weakSelf removeSessionAtSessionSectionIdentifier:
                                sectionIdentifier];
                }];

  [self.contextMenuCoordinator start];
}

- (void)openTabsFromSessionSectionIdentifier:(NSInteger)sectionIdentifier {
  NSInteger section =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  synced_sessions::DistantSession const* session =
      [self sessionForSection:section];
  for (auto const& tab : session->tabs) {
    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:tab->virtual_url
                                      referrer:web::Referrer()
                                   inIncognito:NO
                                  inBackground:YES
                                      appendTo:kLastTab];

    [self.loader webPageOrderedOpen:command];
  }
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

- (void)removeSessionAtSessionSectionIdentifier:(NSInteger)sectionIdentifier {
  DCHECK([self isSessionSectionIdentifier:sectionIdentifier]);
  NSInteger section =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  synced_sessions::DistantSession const* session =
      [self sessionForSection:section];
  std::string sessionTagCopy = session->tag;
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  sync_sessions::OpenTabsUIDelegate* openTabs =
      syncService->GetOpenTabsUIDelegate();

  void (^tableUpdates)(void) = ^{
    [self.tableViewModel removeSectionWithIdentifier:sectionIdentifier];
    _syncedSessions->EraseSession(section - kNumberOfSectionsBeforeSessions);
    [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                  withRowAnimation:UITableViewRowAnimationLeft];
  };

  // If iOS11+ use performBatchUpdates: instead of beginUpdates/endUpdates.
  if (@available(iOS 11, *)) {
    [self.tableView performBatchUpdates:tableUpdates
                             completion:^(BOOL) {
                               openTabs->DeleteForeignSession(sessionTagCopy);
                             }];
  } else {
    [self.tableView beginUpdates];
    tableUpdates();
    // DeleteForeignSession will cause |self refreshUserState:| to be called,
    // thus refreshing the TableView, running this inside the updates block will
    // make sure that the tableView animations are performed in order.
    openTabs->DeleteForeignSession(sessionTagCopy);
    [self.tableView endUpdates];
  }
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  DCHECK(self.signinPromoViewMediator);
  // Update the TableViewSigninPromoItem configurator. It will be used by the
  // item to configure the cell once |self.tableView| requests a cell on
  // cellForRowAtIndexPath.
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeOtherDevicesSigninPromo
                              sectionIdentifier:SectionIdentifierOtherDevices];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  TableViewSigninPromoItem* signInItem =
      base::mac::ObjCCastStrict<TableViewSigninPromoItem>(item);
  signInItem.configurator = configurator;
  // If section is collapsed no tableView update is needed.
  if ([self.tableViewModel sectionIsCollapsed:SectionIdentifierOtherDevices]) {
    return;
  }
  // After setting the new configurator to the item, reload the item's Cell.
  [self reloadCellsForItems:@[ signInItem ]
           withRowAnimation:UITableViewRowAnimationNone];
}

- (void)signinDidFinish {
  [self.delegate refreshSessionsView];
}

#pragma mark - SyncPresenter

- (void)showReauthenticateSignin {
  [self.dispatcher
              showSignin:
                  [[ShowSigninCommand alloc]
                      initWithOperation:AUTHENTICATION_OPERATION_REAUTHENTICATE
                            accessPoint:signin_metrics::AccessPoint::
                                            ACCESS_POINT_UNKNOWN]
      baseViewController:self];
}

- (void)showSyncSettings {
  [self.dispatcher showSyncSettingsFromViewController:self];
}

- (void)showSyncPassphraseSettings {
  [self.dispatcher showSyncPassphraseSettingsFromViewController:self];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command baseViewController:self];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
  return YES;
}

#pragma mark - Private Helpers

- (void)updateSyncState {
  SyncSetupService::SyncServiceState syncState =
      GetSyncStateForBrowserState(_browserState);
  if (ShouldShowSyncSignin(syncState)) {
    [self showReauthenticateSignin];
  } else if (ShouldShowSyncSettings(syncState)) {
    [self showSyncSettings];
  } else if (ShouldShowSyncPassphraseSettings(syncState)) {
    [self showSyncPassphraseSettings];
  }
}

@end
