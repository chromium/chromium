// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_H_

#import <set>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/uuid.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/keyed_service/core/keyed_service.h"

class ProfileIOS;

namespace collaboration::messaging {

using SuccessCallback = collaboration::messaging::MessagingBackendService::
    InstantMessageDelegate::SuccessCallback;

class InstantMessagingService
    : public KeyedService,
      public collaboration::messaging::MessagingBackendService::
          InstantMessageDelegate {
 public:
  InstantMessagingService(ProfileIOS* profile);
  InstantMessagingService(const InstantMessagingService&) = delete;
  InstantMessagingService& operator=(const InstantMessagingService&) = delete;
  ~InstantMessagingService() override;

  // MessagingBackendService::InstantMessageDelegate implementation.
  void DisplayInstantaneousMessage(
      collaboration::messaging::InstantMessage message,
      MessagingBackendService::InstantMessageDelegate::SuccessCallback
          success_callback) override;
  void HideInstantaneousMessage(
      const std::set<base::Uuid>& message_ids) override;

  // Displays the out-of-date infobar if required.
  void DisplayOutOfDateMessageIfNeeded(bool should_display);

 private:
  // Shows a collaboration group infobar for the given `instant_message`.
  // Returns `true` if the infobar has been displayed.
  bool ShowCollaborationGroupInfobar(
      collaboration::messaging::InstantMessage instant_message);

  raw_ptr<ProfileIOS> profile_ = nullptr;

  base::WeakPtrFactory<InstantMessagingService> weak_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_H_
