// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"

#import <set>

#import "base/functional/callback.h"
#import "base/uuid.h"
#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_group_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace collaboration::messaging {

InstantMessagingService::InstantMessagingService(ProfileIOS* profile)
    : profile_(profile) {
  DCHECK(profile_);
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
  // TODO(crbug.com/416265501) Implement this.
}

bool InstantMessagingService::ShowCollaborationGroupInfobar(
    collaboration::messaging::InstantMessage instant_message) {
  bool infobar_displayed =
      CollaborationGroupInfoBarDelegate::Create(profile_, instant_message);
  return infobar_displayed;
}

}  // namespace collaboration::messaging
