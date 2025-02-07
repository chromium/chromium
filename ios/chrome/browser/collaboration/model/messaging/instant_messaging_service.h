// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_H_

#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/keyed_service/core/keyed_service.h"

namespace collaboration::messaging {

using SuccessCallback = collaboration::messaging::MessagingBackendService::
    InstantMessageDelegate::SuccessCallback;

class InstantMessagingService
    : public KeyedService,
      public collaboration::messaging::MessagingBackendService::
          InstantMessageDelegate {
 public:
  InstantMessagingService();
  InstantMessagingService(const InstantMessagingService&) = delete;
  InstantMessagingService& operator=(const InstantMessagingService&) = delete;
  ~InstantMessagingService() override;

  // MessagingBackendService::InstantMessageDelegate implementation.
  void DisplayInstantaneousMessage(
      collaboration::messaging::InstantMessage message,
      MessagingBackendService::InstantMessageDelegate::SuccessCallback
          success_callback) override;
};

}  // namespace collaboration::messaging

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INSTANT_MESSAGING_SERVICE_H_
