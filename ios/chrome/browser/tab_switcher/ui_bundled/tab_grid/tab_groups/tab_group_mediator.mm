// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_mediator.h"

#import <algorithm>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/data_sharing/public/group_data.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/activity_label_data.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ScopedTabGroupSyncObservation =
    base::ScopedObservation<tab_groups::TabGroupSyncService,
                            tab_groups::TabGroupSyncService::Observer>;

namespace {
// The preferred size in points for the avatar icons.
constexpr CGFloat kFacePileAvatarSize = 24;
// The preferred size in points for the avatar icon in the activity label.
constexpr CGFloat kActivityLabelAvatarSize = 16;
}  // namespace

@interface TabGroupMediator () <MessagingBackendServiceObserving,
                                TabGroupSyncServiceObserverDelegate>
@end

@implementation TabGroupMediator {
  // The service to observe.
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  // The share kit service.
  raw_ptr<ShareKitService> _shareKitService;
  // The collaboration service.
  raw_ptr<collaboration::CollaborationService> _collaborationService;
  // The bridge between the service C++ observer and this Objective-C class.
  std::unique_ptr<TabGroupSyncServiceObserverBridge> _syncServiceObserver;
  std::unique_ptr<ScopedTabGroupSyncObservation> _scopedSyncServiceObservation;
  // Tab group consumer.
  __weak id<TabGroupConsumer> _groupConsumer;
  // Current group.
  base::WeakPtr<const TabGroup> _tabGroup;
  // A service to get activity messages for a shared tab group.
  raw_ptr<collaboration::messaging::MessagingBackendService> _messagingService;
  // A map of a tab ID and a message to indicate that a tab should display a
  // chip on its cell.
  std::map<tab_groups::LocalTabID, collaboration::messaging::PersistentMessage>
      _dirtyTabs;
  // The bridge between the C++ MessagingBackendService observer and this
  // Objective-C class.
  std::unique_ptr<MessagingBackendServiceBridge> _messagingBackendServiceBridge;
}

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
     tabGroupSyncService:(tab_groups::TabGroupSyncService*)tabGroupSyncService
         shareKitService:(ShareKitService*)shareKitService
    collaborationService:
        (collaboration::CollaborationService*)collaborationService
                tabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                consumer:(id<TabGroupConsumer>)groupConsumer
            gridConsumer:(id<TabCollectionConsumer>)gridConsumer
              modeHolder:(TabGridModeHolder*)modeHolder
        messagingService:(collaboration::messaging::MessagingBackendService*)
                             messagingService {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group mediator outside the "
         "Tab Groups experiment.";
  CHECK(webStateList);
  CHECK(groupConsumer);
  CHECK(tabGroup);
  if ((self = [super initWithModeHolder:modeHolder])) {
    _tabGroupSyncService = tabGroupSyncService;
    _shareKitService = shareKitService;
    _collaborationService = collaborationService;
    _syncServiceObserver =
        std::make_unique<TabGroupSyncServiceObserverBridge>(self);

    // The `_tabGroupSyncService` is nil in incognito.
    if (_tabGroupSyncService) {
      _scopedSyncServiceObservation =
          std::make_unique<ScopedTabGroupSyncObservation>(
              _syncServiceObserver.get());
      _scopedSyncServiceObservation->Observe(_tabGroupSyncService);
    }

    self.webStateList = webStateList;
    _groupConsumer = groupConsumer;
    self.consumer = gridConsumer;

    _tabGroup = tabGroup;

    [_groupConsumer setGroupTitle:tabGroup->GetTitle()];
    [_groupConsumer setGroupColor:tabGroup->GetColor()];

    _messagingService = messagingService;
    if (_messagingService) {
      _messagingBackendServiceBridge =
          std::make_unique<MessagingBackendServiceBridge>(self);
      _messagingService->AddPersistentMessageObserver(
          _messagingBackendServiceBridge.get());
      [self fetchMessagesForChip];
    }

    [self updateFacePileUI];
    [self populateConsumerItems];
  }
  return self;
}

#pragma mark - TabGroupMutator

- (BOOL)addNewItemInGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  if (!_tabGroup) {
    return NO;
  }
  GURL URL(kChromeUINewTabURL);
  int groupCount = _tabGroup->range().count();
  [self insertNewWebStateAtGridIndex:groupCount withURL:URL];
  return groupCount != _tabGroup->range().count();
}

- (void)ungroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  auto scoped_lock = self.webStateList->StartBatchOperation();
  self.webStateList->DeleteGroup(_tabGroup.get());
  _tabGroup.reset();
}

- (void)closeGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  if (IsTabGroupSyncEnabled()) {
    tab_groups::TabGroupSyncService* syncService =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(
            self.browser->GetProfile());
    tab_groups::utils::CloseTabGroupLocally(_tabGroup.get(), self.webStateList,
                                            syncService);
  } else {
    CloseAllWebStatesInGroup(*self.webStateList, _tabGroup.get(),
                             WebStateList::CLOSE_USER_ACTION);
  }
  _tabGroup.reset();
}

- (void)deleteGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  CloseAllWebStatesInGroup(*self.webStateList, _tabGroup.get(),
                           WebStateList::CLOSE_USER_ACTION);
  _tabGroup.reset();
}

#pragma mark - Parent's functions

- (void)disconnect {
  if (_messagingService) {
    _messagingService->RemovePersistentMessageObserver(
        _messagingBackendServiceBridge.get());
    _messagingBackendServiceBridge.reset();
  }
  _scopedSyncServiceObservation.reset();
  _syncServiceObserver.reset();
  _tabGroupSyncService = nullptr;
  _collaborationService = nullptr;
  _shareKitService = nullptr;
  [super disconnect];
}

- (void)configureToolbarsButtons {
  // No-op
}

// Overrides the parent to only display tabs from the group.
- (void)populateConsumerItems {
  if (!self.webStateList || !_tabGroup) {
    return;
  }

  GridItemIdentifier* identifier = nil;
  int webStateIndex = self.webStateList->active_index();
  if (webStateIndex != WebStateList::kInvalidIndex &&
      self.webStateList->GetGroupOfWebStateAt(webStateIndex) ==
          _tabGroup.get()) {
    web::WebState* webState = self.webStateList->GetWebStateAt(webStateIndex);
    identifier = [GridItemIdentifier tabIdentifier:webState];
  }

  [self.consumer populateItems:CreateTabItems(self.webStateList,
                                              _tabGroup->range())
        selectedItemIdentifier:identifier];
}

// Override the parent to only show individual web state in the group.
- (GridItemIdentifier*)activeIdentifier {
  WebStateList* webStateList = self.webStateList;
  if (!webStateList || !_tabGroup) {
    return nil;
  }

  int webStateIndex = webStateList->active_index();
  if (webStateIndex == WebStateList::kInvalidIndex) {
    return nil;
  }

  if (!_tabGroup->range().contains(webStateIndex)) {
    return nil;
  }

  return [GridItemIdentifier
      tabIdentifier:webStateList->GetWebStateAt(webStateIndex)];
}

// Overrides the parent observations: only observe the group `WebState`s.
- (void)addWebStateObservations {
  if (!_tabGroup) {
    return;
  }
  for (int index : _tabGroup->range()) {
    web::WebState* webState = self.webStateList->GetWebStateAt(index);
    [self addObservationForWebState:webState];
  }
}

// Overrides the parent as there is only tab cells.
- (void)insertItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  GridItemIdentifier* nextItemIdentifier;
  if (nextWebStateIndex < _tabGroup->range().range_end()) {
    nextItemIdentifier = [GridItemIdentifier
        tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
  }
  [self.consumer insertItem:item
                beforeItemID:nextItemIdentifier
      selectedItemIdentifier:[self activeIdentifier]];
}

// Overrides the parent as there is only tab cells.
- (void)moveItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  GridItemIdentifier* nextItem;
  if (nextWebStateIndex < _tabGroup->range().range_end()) {
    nextItem = [GridItemIdentifier
        tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
  }
  [self.consumer moveItem:item beforeItem:nextItem];
}

// Overrides the parent as there is only tab cells.
- (void)updateConsumerItemForWebState:(web::WebState*)webState {
  GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
  [self.consumer replaceItem:item withReplacementItem:item];
}

- (void)insertNewWebStateAtGridIndex:(int)index withURL:(const GURL&)newTabURL {
  ProfileIOS* profile = self.browser->GetProfile();

  if (!self.browser || !_tabGroup || !profile) {
    return;
  }

  WebStateList* webStateList = self.webStateList;
  if (!webStateList->ContainsGroup(_tabGroup.get())) {
    return;
  }

  int webStateListIndex = _tabGroup->range().range_begin() + index;
  webStateListIndex =
      std::clamp(webStateListIndex, _tabGroup->range().range_begin(),
                 _tabGroup->range().range_end());

  UrlLoadParams params = UrlLoadParams::InNewTab(newTabURL);
  params.in_incognito = profile->IsOffTheRecord();
  params.append_to = OpenPosition::kSpecifiedIndex;
  params.insertion_index = webStateListIndex;
  params.load_in_group = true;
  params.tab_group = _tabGroup;
  self.URLLoader->Load(params);
}

- (BOOL)canHandleTabGroupDrop:(TabGroupInfo*)tabGroupInfo {
  return NO;
}

- (void)recordExternalURLDropped {
  base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                DragItemOrigin::kOther);
}

// Overrides the parent to return the data if there is a new message for a tab
// in a group.
- (ActivityLabelData*)activityLabelDataForTab:(web::WebStateID)webStateID {
  if (!_dirtyTabs.contains(webStateID.identifier())) {
    return nil;
  }

  ActivityLabelData* data = [[ActivityLabelData alloc] init];

  collaboration::messaging::PersistentMessage message =
      _dirtyTabs[webStateID.identifier()];
  switch (message.collaboration_event) {
    case collaboration::messaging::CollaborationEvent::TAB_ADDED:
      data.labelString = l10n_util::GetNSString(
          IDS_IOS_TAB_GROUP_ACTIVITY_LABEL_USER_ADDED_TEXT);
      break;
    case collaboration::messaging::CollaborationEvent::TAB_UPDATED:
      data.labelString = l10n_util::GetNSString(
          IDS_IOS_TAB_GROUP_ACTIVITY_LABEL_USER_CHANGED_TEXT);
      break;
    default:
      // Do not show any labels for other activities.
      return nil;
  }

  if (!_shareKitService->IsSupported() ||
      !message.attribution.triggering_user.has_value()) {
    // TODO(crbug.com/385090658): Now, `triggering_user` doesn't have a value in
    // any cases (a tab is added / updated) and the label isn't created. Confirm
    // why `triggering_user` is nil.
    return nil;
  }

  ShareKitAvatarConfiguration* config =
      [[ShareKitAvatarConfiguration alloc] init];
  data_sharing::GroupMember user = message.attribution.triggering_user.value();
  config.avatarUrl =
      [NSURL URLWithString:base::SysUTF8ToNSString(user.avatar_url.spec())];
  // Use email intead when the display name is empty.
  config.displayName = user.display_name.empty()
                           ? base::SysUTF8ToNSString(user.email)
                           : base::SysUTF8ToNSString(user.display_name);
  config.avatarSize =
      CGSizeMake(kActivityLabelAvatarSize, kActivityLabelAvatarSize);
  data.avatarPrimitive = _shareKitService->AvatarImage(config);

  return data;
}

#pragma mark - TabCollectionDragDropHandler override

// Overrides the parent as the given destination index do not take into account
// elements outside the group.
- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection {
  WebStateList* webStateList = self.webStateList;
  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    int destinationWebStateIndex =
        _tabGroup->range().range_begin() + destinationIndex;
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    int sourceWebStateIndex =
        GetWebStateIndex(webStateList, WebStateSearchCriteria{
                                           .identifier = tabInfo.tabID,
                                       });
    const auto insertionParams =
        WebStateList::InsertionParams::AtIndex(destinationWebStateIndex)
            .InGroup(_tabGroup.get());
    if (sourceWebStateIndex == WebStateList::kInvalidIndex) {
      base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
      MoveTabToBrowser(tabInfo.tabID, self.browser, insertionParams);
      return;
    }

    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    } else {
      base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    }

    // Reorder tabs.
    MoveWebStateWithIdentifierToInsertionParams(
        tabInfo.tabID, insertionParams, webStateList, fromSameCollection);
    return;
  }
  base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                DragItemOrigin::kOther);

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewWebStateAtGridIndex:destinationIndex withURL:droppedURL.URL];
    return;
  }
}

#pragma mark - WebStateListObserving override

// Overrides the parent observations. The parent treats a group as one cell,
// whereas this TabGroupMediator only cares about one group, and shows grouped
// tabs as many cells.
- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (webStateList->IsBatchInProgress() || !_tabGroup) {
    return;
  }
  if (detachChange.group() != _tabGroup.get()) {
    // This can occur if a tab from a different group is closed.
    return;
  }

  web::WebState* detachedWebState = detachChange.detached_web_state();
  GridItemIdentifier* identifierToRemove =
      [GridItemIdentifier tabIdentifier:detachedWebState];
  [self.consumer removeItemWithIdentifier:identifierToRemove
                   selectedItemIdentifier:[self activeIdentifier]];
  [self removeObservationForWebState:detachedWebState];
}

// Overrides the parent observations. The parent treats a group as one cell and
// just update it, whereas this TabGroupMediator treats them as multiples cell,
// so this overrides manages notifications accordingly.
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (webStateList->IsBatchInProgress() || !_tabGroup) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& selectionOnlyChange =
          change.As<WebStateListChangeStatusOnly>();
      const TabGroup* oldGroup = selectionOnlyChange.old_group();
      const TabGroup* newGroup = selectionOnlyChange.new_group();

      if (oldGroup != newGroup) {
        // There is a change of group.
        if (oldGroup == _tabGroup.get()) {
          web::WebState* currentWebState =
              self.webStateList->GetWebStateAt(selectionOnlyChange.index());

          GridItemIdentifier* tabIdentifierToAddToGroup =
              [GridItemIdentifier tabIdentifier:currentWebState];
          [self.consumer removeItemWithIdentifier:tabIdentifierToAddToGroup
                           selectedItemIdentifier:[self activeIdentifier]];
          [self removeObservationForWebState:currentWebState];
        }

        if (newGroup == _tabGroup.get()) {
          int webStateIndex = selectionOnlyChange.index();
          web::WebState* currentWebState =
              self.webStateList->GetWebStateAt(webStateIndex);

          [self insertItem:[GridItemIdentifier tabIdentifier:currentWebState]
              beforeWebStateIndex:webStateIndex + 1];
          [self addObservationForWebState:currentWebState];
        }
        break;
      }
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataChange =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      const TabGroup* tabGroup = visualDataChange.updated_group();
      if (_tabGroup.get() != tabGroup) {
        break;
      }
      [_groupConsumer setGroupTitle:tabGroup->GetTitle()];
      [_groupConsumer setGroupColor:tabGroup->GetColor()];
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& groupDeleteChange =
          change.As<WebStateListChangeGroupDelete>();
      if (groupDeleteChange.deleted_group() == _tabGroup.get()) {
        _tabGroup.reset();
        [self.tabGroupsHandler hideTabGroup];
      }
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();
      if (moveChange.old_group() != _tabGroup.get() &&
          moveChange.new_group() != _tabGroup.get()) {
        // Not related to this group.
        break;
      }
      web::WebState* webState = moveChange.moved_web_state();
      GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
      if (moveChange.old_group() == moveChange.new_group()) {
        // Move in the same group
        [self moveItem:item
            beforeWebStateIndex:moveChange.moved_to_index() + 1];
      } else {
        if (moveChange.old_group() == _tabGroup.get()) {
          // The tab left the group.
          [self.consumer removeItemWithIdentifier:item
                           selectedItemIdentifier:[self activeIdentifier]];
          [self removeObservationForWebState:webState];
        } else if (moveChange.new_group() == _tabGroup.get()) {
          // The tab joined the group.
          [self insertInConsumerWebState:webState
                                 atIndex:moveChange.moved_to_index()];
          [self addObservationForWebState:webState];
        }
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      if (insertChange.group() != _tabGroup.get()) {
        break;
      }

      [self insertInConsumerWebState:insertChange.inserted_web_state()
                             atIndex:insertChange.index()];

      [self addObservationForWebState:insertChange.inserted_web_state()];
      break;
    }
    default:
      [super didChangeWebStateList:webStateList change:change status:status];
      break;
  }
  if (_tabGroup) {
    // Update the title in case the number of tabs changed.
    [_groupConsumer setGroupTitle:_tabGroup->GetTitle()];
  }
  if (status.active_web_state_change()) {
    [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
  }
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(self.webStateList, webStateList);
  [self addWebStateObservations];
  [self populateConsumerItems];
  if (_tabGroup) {
    [_groupConsumer setGroupTitle:_tabGroup->GetTitle()];
    [_groupConsumer setGroupColor:_tabGroup->GetColor()];
  } else {
    [self.tabGroupsHandler hideTabGroup];
  }
}

#pragma mark - TabGroupSyncServiceObserverDelegate

- (void)tabGroupSyncServiceTabGroupMigrated:
            (const tab_groups::SavedTabGroup&)newGroup
                                  oldSyncID:(const base::Uuid&)oldSyncId
                                 fromSource:(tab_groups::TriggerSource)source {
  if (newGroup.local_group_id() != _tabGroup->tab_group_id()) {
    return;
  }
  [self updateFacePileUI];
}

#pragma mark - Private

// Updates the facePile UI and the share state of the consumer.
- (void)updateFacePileUI {
  if (!_shareKitService || !_shareKitService->IsSupported() ||
      !_collaborationService || !_tabGroupSyncService) {
    return;
  }

  tab_groups::CollaborationId savedCollabID =
      tab_groups::utils::GetTabGroupCollabID(_tabGroup.get(),
                                             _tabGroupSyncService);
  BOOL isShared = !savedCollabID.value().empty();
  [_groupConsumer setGroupShared:isShared];

  // Prevent the face pile from being set up for tab groups that are not shared
  // and cannot be shared.
  if (!isShared &&
      !_collaborationService->GetServiceStatus().IsAllowedToCreate()) {
    return;
  }

  // Configure the face pile.
  ShareKitFacePileConfiguration* config =
      [[ShareKitFacePileConfiguration alloc] init];
  config.collabID = base::SysUTF8ToNSString(savedCollabID.value());
  config.showsEmptyState = YES;
  config.avatarSize = kFacePileAvatarSize;
  [_groupConsumer setFacePileViewController:_shareKitService->FacePile(config)];
}

// Inserts an item representing `webState` in the consumer at `index`.
- (void)insertInConsumerWebState:(web::WebState*)webState atIndex:(int)index {
  CHECK(_tabGroup);
  GridItemIdentifier* newItem = [GridItemIdentifier tabIdentifier:webState];

  GridItemIdentifier* nextItemIdentifier;
  if (index + 1 < _tabGroup->range().range_end()) {
    nextItemIdentifier = [GridItemIdentifier
        tabIdentifier:self.webStateList->GetWebStateAt(index + 1)];
  }

  [self.consumer insertItem:newItem
                beforeItemID:nextItemIdentifier
      selectedItemIdentifier:[self activeIdentifier]];
}

// Gets messages to indicate that a tab should display a chip on its cell.
- (void)fetchMessagesForChip {
  if (!_tabGroup || !_messagingService || !_messagingService->IsInitialized()) {
    return;
  }

  std::vector<collaboration::messaging::PersistentMessage> messages =
      _messagingService->GetMessagesForGroup(
          _tabGroup->tab_group_id(),
          collaboration::messaging::PersistentNotificationType::CHIP);

  for (auto& message : messages) {
    if (!message.attribution.tab_metadata.has_value()) {
      continue;
    }
    collaboration::messaging::TabMessageMetadata tab_data =
        message.attribution.tab_metadata.value();
    if (!tab_data.local_tab_id.has_value()) {
      continue;
    }
    _dirtyTabs[tab_data.local_tab_id.value()] = message;
  }
}

// Reconfigures a tab cell specified by `localTabID`.
- (void)reconfigureTab:(tab_groups::LocalTabID)localTabID {
  for (int index = 0; index < self.webStateList->count(); ++index) {
    web::WebState* webState = self.webStateList->GetWebStateAt(index);
    if (localTabID == webState->GetUniqueIdentifier().identifier()) {
      GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
      [self.consumer replaceItem:item withReplacementItem:item];
      return;
    }
  }
}

#pragma mark - MessagingBackendServiceObserving

- (void)onMessagingBackendServiceInitialized {
  [self fetchMessagesForChip];
}

- (void)displayPersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (message.type !=
      collaboration::messaging::PersistentNotificationType::CHIP) {
    return;
  }
  if (!message.attribution.tab_metadata.has_value()) {
    return;
  }
  collaboration::messaging::TabMessageMetadata tab_data =
      message.attribution.tab_metadata.value();
  if (!tab_data.local_tab_id.has_value()) {
    return;
  }
  tab_groups::LocalTabID localTabID = tab_data.local_tab_id.value();
  _dirtyTabs[localTabID] = message;

  [self reconfigureTab:localTabID];
}

- (void)hidePersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (!message.attribution.tab_metadata.has_value()) {
    return;
  }
  collaboration::messaging::TabMessageMetadata tab_data =
      message.attribution.tab_metadata.value();
  if (!tab_data.local_tab_id.has_value()) {
    return;
  }
  tab_groups::LocalTabID localTabID = tab_data.local_tab_id.value();
  _dirtyTabs.erase(localTabID);

  [self reconfigureTab:localTabID];
}

@end
