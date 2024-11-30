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
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_item.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {

// The size of a favicon image.
const CGFloat kFaviconSize = 20;
// The size of an avatar image.
const CGFloat kAvatarSize = 30;

ActivityLogType ConvertCollaborationEvent(
    collaboration::messaging::CollaborationEvent collaboration_event) {
  switch (collaboration_event) {
    case collaboration::messaging::CollaborationEvent::TAB_ADDED:
      return ActivityLogType::kTabAdded;
    case collaboration::messaging::CollaborationEvent::TAB_REMOVED:
      return ActivityLogType::kTabRemoved;
    case collaboration::messaging::CollaborationEvent::TAB_UPDATED:
      return ActivityLogType::kTabUpdated;
    case collaboration::messaging::CollaborationEvent::TAB_GROUP_NAME_UPDATED:
      return ActivityLogType::kGroupNameChanged;
    case collaboration::messaging::CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      return ActivityLogType::kGroupColorChanged;
    case collaboration::messaging::CollaborationEvent::
        COLLABORATION_MEMBER_REMOVED:
      return ActivityLogType::kMemberRemoved;
    default:
      return ActivityLogType::kUndefined;
  }
}

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
}

- (instancetype)initWithtabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                messagingService:
                    (collaboration::messaging::MessagingBackendService*)
                        messagingService
                   faviconLoader:(FaviconLoader*)faviconLoader
                     syncService:(tab_groups::TabGroupSyncService*)syncService
                 shareKitService:(ShareKitService*)shareKitService {
  CHECK(messagingService);
  CHECK(faviconLoader);
  CHECK(syncService);
  CHECK(shareKitService);
  if ((self = [super init])) {
    _messagingService = messagingService;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
    _shareKitService = shareKitService;
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
  collaboration::messaging::ActivityLogQueryParams params;
  NSString* collabID =
      tab_groups::utils::GetTabGroupCollabID(_tabGroup.get(), _syncService);
  params.collaboration_id =
      data_sharing::GroupId(base::SysNSStringToUTF8(collabID));

  NSMutableArray<RecentActivityLogItem*>* items = [[NSMutableArray alloc] init];

  for (auto& log : _messagingService->GetActivityLog(params)) {
    RecentActivityLogItem* item = [[RecentActivityLogItem alloc] init];
    item.type = ConvertCollaborationEvent(log.collaboration_event);
    item.title = base::SysUTF8ToNSString(log.title_text);
    item.actionDescription = base::SysUTF8ToNSString(log.description_text);
    item.timestamp = base::SysUTF8ToNSString(log.timestamp_text);

    // Get a favicon from the URL and set it to `item`.
    if (log.activity_metadata.tab_metadata.has_value()) {
      _faviconLoader->FaviconForPageUrlOrHost(
          GURL(log.activity_metadata.tab_metadata.value()
                   .last_known_url.value()),
          kFaviconSize, ^(FaviconAttributes* attributes) {
            item.favicon = attributes.faviconImage;
          });
    }

    // Get a user's icon from the avatar URL and set it to `item`.
    // The image is asynchronously loaded.
    if (_shareKitService->IsSupported() &&
        log.activity_metadata.triggering_user.has_value()) {
      ShareKitAvatarConfiguration* config =
          [[ShareKitAvatarConfiguration alloc] init];
      data_sharing::GroupMember user =
          log.activity_metadata.triggering_user.value();
      config.avatarUrl =
          [NSURL URLWithString:base::SysUTF8ToNSString(user.avatar_url.spec())];
      // Use email intead when the display name is empty.
      config.displayName = user.display_name.empty()
                               ? base::SysUTF8ToNSString(user.email)
                               : base::SysUTF8ToNSString(user.display_name);
      config.avatarSize = CGSizeMake(kAvatarSize, kAvatarSize);
      item.avatarPrimitive = _shareKitService->AvatarImage(config);
    }

    [items addObject:item];
  }

  [_consumer populateItems:items];
}

@end
