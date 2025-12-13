// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_command.h"

#import "components/collaboration/public/collaboration_service.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/device_form_factor.h"

@implementation SharedTabGroupLastTabAlertCommand

- (instancetype)initWithTabID:(web::WebStateID)tabID
                      browser:(Browser*)browser
                        group:(const TabGroup*)group
           baseViewController:(UIViewController*)baseViewController
                   sourceView:(UIView*)sourceView
                      closing:(BOOL)closing {
  if ((self = [super init])) {
    _closing = closing;
    _canCancel = closing;
    _tabID = tabID;
    _group = group;
    _groupTitle = group->GetTitle();
    _baseViewController = baseViewController;
    _sourceView = sourceView;

    ProfileIOS* profile = browser->GetProfile();
    tab_groups::TabGroupSyncService* tabGroupSyncService =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
    collaboration::CollaborationService* collaborationService =
        collaboration::CollaborationServiceFactory::GetForProfile(profile);
    data_sharing::MemberRole userRole = tab_groups::utils::GetUserRoleForGroup(
        group, tabGroupSyncService, collaborationService);

    switch (userRole) {
      case data_sharing::MemberRole::kOwner: {
        _actionType = TabGroupActionType::kDeleteOrKeepSharedTabGroup;
        break;
      }
      case data_sharing::MemberRole::kMember: {
        _actionType = TabGroupActionType::kLeaveOrKeepSharedTabGroup;
        break;
      }
      case data_sharing::MemberRole::kUnknown: {
        _actionType = TabGroupActionType::kCloseLastTabUnknownRole;
        break;
      }
      case data_sharing::MemberRole::kInvitee:
      case data_sharing::MemberRole::kFormerMember:
        NOTREACHED();
    }

    _displayAsAlert = !_sourceView;
  }
  return self;
}

@end
