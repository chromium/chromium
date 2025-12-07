// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"

#import <set>

#import "base/functional/callback.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/versioning_message_controller.h"
#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_group_infobar_delegate.h"
#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_out_of_date_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace collaboration::messaging {

InstantMessagingService::InstantMessagingService(ProfileIOS* profile)
    : profile_(profile) {
  CHECK(profile_);
  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_);
  auto* versioning_message_controller =
      tab_group_sync_service->GetVersioningMessageController();
  if (versioning_message_controller) {
    versioning_message_controller->ShouldShowMessageUiAsync(
        tab_groups::VersioningMessageController::MessageType::
            VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
        base::BindOnce(
            &InstantMessagingService::DisplayOutOfDateMessageIfNeeded,
            weak_factory_.GetWeakPtr()));
  }
}

InstantMessagingService::~InstantMessagingService() {}

void InstantMessagingService::DisplayInstantaneousMessage(
    collaboration::messaging::InstantMessage instant_message,
    MessagingBackendService::InstantMessageDelegate::SuccessCallback
        success_callback) {
  bool message_displayed = false;

  switch (instant_message.level) {
    case InstantNotificationLevel::UNDEFINED:
    case InstantNotificationLevel::SYSTEM:
      break;
    case InstantNotificationLevel::BROWSER:
      message_displayed = ShowCollaborationGroupInfobar(instant_message);
      break;
  }

  std::move(success_callback).Run(message_displayed);
}

void InstantMessagingService::HideInstantaneousMessage(
    const std::set<base::Uuid>& message_ids) {
  CollaborationGroupInfoBarDelegate::ClearCollaborationGroupInfobars(
      profile_, message_ids);
}

bool InstantMessagingService::ShowCollaborationGroupInfobar(
    collaboration::messaging::InstantMessage instant_message) {
  bool infobar_displayed =
      CollaborationGroupInfoBarDelegate::Create(profile_, instant_message);
  return infobar_displayed;
}

void InstantMessagingService::DisplayOutOfDateMessageIfNeeded(
    bool should_display) {
  if (!should_display) {
    return;
  }

  // Try to display the infobar.
  bool infobar_displayed =
      CollaborationOutOfDateInfoBarDelegate::Create(profile_);
  if (!infobar_displayed) {
    return;
  }

  // Notify the versioning message controller that the infobar was actually
  // displayed.
  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_);
  auto* versioning_message_controller =
      tab_group_sync_service->GetVersioningMessageController();
  CHECK(versioning_message_controller);
  versioning_message_controller->OnMessageUiShown(
      tab_groups::VersioningMessageController::MessageType::
          VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
}

}  // namespace collaboration::messaging
