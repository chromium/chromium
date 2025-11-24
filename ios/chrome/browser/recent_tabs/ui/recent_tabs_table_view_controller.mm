// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_table_view_controller.h"

#import <objc/runtime.h>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/sessions/core/session_id.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_recent_tabs_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo_view_mediator.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/recent_tabs/public/recent_tabs_constants.h"
#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/sessions/model/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_activity_indicator_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_disclosure_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_illustrated_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"

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
  ItemTypeOtherDevicesSyncOff,
  ItemTypeOtherDevicesNoSessions,
  ItemTypeOtherDevicesSignedOut,
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
// Estimated Table Row height.
const CGFloat kEstimatedRowHeight = 56;
// Separation space between sections.
const CGFloat kSeparationSpaceBetweenSections = 9;
// Section index for recently closed tabs.
const int kRecentlyClosedTabsSectionIndex = 0;

// A pair representing a single recently closed item. The `TableViewURLItem` is
// used to display the item and the `SessionID` is used to restore the item if
// selected by the user.
typedef std::pair<SessionID, TableViewURLItem*> RecentlyClosedTableViewItemPair;

}  // namespace

@interface ListModelCollapsedSceneSessionMediator : ListModelCollapsedMediator
// Creates a collapsed section mediator that stores data in the session's
// userInfo instead of NSUserDefaults, which allows different states per window.
- (instancetype)initWithSession:(UISceneSession*)session;
@end

@interface RecentTabsTableViewController () <
    SigninPromoViewConsumer,
    SigninPromoViewMediatorDelegate,
    SyncObserverModelBridge,
    SyncPresenter,
    TableViewURLDragDataSource,
    TrustedVaultReauthenticationCoordinatorDelegate,
    UIContextMenuInteractionDelegate,
    UIGestureRecognizerDelegate> {
  // The displayed recently closed tabs.
  std::vector<RecentlyClosedTableViewItemPair> _recentlyClosedItems;

  // The instance which owns the DistantTabs to display.
  std::unique_ptr<synced_sessions::SyncedSessions> _syncedSessions;
  // The displayed sessions and tabs. The sessions and tabs are owned by
  // `_syncedSessions`, but `_displayedTabs` allows for filtering to display
  // only particular tabs.
  std::vector<synced_sessions::DistantTabsSet> _displayedTabs;

  std::unique_ptr<SyncObserverBridge> _syncObserver;
  TrustedVaultReauthenticationCoordinator*
      _trustedVaultReauthenticationCoordinator;
}
// The service that manages the recently closed tabs
@property(nonatomic, assign) sessions::TabRestoreService* tabRestoreService;
// The sync state.
@property(nonatomic, assign) SessionsSyncUserState sessionState;
// Mediator in charge of inviting the user to sign-in with a Google account.
@property(nonatomic, strong) SigninPromoViewMediator* signinPromoViewMediator;
// The browser state used for many operations, derived from the one provided by
// `self.browser`.
@property(nonatomic, readonly) ProfileIOS* profile;
// YES if this ViewController is being presented on incognito mode.
@property(nonatomic, readonly, getter=isIncognito) BOOL incognito;
// Convenience getter for `self.browser`'s WebStateList
@property(nonatomic, readonly) WebStateList* webStateList;
// Handler for URL drag interactions.
@property(nonatomic, strong) TableViewURLDragDropHandler* dragDropHandler;
@end

@implementation RecentTabsTableViewController {
  SigninCoordinator* _signinCoordinator;
}

#pragma mark - Public Interface

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _sessionState = SessionsSyncUserState::USER_SIGNED_OUT;
    _syncedSessions.reset(new synced_sessions::SyncedSessions());
    _preventUpdates = YES;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kRecentTabsTableViewControllerAccessibilityIdentifier;
  [self.tableView setDelegate:self];
  self.tableView.cellLayoutMarginsFollowReadableWidth = NO;
  self.tableView.estimatedRowHeight = kEstimatedRowHeight;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionFooterHeight = 0.0;
  self.title = l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS);

  self.dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityRecentTabsOrigin;
  self.dragDropHandler.dragDataSource = self;
  self.tableView.dragDelegate = self.dragDropHandler;
  self.tableView.dragInteractionEnabled = true;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (!self.preventUpdates) {
    // The table view might get stale while hidden, so we need to forcibly
    // refresh it here.
    [self loadModel];
    [self.tableView reloadData];
  }
}

#pragma mark - Setters & Getters

- (void)setBrowser:(Browser*)browser {
  _browser = browser;
  if (browser) {
    ProfileIOS* profile = browser->GetProfile();
    // Some RecentTabs services depend on objects not present in the
    // OffTheRecord profile, in order to prevent crashes set
    // `_profile` to `profile->OriginalProfile`. While
    // doing this check if incognito or not so that pages are loaded
    // accordingly.
    _profile = profile->GetOriginalProfile();
    _incognito = profile->IsOffTheRecord();
    _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
  } else {
    _syncObserver.reset();
  }
}

- (WebStateList*)webStateList {
  return self.browser->GetWebStateList();
}

- (void)setPreventUpdates:(BOOL)preventUpdates {
  if (_preventUpdates == preventUpdates) {
    return;
  }

  _preventUpdates = preventUpdates;

  if (preventUpdates) {
    return;
  }
  [self loadModel];
  [self.tableView reloadData];
}

- (syncer::SyncService*)syncService {
  DCHECK(_profile);
  return SyncServiceFactory::GetForProfile(_profile);
}

// Returns YES if the user cannot turn on sync for enterprise policy reasons.
- (BOOL)isSyncDisabledByAdministrator {
  DCHECK(self.syncService);
  if (self.syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // Return YES if the SyncDisabled policy is enabled.
    return YES;
  }

  if (self.syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kTabs) ||
      self.syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kHistory)) {
    // Return YES if the data type is disabled by the SyncTypesListDisabled
    // policy.
    return YES;
  }

  DCHECK(self.profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  DCHECK(authService);
  // Return NO is sign-in is disabled by the BrowserSignin policy.
  return authService->GetServiceStatus() ==
         AuthenticationService::ServiceStatus::SigninDisabledByPolicy;
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (self.preventUpdates ||
      ![self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOtherDevices]) {
    return;
  }

  [self.tableView
      performBatchUpdates:^{
        [self updateOtherDevicesSectionForState:self.sessionState];
      }
               completion:nil];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  if (self.session) {
    // Replace mediator to store collapsed keys in scene session.
    self.tableViewModel.collapsableMediator =
        [[ListModelCollapsedSceneSessionMediator alloc]
            initWithSession:self.session];
  }

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
  if (!self.tabRestoreService || self.tabRestoreService->entries().empty()) {
    header.subtitleText =
        l10n_util::GetNSString(IDS_IOS_RECENT_TABS_RECENTLY_CLOSED_EMPTY);
  }
  [model setHeader:header
      forSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
  header.collapsed = [self.tableViewModel
      sectionIsCollapsed:SectionIdentifierRecentlyClosedTabs];

  // Add Recently Closed Tabs Cells.
  [self addRecentlyClosedTabItems];

  // Add show full history item last.
  TableViewImageItem* historyItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeShowFullHistory];
  historyItem.title = l10n_util::GetNSString(IDS_HISTORY_SHOWFULLHISTORY_LINK);

  historyItem.image =
      DefaultSymbolWithPointSize(kHistorySymbol, kSymbolActionPointSize);
  historyItem.textColor = [UIColor colorNamed:kBlueColor];
  historyItem.accessibilityIdentifier =
      kRecentTabsShowFullHistoryCellAccessibilityIdentifier;
  historyItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:historyItem
      toSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
}

// Iterates through all the TabRestoreService entries and adds items to the
// recently closed tabs section. This method performs no UITableView operations.
- (void)addRecentlyClosedTabItems {
  if (!self.tabRestoreService) {
    return;
  }

  // A manual item refresh is necessary.
  std::vector<RecentlyClosedTableViewItemPair> recentlyClosedItems;
  for (auto iter = self.tabRestoreService->entries().begin();
       iter != self.tabRestoreService->entries().end(); ++iter) {
    const sessions::tab_restore::Entry* entry = iter->get();
    DCHECK(entry);
    // Only TAB type is handled.
    // TODO(crbug.com/40676931) : Support WINDOW restoration under
    // multi-window.
    DCHECK_EQ(sessions::tab_restore::Type::TAB, entry->type);

    const sessions::tab_restore::Tab* tab =
        static_cast<const sessions::tab_restore::Tab*>(entry);
    const sessions::SerializedNavigationEntry& navigationEntry =
        tab->navigations[tab->current_navigation_index];

    TableViewURLItem* recentlyClosedTab =
        [[TableViewURLItem alloc] initWithType:ItemTypeRecentlyClosed];
    recentlyClosedTab.title = base::SysUTF16ToNSString(navigationEntry.title());
    recentlyClosedTab.URL =
        [[CrURL alloc] initWithGURL:navigationEntry.virtual_url()];

    RecentlyClosedTableViewItemPair item(entry->id, recentlyClosedTab);
    recentlyClosedItems.push_back(item);
  }
  _recentlyClosedItems = recentlyClosedItems;

  for (const RecentlyClosedTableViewItemPair& item : _recentlyClosedItems) {
    [self.tableViewModel addItem:item.second
         toSectionWithIdentifier:SectionIdentifierRecentlyClosedTabs];
  }
}

// Updates the recently closed tabs section by clobbering and reinserting
// section. Needs to be called inside a performBatchUpdates block.
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
// called inside a performBatchUpdates block.
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

  // Re-Add the session sections to `self.tableViewModel` and insert them into
  // `self.tableView`.
  [self addSessionSections];
  [self.tableView insertSections:[self sessionSectionIndexSet]
                withRowAnimation:UITableViewRowAnimationNone];
}

// Adds all the Remote Sessions sections with its respective items.
- (void)addSessionSections {
  TableViewModel* model = self.tableViewModel;
  for (NSUInteger i = 0; i < [self numberOfSessions]; i++) {
    synced_sessions::DistantSession const* session =
        _syncedSessions->GetSessionWithTag(_displayedTabs[i].session_tag);
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
  const synced_sessions::DistantTabsSet* session_tabs_set =
      [self distantTabsSetForSessionWithTag:session->tag];

  if (!session_tabs_set) {
    return;
  }

  NSInteger sectionIdentifier = [self sectionIdentifierForSession:session];
  if (session_tabs_set->filtered_tabs) {
    // Only add the items from `filtered_tabs`.
    for (synced_sessions::DistantTab* sessionTab :
         session_tabs_set->filtered_tabs.value()) {
      [self addItemForDistantTab:sessionTab
          toSectionWithIdentifier:sectionIdentifier];
    }
  } else {
    // When `filtered_tabs` is null, all tabs in the session are included
    // in the set.
    for (auto&& sessionTab : session->tabs) {
      [self addItemForDistantTab:sessionTab.get()
          toSectionWithIdentifier:sectionIdentifier];
    }
  }
}

- (void)addItemForDistantTab:(synced_sessions::DistantTab*)sessionTab
     toSectionWithIdentifier:(NSInteger)sectionIdentifier {
  NSString* title = base::SysUTF16ToNSString(sessionTab->title);

  TableViewURLItem* sessionTabItem =
      [[TableViewURLItem alloc] initWithType:ItemTypeSessionTabData];
  sessionTabItem.title = title;
  sessionTabItem.URL = [[CrURL alloc] initWithGURL:sessionTab->virtual_url];
  [self.tableViewModel addItem:sessionTabItem
       toSectionWithIdentifier:sectionIdentifier];
}

// Remove all SessionSections from `self.tableViewModel` and `self.tableView`
// Needs to be called inside a performBatchUpdates block.
- (void)removeSessionSections {
  NSMutableIndexSet* indexesToBeDeleted = [NSMutableIndexSet indexSet];
  NSMutableIndexSet* sectionIdentifiersToBeDeleted =
      [NSMutableIndexSet indexSet];
  for (NSInteger index = 0; index < [self.tableViewModel numberOfSections];
       index++) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:index];
    if (sectionIdentifier >= kFirstSessionSectionIdentifier) {
      [sectionIdentifiersToBeDeleted addIndex:sectionIdentifier];
      [indexesToBeDeleted addIndex:index];
    }
  }

  [sectionIdentifiersToBeDeleted
      enumerateIndexesUsingBlock:^(NSUInteger sectionIdentifier, BOOL* stop) {
        [self.tableViewModel removeSectionWithIdentifier:sectionIdentifier];
      }];
  [self.tableView deleteSections:indexesToBeDeleted
                withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark Other Devices Section

// Cleans up the model in order to update the Other devices section. Needs to be
// called inside a performBatchUpdates block.
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
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  const AuthenticationService::ServiceStatus authServiceStatus =
      authService->GetServiceStatus();
  // If sign-in is disabled through user Settings, do not show Other Devices
  // section. However, if sign-in is disabled by policy Chrome will
  // continue to show the Other Devices section with a specialized message.
  switch (authServiceStatus) {
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return;
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
  }

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
    if (self.isSyncDisabledByAdministrator) {
      header.disabled = YES;
      header.subtitleText =
          l10n_util::GetNSString(IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION);
    }
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierOtherDevices];
    header.collapsed =
        [self.tableViewModel sectionIsCollapsed:SectionIdentifierOtherDevices];
  }

  if (!self.isSyncDisabledByAdministrator &&
      authServiceStatus !=
          AuthenticationService::ServiceStatus::SigninDisabledByPolicy) {
    ItemType itemType;
    NSString* itemSubtitle;
    NSString* itemButtonText;
    switch (state) {
      case SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS:
      case SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS:
        NOTREACHED();

      case SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF:
        itemType = ItemTypeOtherDevicesSyncOff;
        itemSubtitle =
            l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES_LABEL);
        itemButtonText = l10n_util::GetNSString(
            IDS_IOS_RECENT_TABS_OTHER_DEVICES_TURN_ON_TABS);
        break;

      case SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS:
        itemType = ItemTypeOtherDevicesNoSessions;
        itemSubtitle = l10n_util::GetNSString(
            IDS_IOS_RECENT_TABS_OTHER_DEVICES_EMPTY_MESSAGE);
        break;

      case SessionsSyncUserState::USER_SIGNED_OUT:
        [self addSigninPromoViewItem];
        itemType = ItemTypeOtherDevicesSignedOut;
        itemSubtitle =
            l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES_LABEL);
        break;
    }
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES_EMPTY_TITLE);
    NSString* accessibilityId =
        kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier;
    TableViewIllustratedItem* illustratedItem = [self
        createIllustratedItemWithType:itemType
                                image:[UIImage imageNamed:@"recent_tabs_other_"
                                                          @"devices_empty"]
                                title:title
                             subtitle:itemSubtitle
                           buttonText:itemButtonText
              accessibilityIdentifier:accessibilityId];
    [self.tableViewModel insertItem:illustratedItem
            inSectionWithIdentifier:SectionIdentifierOtherDevices
                            atIndex:0];
  }
}

- (TableViewIllustratedItem*)createIllustratedItemWithType:(ItemType)type
                                                     image:(UIImage*)image
                                                     title:(NSString*)title
                                                  subtitle:(NSString*)subtitle
                                                buttonText:(NSString*)buttonText
                                   accessibilityIdentifier:
                                       (NSString*)accessibilityIdentifier {
  TableViewIllustratedItem* illustratedItem =
      [[TableViewIllustratedItem alloc] initWithType:type];
  illustratedItem.image = image;
  illustratedItem.title = title;
  illustratedItem.subtitle = subtitle;
  illustratedItem.buttonText = buttonText;
  illustratedItem.accessibilityIdentifier = accessibilityIdentifier;
  return illustratedItem;
}

- (void)addSigninPromoViewItem {
  // Init `_signinPromoViewMediator` if nil.
  ProfileIOS* profile = self.profile;
  if (!self.signinPromoViewMediator && profile) {
    self.signinPromoViewMediator = [[SigninPromoViewMediator alloc]
                  initWithIdentityManager:IdentityManagerFactory::GetForProfile(
                                              profile)
                    accountManagerService:ChromeAccountManagerServiceFactory::
                                              GetForProfile(profile)
                              authService:AuthenticationServiceFactory::
                                              GetForProfile(profile)
                              prefService:profile->GetPrefs()
                              syncService:self.syncService
                              accessPoint:signin_metrics::AccessPoint::
                                              kRecentTabs
                                 delegate:self
                 accountSettingsPresenter:nil
        changeProfileContinuationProvider:
            base::BindRepeating(&CreateChangeProfileRecentTabsContinuation)];
    self.signinPromoViewMediator.signinPromoAction =
        SigninPromoAction::kSigninWithNoDefaultIdentity;
    self.signinPromoViewMediator.consumer = self;
  }

  // Configure and add a TableViewSigninPromoItem to the model.
  TableViewSigninPromoItem* signinPromoItem = [[TableViewSigninPromoItem alloc]
      initWithType:ItemTypeOtherDevicesSigninPromo];
  signinPromoItem.text =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_RECENT_TABS_WITH_UNITY);
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

// Returns the TableViewModel SectionIdentifier for `distantSession`. Returns -1
// if `distantSession` doesn't exists.
- (NSInteger)sectionIdentifierForSession:
    (synced_sessions::DistantSession const*)distantSession {
  for (NSUInteger i = 0; i < [self numberOfSessions]; i++) {
    if (_displayedTabs[i].session_tag == distantSession->tag) {
      return i + kFirstSessionSectionIdentifier;
    }
  }
  NOTREACHED();
}

// Returns an IndexSet containing the Other Devices Section.
- (NSIndexSet*)otherDevicesSectionIndexSet {
  NSUInteger otherDevicesSection = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierOtherDevices];
  return [NSIndexSet indexSetWithIndex:otherDevicesSection];
}

// Returns an IndexSet containing all the Session Sections.
- (NSIndexSet*)sessionSectionIndexSet {
  // Create a range of all Session Sections.
  NSRange rangeOfSessionSections =
      NSMakeRange([self firstSessionSectionIndex], [self numberOfSessions]);
  NSIndexSet* sessionSectionIndexes =
      [NSIndexSet indexSetWithIndexesInRange:rangeOfSessionSections];
  return sessionSectionIndexes;
}

- (NSInteger)firstSessionSectionIndex {
  return 1;
}

#pragma mark - Public

- (synced_sessions::DistantSession const*)sessionForTableSectionWithIdentifier:
    (NSInteger)sectionIdentifer {
  NSInteger section =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifer];
  DCHECK([self isSessionSectionIdentifier:sectionIdentifer]);
  const synced_sessions::DistantTabsSet& tabsSet =
      _displayedTabs[section - [self firstSessionSectionIndex]];
  return _syncedSessions->GetSessionWithTag(tabsSet.session_tag);
}

- (void)removeSessionAtTableSectionWithIdentifier:(NSInteger)sectionIdentifier {
  DCHECK([self isSessionSectionIdentifier:sectionIdentifier]);

  // Save the sessionTag before removing it from the table. It will be needed to
  // delete the session later.
  synced_sessions::DistantSession const* session =
      [self sessionForTableSectionWithIdentifier:sectionIdentifier];
  std::string sessionTag = session->tag;

  // Remove the section and, on completion, the delete the session.
  __weak __typeof(self) weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        [weakSelf removeSection:sectionIdentifier forSessionWithTag:sessionTag];
      }
      completion:^(BOOL) {
        [weakSelf deleteSession:sessionTag];
      }];
}

// Helper to set the distant tabs to be displayed. The tabs referenced in
// `displayedTabs` must be owned by `syncedSessions`.
- (void)setSyncedSessions:
            (std::unique_ptr<synced_sessions::SyncedSessions>)syncedSessions
       distantSessionTabs:
           (std::vector<synced_sessions::DistantTabsSet>)displayedTabs {
  _syncedSessions = std::move(syncedSessions);
  _displayedTabs = displayedTabs;
}

// Helper to set the recently closed items vector.
- (void)setRecentlyClosedItems:
    (std::vector<RecentlyClosedTableViewItemPair>)recentlyClosedItems {
  _recentlyClosedItems = recentlyClosedItems;
}

// Helper for removeSessionAtTableSectionWithIdentifier
- (void)removeSection:(NSInteger)sectionIdentifier
    forSessionWithTag:(std::string)sessionTag {
  NSInteger sectionIndex =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  [self.tableViewModel removeSectionWithIdentifier:sectionIdentifier];

  for (NSUInteger i = 0; i < _displayedTabs.size(); i++) {
    if (sessionTag == _displayedTabs[i].session_tag) {
      _displayedTabs.erase(_displayedTabs.begin() + i);
      break;
    }
  }
  _syncedSessions->EraseSessionWithTag(sessionTag);

  [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                withRowAnimation:UITableViewRowAnimationLeft];
}

// Helper for removeSessionAtTableSectionWithIdentifier
- (void)deleteSession:(std::string)sessionTag {
  SessionSyncServiceFactory::GetForProfile(self.profile)
      ->GetOpenTabsUIDelegate()
      ->DeleteForeignSession(sessionTag);
}

#pragma mark - Private

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

// Returns YES if `sectionIdentifier` is a Sessions sectionIdentifier.
- (BOOL)isSessionSectionIdentifier:(NSInteger)sectionIdentifier {
  NSArray* sessionSectionIdentifiers = [self allSessionSectionIdentifiers];
  NSNumber* sectionIdentifierObject = @(sectionIdentifier);
  return [sessionSectionIdentifiers containsObject:sectionIdentifierObject];
}

// Returns YES if the recent tabs is presented modally.
- (BOOL)isPresentedModally {
  return self.navigationController.presentingViewController;
}

#pragma mark - Consumer Protocol

- (void)refreshUserState:(SessionsSyncUserState)newSessionState {
  if ((newSessionState == self.sessionState &&
       self.sessionState !=
           SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) ||
      self.signinPromoViewMediator.showSpinner) {
    // No need to refresh the sections since all states other than
    // USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS only have static content. This means
    // that if the previous State is the same as the new one the static content
    // won't change.
    return;
  }

  // A manual item refresh is necessary.
  sync_sessions::SessionSyncService* syncService =
      SessionSyncServiceFactory::GetForProfile(self.profile);
  auto syncedSessions =
      std::make_unique<synced_sessions::SyncedSessions>(syncService);

  std::vector<synced_sessions::DistantTabsSet> displayedTabs;
  for (size_t s = 0; s < syncedSessions->GetSessionCount(); s++) {
    const synced_sessions::DistantSession* session =
        syncedSessions->GetSession(s);

    synced_sessions::DistantTabsSet distant_tabs;
    distant_tabs.session_tag = session->tag;
    displayedTabs.push_back(distant_tabs);
  }

  // Reset `_displayedTabs` to contain all sessions and tabs.
  [self setSyncedSessions:std::move(syncedSessions)
       distantSessionTabs:displayedTabs];

  if (!self.preventUpdates) {
    // Update the TableView and TableViewModel sections to match the new
    // sessionState.
    // Turn Off animations since UITableViewRowAnimationNone still animates.
    const BOOL animationsWereEnabled = [UIView areAnimationsEnabled];
    [UIView setAnimationsEnabled:NO];
    if (newSessionState ==
        SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS) {
      [self.tableView
          performBatchUpdates:^{
            [self updateSessionSections];
          }
                   completion:nil];
    } else {
      [self.tableView
          performBatchUpdates:^{
            [self updateOtherDevicesSectionForState:newSessionState];
          }
                   completion:nil];
    }
    [UIView setAnimationsEnabled:animationsWereEnabled];
  }

  // Table updates must happen before `sessionState` gets updated, since some
  // table updates rely on knowing the previous state.
  self.sessionState = newSessionState;

  if (self.sessionState != SessionsSyncUserState::USER_SIGNED_OUT) {
    [self disconnectMediator];
  }
}

- (void)refreshRecentlyClosedTabs {
  if (self.preventUpdates) {
    return;
  }

  [self.tableView
      performBatchUpdates:^{
        [self updateRecentlyClosedSection];
      }
               completion:nil];
}

- (void)setTabRestoreService:(sessions::TabRestoreService*)tabRestoreService {
  _tabRestoreService = tabRestoreService;
}

- (void)dismissModals {
  [self disconnectMediator];
  [self stopSigninCoordinator];
  [self.tableView.contextMenuInteraction dismissMenu];
  [self stopTrustedVaultReauthenticationCoordinator];
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
      [self openTabWithTabRestoreEntryId:
                [self tabRestoreEntryIdAtIndexPath:indexPath]];
      break;
    case ItemTypeSessionTabData:
      [self
          openTabWithContentOfDistantTab:[self
                                             distantTabAtIndexPath:indexPath]];
      break;
    case ItemTypeShowFullHistory:
      base::RecordAction(
          base::UserMetricsAction("MobileRecentTabManagerShowFullHistory"));
      [tableView deselectRowAtIndexPath:indexPath animated:NO];

      // Tapping "show full history" attempts to dismiss recent tabs to show the
      // history UI. It is reasonable to ignore this if a modal UI is already
      // showing above recent tabs. This can happen when a user simultaneously
      // taps "show full history" and "enable sync". The sync settings UI
      // appears first and we should not dismiss it to display history.
      if (!self.presentedViewController) {
        [self.presentationDelegate showHistoryFromRecentTabs];
      }
      break;
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  DCHECK_EQ(tableView, self.tableView);
  return UITableViewAutomaticDimension;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // If section is collapsed there's no need to add a separation space.
  return [self.tableViewModel
             sectionIsCollapsed:[self.tableViewModel
                                    sectionIdentifierForSectionIndex:section]]
             ? 1.0
             : kSeparationSpaceBetweenSections;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemTypeSelected =
      [self.tableViewModel itemTypeForIndexPath:indexPath];

  // Retrieve favicons for closed tabs and remote sessions.
  if (itemTypeSelected == ItemTypeRecentlyClosed ||
      itemTypeSelected == ItemTypeSessionTabData) {
    [self loadFaviconForIndexPath:indexPath];
  }

  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  // If SigninPromo will be shown, `self.signinPromoViewMediator` must know.
  if (itemTypeSelected == ItemTypeOtherDevicesSigninPromo) {
    [self.signinPromoViewMediator signinPromoViewIsVisible];
    TableViewSigninPromoCell* signinPromoCell =
        base::apple::ObjCCastStrict<TableViewSigninPromoCell>(cell);
    TableViewSigninPromoItem* signinPromoItem =
        base::apple::ObjCCastStrict<TableViewSigninPromoItem>(
            [self.tableViewModel itemAtIndexPath:indexPath]);
    [signinPromoItem.configurator
        configureSigninPromoView:signinPromoCell.signinPromoView
                       withStyle:SigninPromoViewStyleOnlyButton];
    // Disable animations when setting the background color to prevent flash on
    // rotation.
    const BOOL animationsWereEnabled = [UIView areAnimationsEnabled];
    [UIView setAnimationsEnabled:NO];
    signinPromoCell.backgroundColor = nil;
    [UIView setAnimationsEnabled:animationsWereEnabled];
  }
  // ItemTypeOtherDevicesNoSessions should not be selectable.
  if (itemTypeSelected == ItemTypeOtherDevicesNoSessions) {
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  // Set button action method for ItemTypeOtherDevicesSyncOff.
  if (itemTypeSelected == ItemTypeOtherDevicesSyncOff) {
    TableViewIllustratedCell* illustratedCell =
        base::apple::ObjCCastStrict<TableViewIllustratedCell>(cell);
    [illustratedCell.button addTarget:self
                               action:@selector(didTapPromoActionButton)
                     forControlEvents:UIControlEventTouchUpInside];
    illustratedCell.button.accessibilityIdentifier =
        kRecentTabsTabSyncOffButtonAccessibilityIdentifier;
  }
  // Hide the separator between this cell and the SignIn Promo.
  if (itemTypeSelected == ItemTypeOtherDevicesSignedOut) {
    cell.separatorInset =
        UIEdgeInsetsMake(0, self.tableView.bounds.size.width, 0, 0);
  }
  [cell layoutIfNeeded];
  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* header = [super tableView:tableView viewForHeaderInSection:section];
  // Set the header tag as the sectionIdentifer in order to recognize which
  // header was tapped.
  header.tag = [self.tableViewModel sectionIdentifierForSectionIndex:section];
  // Remove all existing gestureRecognizers since the header might be reused.
  for (UIGestureRecognizer* recognizer in header.gestureRecognizers) {
    [header removeGestureRecognizer:recognizer];
  }

  // Gesture recognizer for long press context menu.
  [header
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];

  // Gesture recognizer for header collapsing/expanding.
  UITapGestureRecognizer* tapGesture =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  [header addGestureRecognizer:tapGesture];
  return header;
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType != ItemTypeRecentlyClosed &&
      itemType != ItemTypeSessionTabData) {
    return nil;
  }

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  TableViewURLItem* URLItem =
      base::apple::ObjCCastStrict<TableViewURLItem>(item);

  return [self.menuProvider
      contextMenuConfigurationForItem:URLItem
                             fromView:[tableView
                                          cellForRowAtIndexPath:indexPath]];
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  UIView* header = [interaction view];
  NSInteger tappedHeaderSectionIdentifier = header.tag;

  if (![self isSessionSectionIdentifier:tappedHeaderSectionIdentifier]) {
    return [[UIContextMenuConfiguration alloc] init];
  }

  return
      [self.menuProvider contextMenuConfigurationForHeaderWithSectionIdentifier:
                             tappedHeaderSectionIdentifier];
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeRecentlyClosed:
    case ItemTypeSessionTabData: {
      TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
      TableViewURLItem* URLItem =
          base::apple::ObjCCastStrict<TableViewURLItem>(item);
      GURL gurl;
      if (URLItem.URL) {
        gurl = URLItem.URL.gurl;
      }
      return [[URLInfo alloc] initWithURL:gurl title:URLItem.title];
    }

    case ItemTypeRecentlyClosedHeader:
    case ItemTypeOtherDevicesSyncOff:
    case ItemTypeOtherDevicesNoSessions:
    case ItemTypeOtherDevicesSigninPromo:
    case ItemTypeOtherDevicesSyncInProgressHeader:
    case ItemTypeSessionHeader:
    case ItemTypeShowFullHistory:
      break;
  }
  return nil;
}

#pragma mark - Recently closed tab helpers

- (NSInteger)numberOfRecentlyClosedTabs {
  if (!self.tabRestoreService) {
    return 0;
  }
  return _recentlyClosedItems.size();
}

- (const SessionID)tabRestoreEntryIdAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section],
      SectionIdentifierRecentlyClosedTabs);
  NSInteger index = indexPath.row;
  DCHECK_LE(index, [self numberOfRecentlyClosedTabs]);
  if (!self.tabRestoreService) {
    return SessionID::InvalidValue();
  }

  return _recentlyClosedItems[index].first;
}

// Retrieves favicon from FaviconLoader and sets image.
- (void)loadFaviconForIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  CHECK(item);

  TableViewURLItem* URLItem =
      base::apple::ObjCCastStrict<TableViewURLItem>(item);

  if (URLItem.faviconAttributes) {
    return;
  }

  __weak UITableView* tableView = self.tableView;

  [self.imageDataSource
      faviconForPageURL:URLItem.URL
             completion:^(FaviconAttributes* attributes, bool cached) {
               URLItem.faviconAttributes = attributes;
               if (!cached && attributes.faviconImage) {
                 [tableView reconfigureRowsAtIndexPaths:@[ indexPath ]];
               }
             }];
}

#pragma mark - Distant Sessions helpers

- (NSUInteger)numberOfSessions {
  if (!_syncedSessions) {
    return 0;
  }
  return _displayedTabs.size();
}

// Returns the Session Index for a given Session Tab `indexPath`.
- (size_t)indexOfSessionForTabAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ([self.tableViewModel itemTypeForIndexPath:indexPath],
            ItemTypeSessionTabData);
  // Get the sectionIdentifier for `indexPath`,
  NSNumber* sectionIdentifierForIndexPath = @(
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section]);
  // Get the index of this sectionIdentifier.
  size_t indexOfSession = [[self allSessionSectionIdentifiers]
      indexOfObject:sectionIdentifierForIndexPath];
  DCHECK_LT(indexOfSession, _displayedTabs.size());
  return indexOfSession;
}

- (synced_sessions::DistantSession const*)sessionForTabAtIndexPath:
    (NSIndexPath*)indexPath {
  const synced_sessions::DistantTabsSet& tabs_set =
      _displayedTabs[[self indexOfSessionForTabAtIndexPath:indexPath]];
  return _syncedSessions->GetSessionWithTag(tabs_set.session_tag);
}

- (synced_sessions::DistantTab const*)distantTabAtIndexPath:
    (NSIndexPath*)indexPath {
  DCHECK_EQ([self.tableViewModel itemTypeForIndexPath:indexPath],
            ItemTypeSessionTabData);
  size_t indexOfDistantTab = indexPath.row;
  synced_sessions::DistantSession const* session =
      [self sessionForTabAtIndexPath:indexPath];
  const synced_sessions::DistantTabsSet* tabs_set =
      [self distantTabsSetForSessionWithTag:session->tag];
  if (tabs_set->filtered_tabs) {
    DCHECK_LT(indexOfDistantTab, tabs_set->filtered_tabs->size());
    return tabs_set->filtered_tabs.value()[indexOfDistantTab];
  }

  // If filtered_tabs is null, all tabs in `session` should be used.
  DCHECK_LT(indexOfDistantTab, session->tabs.size());
  return session->tabs[indexOfDistantTab].get();
}

- (const synced_sessions::DistantTabsSet*)distantTabsSetForSessionWithTag:
    (const std::string&)sessionTag {
  for (const synced_sessions::DistantTabsSet& tabs_set : _displayedTabs) {
    if (sessionTag == tabs_set.session_tag) {
      return &tabs_set;
    }
  }
  return nullptr;
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
  if (base::Time::Now() > time) {
    last_used_delta = base::Time::Now() - time;
  }

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
  NSInteger dateDay = [[NSCalendar currentCalendar] component:NSCalendarUnitDay
                                                     fromDate:date];

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
  if (!self.browser) {
    // Prevent interactions if the browser is nil, for example during dismissal.
    return;
  }

  // Shouldn't reach this if in incognito.
  DCHECK(!self.isIncognito);

  // It is reasonable to ignore this request if a modal UI is already showing
  // above recent tabs. This can happen when a user simultaneously taps a
  // distant tab and "enable sync". The sync settings UI appears first and we
  // should not dismiss it to show a distant tab.
  if (self.presentedViewController) {
    return;
  }

  sync_sessions::OpenTabsUIDelegate* openTabs =
      SessionSyncServiceFactory::GetForProfile(self.profile)
          ->GetOpenTabsUIDelegate();
  const sessions::SessionTab* toLoad = nullptr;
  if (openTabs->GetForeignTab(distantTab->session_tag, distantTab->tab_id,
                              &toLoad)) {
    base::TimeDelta time_since_last_use = base::Time::Now() - toLoad->timestamp;
    base::UmaHistogramCustomTimes("IOS.DistantTab.TimeSinceLastUse",
                                  time_since_last_use, base::Minutes(1),
                                  base::Days(24), 50);

    base::RecordAction(base::UserMetricsAction(
        "MobileRecentTabManagerTabFromOtherDeviceOpened"));
    web::WebState* currentWebState = self.webStateList->GetActiveWebState();
    bool is_ntp = currentWebState &&
                  currentWebState->GetVisibleURL() == kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(
        self.isIncognito, is_ntp,
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);
    std::unique_ptr<web::WebState> web_state =
        session_util::CreateWebStateWithNavigationEntries(
            self.profile, toLoad->current_navigation_index,
            toLoad->navigations);
    if (IsNTPWithoutHistory(currentWebState)) {
      self.webStateList->ReplaceWebStateAt(self.webStateList->active_index(),
                                           std::move(web_state));
    } else {
      self.webStateList->InsertWebState(
          std::move(web_state),
          WebStateList::InsertionParams::Automatic().Activate());
    }
  }
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

- (void)openTabWithTabRestoreEntryId:(const SessionID)entry_id {
  if (!self.browser) {
    // Prevent interactions if the browser is nil, for example during dismissal.
    return;
  }

  // It is reasonable to ignore this request if a modal UI is already showing
  // above recent tabs. This can happen when a user simultaneously taps a
  // recently closed tab and "enable sync". The sync settings UI appears first
  // and we should not dismiss it to restore a recently closed tab.
  if (self.presentedViewController) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileRecentTabManagerRecentTabOpened"));
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  bool is_ntp =
      activeWebState && activeWebState->GetVisibleURL() == kChromeUINewTabURL;
  new_tab_page_uma::RecordNTPAction(
      self.isIncognito, is_ntp,
      new_tab_page_uma::ACTION_OPENED_RECENTLY_CLOSED_ENTRY);

  WindowOpenDisposition disposition =
      IsNTPWithoutHistory(self.webStateList->GetActiveWebState())
          ? WindowOpenDisposition::CURRENT_TAB
          : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  RestoreTab(entry_id, disposition, self.browser);
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

#pragma mark - Collapse/Expand sections

- (void)handleTap:(UITapGestureRecognizer*)sender {
  UIView* headerTapped = sender.view;
  NSInteger tappedHeaderSectionIdentifier = headerTapped.tag;

  if (sender.state == UIGestureRecognizerStateEnded) {
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:tappedHeaderSectionIdentifier];
    ListItem* headerItem = [self.tableViewModel headerForSectionIndex:section];

    [self toggleExpansionOfSectionIdentifier:tappedHeaderSectionIdentifier];

    UITableViewHeaderFooterView* headerView =
        [self.tableView headerViewForSection:section];
    // Highlight and collapse the section header being tapped.
    // Don't for the Loading Other Devices section header.
    if (headerItem.type == ItemTypeRecentlyClosedHeader ||
        headerItem.type == ItemTypeSessionHeader) {
      TableViewDisclosureHeaderFooterView* disclosureHeaderView =
          base::apple::ObjCCastStrict<TableViewDisclosureHeaderFooterView>(
              headerView);
      TableViewDisclosureHeaderFooterItem* disclosureItem =
          base::apple::ObjCCastStrict<TableViewDisclosureHeaderFooterItem>(
              headerItem);
      BOOL collapsed = [self.tableViewModel
          sectionIsCollapsed:[self.tableViewModel
                                 sectionIdentifierForSectionIndex:section]];
      DisclosureDirection direction =
          collapsed ? DisclosureDirectionTrailing : DisclosureDirectionDown;

      [disclosureHeaderView rotateToDirection:direction];
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
    NSIndexPath* tabIndexPath = [NSIndexPath indexPathForRow:i
                                                   inSection:sectionIndex];
    [cellIndexPathsToDeleteOrInsert addObject:tabIndexPath];
  }

  // No update required if `cellIndexPathsToDeleteOrInsert` is empty.
  // Additionally, calling `performBatchUpdates` if the table view is not
  // already displaying the current model state could crash. (crbug.com/1328988)
  if ([cellIndexPathsToDeleteOrInsert count] == 0) {
    return;
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

  [self.tableView performBatchUpdates:tableUpdates completion:nil];
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  DCHECK(self.signinPromoViewMediator);
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOtherDevices] ||
      ![self.tableViewModel hasItemForItemType:ItemTypeOtherDevicesSigninPromo
                             sectionIdentifier:SectionIdentifierOtherDevices]) {
    // Need to remove the sign-in promo view mediator when the section doesn't
    // exist anymore. The mediator should not be removed each time the section
    // is removed since the section is replaced at each reload.
    // Metrics would be recorded too often.
    // The other device section can be present even without the promo.
    [self disconnectMediator];
    return;
  }
  if ([self.tableViewModel hasItemForItemType:ItemTypeOtherDevicesSigninPromo
                            sectionIdentifier:SectionIdentifierOtherDevices]) {
    // Update the TableViewSigninPromoItem configurator. It will be used by the
    // item to configure the cell once `self.tableView` requests a cell on
    // cellForRowAtIndexPath.
    NSIndexPath* indexPath = [self.tableViewModel
        indexPathForItemType:ItemTypeOtherDevicesSigninPromo
           sectionIdentifier:SectionIdentifierOtherDevices];
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    TableViewSigninPromoItem* signInItem =
        base::apple::ObjCCastStrict<TableViewSigninPromoItem>(item);
    signInItem.configurator = configurator;
    // If section is collapsed no tableView update is needed.
    if ([self.tableViewModel
            sectionIsCollapsed:SectionIdentifierOtherDevices]) {
      return;
    }
    // After setting the new configurator to the item, reload the item's Cell.
    [self reloadCellsForItems:@[ signInItem ]
             withRowAnimation:UITableViewRowAnimationNone];
  }
}

#pragma mark - SyncPresenter

- (void)showPrimaryAccountReauth {
  [self.presentationDelegate showPrimaryAccountReauth];
}

- (void)showSyncPassphraseSettings {
  [self.settingsHandler showSyncPassphraseSettingsFromViewController:self];
}

- (void)showGoogleServicesSettings {
  [self.settingsHandler showGoogleServicesSettingsFromViewController:self];
}

- (void)showAccountSettings {
  [self.settingsHandler showAccountsSettingsFromViewController:self
                                          skipIfUINotAvailable:NO];
}

- (void)showTrustedVaultReauthForFetchKeysWithTrigger:
    (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger {
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  CHECK(!_trustedVaultReauthenticationCoordinator, base::NotFatalUntil::M145);
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:self
                             browser:self.browser
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:securityDomainID
                             trigger:trigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

- (void)showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
    (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger {
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  CHECK(!_trustedVaultReauthenticationCoordinator, base::NotFatalUntil::M145);
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:self
                             browser:self.browser
                              intent:intent
                    securityDomainID:securityDomainID
                             trigger:trigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

#pragma mark - SigninPromoViewMediatorDelegate

- (void)showSignin:(SigninPromoViewMediator*)mediator
           command:(ShowSigninCommand*)command {
  CHECK_EQ(mediator, self.signinPromoViewMediator);
  __weak __typeof(self) weakSelf = self;
  [command addSigninCompletion:^(SigninCoordinator* coordinator,
                                 SigninCoordinatorResult result,
                                 id<SystemIdentity>) {
    [weakSelf signinDidCompleteWithCoordinator:coordinator result:result];
  }];
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  _signinCoordinator = [SigninCoordinator
      signinCoordinatorWithCommand:command
                           browser:signin::GetRegularBrowser(_browser)
                baseViewController:self];
  [_signinCoordinator start];
}

#pragma mark - SigninPromoViewMediatorDelegate Helper

- (void)signinDidCompleteWithCoordinator:(SigninCoordinator*)coordinator
                                  result:(SigninCoordinatorResult)result {
  CHECK_EQ(_signinCoordinator, coordinator, base::NotFatalUntil::M151);
  [self.signinPromoViewMediator signinDidCompleteWithResult:result];
  [self stopSigninCoordinator];
  [self.presentationDelegate showHistorySyncOptInAfterDedicatedSignIn:YES];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction("IOSRecentTabsCloseWithSwipe"));
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (sel_isEqual(action, @selector(keyCommand_close))) {
    return [self isPresentedModally];
  }
  return [super canPerformAction:action withSender:sender];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self.presentationDelegate showActiveRegularTabFromRecentTabs];
}

#pragma mark - TrustedVaultReauthenticationCoordinatorDelegate

- (void)trustedVaultReauthenticationCoordinatorWantsToBeStopped:
    (TrustedVaultReauthenticationCoordinator*)coordinator {
  CHECK_EQ(coordinator, _trustedVaultReauthenticationCoordinator);
  [self stopTrustedVaultReauthenticationCoordinator];
}

#pragma mark - Private Helpers

- (void)stopTrustedVaultReauthenticationCoordinator {
  [_trustedVaultReauthenticationCoordinator stop];
  _trustedVaultReauthenticationCoordinator.delegate = nil;
  _trustedVaultReauthenticationCoordinator = nil;
}

// Disconnects the mediator.
- (void)disconnectMediator {
  [self.signinPromoViewMediator disconnect];
  self.signinPromoViewMediator = nil;
}

- (void)didTapPromoActionButton {
  syncer::SyncService* const syncService = self.syncService;
  if (!syncService) {
    return;
  }
  syncer::SyncService::UserActionableError error =
      syncService->GetUserActionableError();
  if (error == syncer::SyncService::UserActionableError::kSignInNeedsUpdate) {
    [self showPrimaryAccountReauth];
  } else if ([self shouldShowHistorySyncOnPromoAction]) {
    [self.presentationDelegate showHistorySyncOptInAfterDedicatedSignIn:NO];
  } else if (ShouldShowSyncSettings(error)) {
    [self.settingsHandler showSyncSettingsFromViewController:self];
  } else if (error ==
             syncer::SyncService::UserActionableError::kNeedsPassphrase) {
    [self showSyncPassphraseSettings];
  }
}

// Returns YES if the History Sync Opt-In should be shown when the promo action
// button is tapped.
// TODO(crbug.com/40921836): This logic should be moved outside of the
// ViewController.
- (BOOL)shouldShowHistorySyncOnPromoAction {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  // Check if History Sync Opt-In should be skipped.
  // In case it's not necessary to show the history opt-in, but the promo action
  // button is still available, sync errors should be checked to show the
  // correct screen to handle the error (ex. passphrase screen).
  return history_sync::GetSkipReason(self.syncService, authenticationService,
                                     _profile->GetPrefs(), NO) ==
         history_sync::HistorySyncSkipReason::kNone;
}

@end

@implementation ListModelCollapsedSceneSessionMediator {
  UISceneSession* _session;
}

- (instancetype)initWithSession:(UISceneSession*)session {
  self = [super init];
  if (self) {
    _session = session;
  }
  return self;
}

- (void)setSectionKey:(NSString*)sectionKey collapsed:(BOOL)collapsed {
  NSMutableDictionary* newUserInfo =
      [NSMutableDictionary dictionaryWithDictionary:_session.userInfo];
  NSMutableDictionary* newCollapsedSection = [NSMutableDictionary
      dictionaryWithDictionary:newUserInfo[kListModelCollapsedKey]];
  newUserInfo[kListModelCollapsedKey] = newCollapsedSection;
  newCollapsedSection[sectionKey] = [NSNumber numberWithBool:collapsed];
  _session.userInfo = newUserInfo;
}

- (BOOL)sectionKeyIsCollapsed:(NSString*)sectionKey {
  NSDictionary* collapsedSections = _session.userInfo[kListModelCollapsedKey];
  NSNumber* value = (NSNumber*)[collapsedSections valueForKey:sectionKey];
  return [value boolValue];
}

@end
