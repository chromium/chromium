// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/data_sharing/public/group_data.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/recent_activity_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/navigation/navigation_manager.h"

using collaboration::messaging::MessageAttribution;
using collaboration::messaging::RecentActivityAction;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;

namespace {

// The size of a favicon image.
const CGFloat kFaviconSize = 20;

// The maximum number of logs to query, and thus display in the list.
const int kMaxNumberOfLogs = 5;

}  // namespace

@implementation RecentActivityMediator {
  // A shared tab group currently displayed.
  base::WeakPtr<const TabGroup> _tabGroup;
  // A service to get logs for Recent Activity.
  raw_ptr<collaboration::messaging::MessagingBackendService> _messagingService;
  // A service to get a favicon image.
  raw_ptr<FaviconLoader> _faviconLoader;
  // A service to get the information of a tab group.
  raw_ptr<tab_groups::TabGroupSyncService> _syncService;
  // A service to get the information of a shared tab group.
  raw_ptr<ShareKitService> _shareKitService;
  // The WebStateList.
  raw_ptr<WebStateList> _webStateList;
  // Creation parameter for web states.
  std::unique_ptr<web::WebState::CreateParams> _webStateCreationParams;
}

- (instancetype)initWithTabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                messagingService:
                    (collaboration::messaging::MessagingBackendService*)
                        messagingService
                   faviconLoader:(FaviconLoader*)faviconLoader
                     syncService:(tab_groups::TabGroupSyncService*)syncService
                 shareKitService:(ShareKitService*)shareKitService
                    webStateList:(WebStateList*)webStateList
          webStateCreationParams:
              (const web::WebState::CreateParams&)webStateCreationParams {
  CHECK(messagingService);
  CHECK(faviconLoader);
  CHECK(syncService);
  CHECK(shareKitService);
  if ((self = [super init])) {
    _tabGroup = tabGroup;
    _messagingService = messagingService;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
    _shareKitService = shareKitService;
    _webStateList = webStateList;
    _webStateCreationParams =
        std::make_unique<web::WebState::CreateParams>(webStateCreationParams);
  }
  return self;
}

- (void)setConsumer:(id<RecentActivityConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self populateItemsFromService];
  }
}

#pragma mark - RecentActivityMutator

- (void)performActionForItem:(RecentActivityLogItem*)item {
  switch (item.action) {
    case RecentActivityAction::kFocusTab:
      [self focusTabForItem:item];
      break;
    case RecentActivityAction::kReopenTab:
      [self reopenTabForItem:item];
      break;
    case RecentActivityAction::kOpenTabGroupEditDialog:
      [self showEditScreenForItem:item];
      break;
    case RecentActivityAction::kManageSharing:
      [self showManageScreenForItem:item];
      break;
    case RecentActivityAction::kNone:
      NOTREACHED();
  }
}

#pragma mark - Private

// Focues the tab passed in `item`.
- (void)focusTabForItem:(RecentActivityLogItem*)item {
  std::optional<TabMessageMetadata> tabMetadata =
      item.activityMetadata.tab_metadata;
  CHECK(tabMetadata.has_value());

  std::optional<tab_groups::LocalTabID> localTabID = tabMetadata->local_tab_id;
  CHECK(localTabID.has_value());

  web::WebStateID localID =
      web::WebStateID::FromSerializedValue(localTabID.value());
  int tabIndex = GetWebStateIndex(
      _webStateList, WebStateSearchCriteria{.identifier = localID});

  if (tabIndex == WebStateList::kInvalidIndex) {
    // Return early if tab was closed.
    return;
  }

  _webStateList->ActivateWebStateAt(tabIndex);
  [self.recentActivityHandler dismissViewAndExitTabGrid];
}

// Reopens the tab from `item` in the correct group.
- (void)reopenTabForItem:(RecentActivityLogItem*)item {
  std::optional<TabMessageMetadata> tabMetadata =
      item.activityMetadata.tab_metadata;
  CHECK(tabMetadata.has_value());

  const TabGroup* group = [self tabGroupForItem:item];
  if (!group) {
    return;
  }

  std::optional<std::string> lastURLString = tabMetadata->last_known_url;
  CHECK(lastURLString.has_value());

  GURL URLToRestore(lastURLString.value());
  CHECK(URLToRestore.is_valid());

  web::NavigationManager::WebLoadParams web_load_params(URLToRestore);
  std::unique_ptr<web::WebState> webState =
      web::WebState::Create(*_webStateCreationParams.get());
  webState->GetNavigationManager()->LoadURLWithParams(web_load_params);

  WebStateList::InsertionParams params =
      WebStateList::InsertionParams::AtIndex(_webStateList->count());
  params.Activate().InGroup(group);

  _webStateList->InsertWebState(std::move(webState), params);
  [self.recentActivityHandler dismissViewAndExitTabGrid];
}

// Shows the management screen for the group in `item`.
- (void)showManageScreenForItem:(RecentActivityLogItem*)item {
  const TabGroup* group = [self tabGroupForItem:item];
  [self.recentActivityHandler showManageScreenForGroup:group];
}

// Shows the edit screen for the group in `item`.
- (void)showEditScreenForItem:(RecentActivityLogItem*)item {
  const TabGroup* group = [self tabGroupForItem:item];
  [self.recentActivityHandler showTabGroupEditForGroup:group];
}

// Returns the local group associated `item` data.
- (const TabGroup*)tabGroupForItem:(RecentActivityLogItem*)item {
  std::optional<TabGroupMessageMetadata> tabGroupMetadata =
      item.activityMetadata.tab_group_metadata;
  CHECK(tabGroupMetadata.has_value());

  std::set<const TabGroup*> groups = _webStateList->GetGroups();
  for (const TabGroup* group : groups) {
    if (group->tab_group_id() == tabGroupMetadata->local_tab_group_id) {
      return group;
    }
  }

  return nullptr;
}

// Creates recent activity logs and passes them to the consumer.
- (void)populateItemsFromService {
  collaboration::messaging::ActivityLogQueryParams params;
  params.result_length = kMaxNumberOfLogs;
  syncer::CollaborationId collabID =
      tab_groups::utils::GetTabGroupCollabID(_tabGroup.get(), _syncService);
  params.collaboration_id = data_sharing::GroupId(collabID.value());

  NSMutableArray<RecentActivityLogItem*>* items = [[NSMutableArray alloc] init];

  for (collaboration::messaging::ActivityLogItem& log :
       _messagingService->GetActivityLog(params)) {
    RecentActivityLogItem* item = [[RecentActivityLogItem alloc] init];
    if (log.show_favicon && log.activity_metadata.tab_metadata.has_value() &&
        log.activity_metadata.tab_metadata.value().last_known_url.has_value()) {
      item.faviconURL = GURL(
          log.activity_metadata.tab_metadata.value().last_known_url.value());
    }
    item.title = base::SysUTF16ToNSString(log.title_text);
    item.actionDescription = base::SysUTF16ToNSString(log.description_text);
    item.elapsedTime = base::SysUTF16ToNSString(log.time_delta_text);
    item.action = log.action;
    item.activityMetadata = log.activity_metadata;

    UIImage* defaultFavicon = SymbolWithPalette(
        DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kFaviconSize),
        @[ [UIColor colorNamed:kGrey400Color] ]);
    item.attributes = [FaviconAttributes attributesWithImage:defaultFavicon];

    // Get a user's icon from the avatar URL and set it to `item`.
    // The image is asynchronously loaded.
    if (_shareKitService->IsSupported()) {
      std::optional<data_sharing::GroupMember> user =
          log.activity_metadata.triggering_user
              ? log.activity_metadata.triggering_user
              : log.activity_metadata.affected_user;

      if (user.has_value()) {
        ShareKitAvatarConfiguration* config =
            [[ShareKitAvatarConfiguration alloc] init];
        config.avatarUrl = [NSURL
            URLWithString:base::SysUTF8ToNSString(user->avatar_url.spec())];
        // Use email intead when the display name is empty.
        config.displayName = user->display_name.empty()
                                 ? base::SysUTF8ToNSString(user->email)
                                 : base::SysUTF8ToNSString(user->display_name);
        config.avatarSize = CGSizeMake(kRecentActivityLogAvatarSize,
                                       kRecentActivityLogAvatarSize);
        item.avatarPrimitive = _shareKitService->AvatarImage(config);
      }
    }

    [items addObject:item];
  }

  [_consumer populateItems:items];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes* attributes,
                                    bool cached))completion {
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kFaviconSize, kFaviconSize,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        if (attributes.faviconImage) {
          completion(attributes, cached);
        }
      });
}

@end
