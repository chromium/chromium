// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/messaging/messaging_backend_service.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_log_item.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {

// The size of image of a user's icon and a favicon.
const CGFloat kImageSize = 22;

ActivityLogType ConvertUserAction(
    tab_groups::messaging::UserAction user_action) {
  switch (user_action) {
    case tab_groups::messaging::UserAction::TAB_ADDED:
      return ActivityLogType::kTabAdded;
    case tab_groups::messaging::UserAction::TAB_REMOVED:
      return ActivityLogType::kTabRemoved;
    case tab_groups::messaging::UserAction::TAB_NAVIGATED:
      return ActivityLogType::kTabNavigated;
    case tab_groups::messaging::UserAction::TAB_GROUP_VISUALS_UPDATED:
      // TODO(crbug.com/370897655): Confirm how to know that a user changed a
      // color or a name of a group. Return kGroupNameChanged if the name is
      // updated.
      return ActivityLogType::kGroupColorChanged;
    case tab_groups::messaging::UserAction::COLLABORATION_USER_LEFT:
      return ActivityLogType::kUserLeft;
    default:
      return ActivityLogType::kUndefined;
  }
}

std::string CollaborationId(
    raw_ptr<tab_groups::TabGroupSyncService> syncService,
    base::WeakPtr<const TabGroup> tabGroup) {
  if (!tabGroup || !syncService) {
    return "";
  }

  std::optional<tab_groups::SavedTabGroup> savedTabGroup =
      syncService->GetGroup(tabGroup->tab_group_id());
  if (savedTabGroup.has_value() &&
      savedTabGroup->collaboration_id().has_value()) {
    return savedTabGroup->collaboration_id().value();
  }
  return "";
}

}  // namespace

@implementation RecentActivityMediator {
  // A shared tab group currently displayed.
  base::WeakPtr<const TabGroup> _tabGroup;
  // A service to get logs for Recent Activity.
  raw_ptr<tab_groups::messaging::MessagingBackendService> _messagingService;
  // A service to get a favicon image.
  raw_ptr<FaviconLoader> _faviconLoader;
  // A service to get the information of a tab group.
  raw_ptr<tab_groups::TabGroupSyncService> _syncService;
}

- (instancetype)initWithtabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                messagingService:
                    (tab_groups::messaging::MessagingBackendService*)
                        messagingService
                   faviconLoader:(FaviconLoader*)faviconLoader
                     syncService:(tab_groups::TabGroupSyncService*)syncService {
  CHECK(messagingService);
  CHECK(faviconLoader);
  CHECK(syncService);
  if ((self = [super init])) {
    _messagingService = messagingService;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
  }
  return self;
}

- (void)setConsumer:(id<RecentActivityConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self populateItemsFromService];
  }
}

#pragma mark - Private

// Creates recent activity logs and passes them to the consumer.
- (void)populateItemsFromService {
  tab_groups::messaging::ActivityLogQueryParams params;
  params.collaboration_id =
      data_sharing::GroupId(CollaborationId(_syncService, _tabGroup));

  NSMutableArray<RecentActivityLogItem*>* items = [[NSMutableArray alloc] init];

  for (auto& log : _messagingService->GetActivityLog(params)) {
    RecentActivityLogItem* item = [[RecentActivityLogItem alloc] init];
    item.type = ConvertUserAction(log.user_action_type);
    item.title = base::SysUTF8ToNSString(log.title_text);
    item.actionDescription = base::SysUTF8ToNSString(log.description_text);
    item.timestamp = base::SysUTF8ToNSString(log.timestamp_text);

    // Get a favicon from the URL and set it to `item`.
    if (log.activity_metadata.tab_metadata.has_value()) {
      _faviconLoader->FaviconForPageUrlOrHost(
          GURL(log.activity_metadata.tab_metadata.value()
                   .last_known_url.value()),
          kImageSize, ^(FaviconAttributes* attributes) {
            item.favicon = attributes.faviconImage;
          });
    }

    // TODO(crbug.com/370897655): Get a correct user icon.
    item.userIcon =
        DefaultSymbolTemplateWithPointSize(kXMarkCircleFillSymbol, 20);

    [items addObject:item];
  }

  [_consumer populateItems:items];
}

@end
