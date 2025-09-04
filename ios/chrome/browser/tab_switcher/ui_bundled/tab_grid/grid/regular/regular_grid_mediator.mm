// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/activity_label_data.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_toolbars_configuration_provider.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/regular/regular_grid_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tabs/model/tabs_closer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

// TODO(crbug.com/40273478): Needed for `TabPresentationDelegate`, should be
// refactored.
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_view_controller.h"

using collaboration::messaging::PersistentNotificationType;

namespace {

using ScopedTabGroupSyncObservation =
    base::ScopedObservation<tab_groups::TabGroupSyncService,
                            tab_groups::TabGroupSyncService::Observer>;

}  // namespace

@interface RegularGridMediator () <MessagingBackendServiceObserving,
                                   TabGroupSyncServiceObserverDelegate>
@end

@implementation RegularGridMediator {
  // The service to observe.
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  // The share kit service.
  raw_ptr<ShareKitService> _shareKitService;
  // The bridge between the service C++ observer and this Objective-C class.
  std::unique_ptr<TabGroupSyncServiceObserverBridge> _syncServiceObserver;
  std::unique_ptr<ScopedTabGroupSyncObservation> _scopedSyncServiceObservation;
  // TabsClosed used to implement the "close all tabs" operation with support
  // for undoing the operation.
  std::unique_ptr<TabsCloser> _tabsCloser;
  // Whether the current grid is selected.
  BOOL _selected;
  // A service to get activity messages for a shared tab group.
  raw_ptr<collaboration::messaging::MessagingBackendService> _messagingService;
  // A set of ID of the shared tab group that has changed and a user has not
  // seen it yet.
  std::set<tab_groups::LocalTabGroupID> _dirtyGroups;
  // The bridge between the C++ MessagingBackendService observer and this
  // Objective-C class.
  std::unique_ptr<MessagingBackendServiceBridge> _messagingBackendServiceBridge;
}

- (instancetype)
     initWithModeHolder:(TabGridModeHolder*)modeHolder
    tabGroupSyncService:(tab_groups::TabGroupSyncService*)tabGroupSyncService
        shareKitService:(ShareKitService*)shareKitService
       messagingService:(collaboration::messaging::MessagingBackendService*)
                            messagingService {
  if ((self = [super initWithModeHolder:modeHolder])) {
    _tabGroupSyncService = tabGroupSyncService;
    _shareKitService = shareKitService;
    _syncServiceObserver =
        std::make_unique<TabGroupSyncServiceObserverBridge>(self);

    // The `_tabGroupSyncService` is `nullptr` in incognito.
    if (_tabGroupSyncService) {
      _scopedSyncServiceObservation =
          std::make_unique<ScopedTabGroupSyncObservation>(
              _syncServiceObserver.get());
      _scopedSyncServiceObservation->Observe(_tabGroupSyncService);
    }

    _messagingService = messagingService;
    if (_messagingService) {
      _messagingBackendServiceBridge =
          std::make_unique<MessagingBackendServiceBridge>(self);
      _messagingService->AddPersistentMessageObserver(
          _messagingBackendServiceBridge.get());
      [self fetchMessagesForGroup];
    }
  }
  return self;
}

#pragma mark - GridCommands

- (void)closeItemWithID:(web::WebStateID)itemID {
  // Record when a regular tab is closed.
  base::RecordAction(base::UserMetricsAction("MobileTabGridCloseRegularTab"));
  [super closeItemWithID:itemID];
}

// TODO(crbug.com/40273478): Refactor the grid commands to have the same
// function name to close all.
- (void)closeAllItems {
  NOTREACHED() << "Regular tabs should be saved before close all.";
}

- (void)saveAndCloseAllItems {
  [self.inactiveTabsGridCommands saveAndCloseAllItems];
  if (![self canCloseTabs]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));

  const int tabGroupCount = self.webStateList->GetGroups().size();

  const int closedTabs = _tabsCloser->CloseTabs();
  RecordTabGridCloseTabsCount(closedTabs);

  [self showTabGroupSnackbarOrIPH:tabGroupCount];
}

- (void)undoCloseAllItems {
  [self.inactiveTabsGridCommands undoCloseAllItems];
  if (![self canUndoCloseTabs]) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));

  _tabsCloser->UndoCloseTabs();
}

- (void)discardSavedClosedItems {
  [self.inactiveTabsGridCommands discardSavedClosedItems];
  if (![self canUndoCloseTabs]) {
    return;
  }
  _tabsCloser->ConfirmDeletion();
  [self configureToolbarsButtons];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selected = selected;

  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectRegularPanel"));

    [self configureToolbarsButtons];
  }
}

- (void)setPageAsActive {
  [self.gridConsumer setActivePageFromPage:TabGridPageRegularTabs];
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  // TODO(crbug.com/40273478): Clean this in order to have "Close All" and
  // "Undo" separated actions. This was saved as a stack: first save the
  // inactive tabs, then the active tabs. So undo in the reverse order: first
  // undo the active tabs, then the inactive tabs.
  if ([self canUndoCloseRegularOrInactiveTabs]) {
    [self.consumer willUndoCloseAll];
    [self undoCloseAllItems];
    [self.consumer didUndoCloseAll];
  } else {
    [self.consumer willCloseAll];
    [self saveAndCloseAllItems];
    [self.consumer didCloseAll];
  }
  // This is needed because configure button is called (web state list observer
  // in base grid mediator) when regular tabs are modified but not when inactive
  // tabs are modified.
  [self configureToolbarsButtons];
}

- (void)newTabButtonTapped:(id)sender {
  // Ignore the tap if the current page is disabled for some reason, by policy
  // for instance. This is to avoid situations where the tap action from an
  // enabled page can make it to a disabled page by releasing the
  // button press after switching to the disabled page (b/273416844 is an
  // example).
  if (IsIncognitoModeForced(self.browser->GetProfile()->GetPrefs())) {
    return;
  }

  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  [self.gridConsumer prepareForDismissal];
  // Shows the tab only if has been created.
  if ([self addNewItem]) {
    [self displayActiveTab];
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCreateRegularTab"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridFailedCreateRegularTab"));
  }
}

#pragma mark - Parent's function

- (void)disconnect {
  if (_messagingService) {
    _messagingService->RemovePersistentMessageObserver(
        _messagingBackendServiceBridge.get());
    _messagingBackendServiceBridge.reset();
    _messagingService = nullptr;
  }
  _tabsCloser.reset();
  _scopedSyncServiceObservation.reset();
  _syncServiceObserver.reset();
  _tabGroupSyncService = nullptr;
  _shareKitService = nullptr;
  [super disconnect];
}

- (void)configureToolbarsButtons {
  if (!_selected) {
    return;
  }
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  if (IsIncognitoModeForced(self.browser->GetProfile()->GetPrefs())) {
    [self.toolbarsMutator
        setToolbarConfiguration:
            [TabGridToolbarsConfiguration
                disabledConfigurationForPage:TabGridPageRegularTabs]];
    return;
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc]
          initWithPage:TabGridPageRegularTabs];

  if (self.modeHolder.mode == TabGridMode::kSelection) {
    [self configureButtonsInSelectionMode:toolbarsConfiguration];
  } else {
    toolbarsConfiguration.closeAllButton = [self canCloseRegularOrInactiveTabs];
    toolbarsConfiguration.doneButton = !self.webStateList->empty();
    toolbarsConfiguration.newTabButton = YES;
    toolbarsConfiguration.searchButton = YES;
    toolbarsConfiguration.selectTabsButton = [self hasRegularTabs];
    toolbarsConfiguration.undoButton = [self canUndoCloseRegularOrInactiveTabs];
  }

  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

- (void)displayActiveTab {
  [self.gridConsumer setActivePageFromPage:TabGridPageRegularTabs];
  [self.tabPresentationDelegate showActiveTabInPage:TabGridPageRegularTabs
                                       focusOmnibox:NO];
}

- (void)updateForTabInserted {
  if (!self.webStateList->empty()) {
    [self discardSavedClosedItems];
  }
}

// Overrides the parent to return the data used for showing a label if there is
// a new message for a group.
- (ActivityLabelData*)activityLabelDataForGroup:
    (tab_groups::TabGroupId)groupID {
  if (!_dirtyGroups.contains(groupID)) {
    return nil;
  }
  ActivityLabelData* data = [[ActivityLabelData alloc] init];
  data.labelString =
      l10n_util::GetNSString(IDS_IOS_TAB_GROUP_NEW_ACTIVITY_LABEL_TEXT);
  return data;
}

#pragma mark - TabGroupSyncServiceObserverDelegate

- (void)tabGroupSyncServiceInitialized {
  [self populateConsumerItems];
}

- (void)tabGroupSyncServiceTabGroupUpdated:
            (const tab_groups::SavedTabGroup&)group
                                fromSource:(tab_groups::TriggerSource)source {
  [self updateCellForGroup:group];
}

- (void)tabGroupSyncServiceTabGroupMigrated:
            (const tab_groups::SavedTabGroup&)newGroup
                                  oldSyncID:(const base::Uuid&)oldSync
                                 fromSource:(tab_groups::TriggerSource)source {
  [self updateCellForGroup:newGroup];
}

#pragma mark - Private

// Updates the cell corresponding to the given group if the group is present in
// the current web state list.
- (void)updateCellForGroup:(const tab_groups::SavedTabGroup&)savedGroup {
  std::set<const TabGroup*> groups = self.webStateList->GetGroups();
  const TabGroup* localGroup = nullptr;
  for (const TabGroup* group : groups) {
    if (group->tab_group_id() == savedGroup.local_group_id()) {
      localGroup = group;
      break;
    }
  }
  if (!localGroup) {
    return;
  }

  GridItemIdentifier* groupIdentifier =
      [GridItemIdentifier groupIdentifier:localGroup];
  [self.consumer replaceItem:groupIdentifier
         withReplacementItem:groupIdentifier];
}

// YES if there are regular tabs in the grid.
- (BOOL)hasRegularTabs {
  return [self canCloseTabs];
}

- (BOOL)canCloseTabs {
  return _tabsCloser && _tabsCloser->CanCloseTabs();
}

- (BOOL)canUndoCloseTabs {
  return _tabsCloser && _tabsCloser->CanUndoCloseTabs();
}

- (BOOL)canCloseRegularOrInactiveTabs {
  if ([self canCloseTabs]) {
    return YES;
  }

  // This is an indirect way to check whether the inactive tabs can close
  // tabs or undo a close tabs action.
  TabGridToolbarsConfiguration* containedGridToolbarsConfiguration =
      [self.containedGridToolbarsProvider toolbarsConfiguration];
  return containedGridToolbarsConfiguration.closeAllButton;
}

- (BOOL)canUndoCloseRegularOrInactiveTabs {
  if ([self canUndoCloseTabs]) {
    return YES;
  }

  // This is an indirect way to check whether the inactive tabs can close
  // tabs or undo a close tabs action.
  TabGridToolbarsConfiguration* containedGridToolbarsConfiguration =
      [self.containedGridToolbarsProvider toolbarsConfiguration];
  return containedGridToolbarsConfiguration.undoButton;
}

// Gets messages to indicate that the shared tab group has changed and the user
// has not seen it yet and keeps the necessary information from the messages.
- (void)fetchMessagesForGroup {
  if (!_messagingService || !_messagingService->IsInitialized() ||
      !self.webStateList) {
    return;
  }

  for (const TabGroup* tabGroup : self.webStateList->GetGroups()) {
    tab_groups::LocalTabGroupID groupID = tabGroup->tab_group_id();
    if ([self hasNotificationsForGroup:groupID]) {
      _dirtyGroups.insert(groupID);
    }
  }
}

// Returns whether there are notifications to be displayed for `groupID`.
- (BOOL)hasNotificationsForGroup:(tab_groups::LocalTabGroupID)groupID {
  std::vector<collaboration::messaging::PersistentMessage> messages =
      _messagingService->GetMessagesForGroup(
          groupID,
          collaboration::messaging::PersistentNotificationType::DIRTY_TAB);

  for (auto const& message : messages) {
    if (!message.attribution.tab_metadata.has_value()) {
      continue;
    }

    if (message.collaboration_event ==
            collaboration::messaging::CollaborationEvent::TAB_ADDED ||
        message.collaboration_event ==
            collaboration::messaging::CollaborationEvent::TAB_UPDATED) {
      return YES;
    }
  }

  messages = _messagingService->GetMessagesForGroup(
      groupID,
      collaboration::messaging::PersistentNotificationType::TOMBSTONED);

  for (auto const& message : messages) {
    if (!message.attribution.tab_metadata.has_value()) {
      continue;
    }

    if (message.collaboration_event ==
        collaboration::messaging::CollaborationEvent::TAB_REMOVED) {
      return YES;
    }
  }
  return NO;
}

// Reconfigures a group cell specified by `localTabGroupID`.
- (void)reconfigureGroup:(tab_groups::LocalTabGroupID)localTabGroupID {
  for (const TabGroup* group : self.webStateList->GetGroups()) {
    if (group->tab_group_id() == localTabGroupID) {
      GridItemIdentifier* item = [GridItemIdentifier groupIdentifier:group];
      [self.consumer replaceItem:item withReplacementItem:item];
      return;
    }
  }
}

#pragma mark - Setters

- (void)setBrowser:(Browser*)browser {
  [super setBrowser:browser];
  if (browser) {
    _tabsCloser = std::make_unique<TabsCloser>(
        browser, TabsCloser::ClosePolicy::kRegularTabs);
  } else {
    _tabsCloser.reset();
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  [super setWebStateList:webStateList];
  if (webStateList) {
    [self fetchMessagesForGroup];
  }
}

#pragma mark - BaseGridMediatorItemProvider

- (id<FacePileProviding>)facePileProviderForItem:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kGroup);

  const TabGroup* tabGroup = itemID.tabGroupItem.tabGroup;

  if (!_shareKitService || !_shareKitService->IsSupported() ||
      !_tabGroupSyncService || !tabGroup) {
    return nil;
  }

  syncer::CollaborationId collaborationID =
      tab_groups::utils::GetTabGroupCollabID(tabGroup, _tabGroupSyncService);
  if (collaborationID->empty()) {
    return nil;
  }

  UIColor* groupColor =
      tab_groups::ColorForTabGroupColorId(tabGroup->GetColor());
  return
      [self.regularDelegate facePileProviderForGroupID:collaborationID.value()
                                            groupColor:groupColor];
}

#pragma mark - MessagingBackendServiceObserving

- (void)onMessagingBackendServiceInitialized {
  [self fetchMessagesForGroup];
}

- (void)displayPersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (message.type != PersistentNotificationType::DIRTY_TAB_GROUP) {
    return;
  }
  if (!message.attribution.tab_group_metadata.has_value()) {
    return;
  }
  collaboration::messaging::TabGroupMessageMetadata group_data =
      message.attribution.tab_group_metadata.value();
  if (!group_data.local_tab_group_id.has_value()) {
    return;
  }
  tab_groups::LocalTabGroupID localTabGroupID =
      group_data.local_tab_group_id.value();
  _dirtyGroups.insert(localTabGroupID);

  [self reconfigureGroup:localTabGroupID];
}

- (void)hidePersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (!message.attribution.tab_group_metadata.has_value()) {
    return;
  }
  collaboration::messaging::TabGroupMessageMetadata group_data =
      message.attribution.tab_group_metadata.value();
  if (!group_data.local_tab_group_id.has_value()) {
    return;
  }
  tab_groups::LocalTabGroupID localTabGroupID =
      group_data.local_tab_group_id.value();
  if ([self hasNotificationsForGroup:localTabGroupID]) {
    _dirtyGroups.insert(localTabGroupID);
  } else {
    _dirtyGroups.erase(localTabGroupID);
  }

  [self reconfigureGroup:localTabGroupID];
}

@end
