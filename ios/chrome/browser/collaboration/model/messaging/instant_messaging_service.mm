// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"

#import "base/functional/callback.h"

namespace collaboration::messaging {

InstantMessagingService::InstantMessagingService() {}
InstantMessagingService::~InstantMessagingService() {}

void InstantMessagingService::DisplayInstantaneousMessage(
    collaboration::messaging::InstantMessage message,
    MessagingBackendService::InstantMessageDelegate::SuccessCallback
        success_callback) {
  // TODO(crbug.com/375595834): Send the message to the UI components.

  // Inform the backend that the message was displayed.
  std::move(success_callback).Run(true);
  // TODO(crbug.com/375595834): Call the callback with `false` when showing the
  // message fails.
}

}  // namespace collaboration::messaging
