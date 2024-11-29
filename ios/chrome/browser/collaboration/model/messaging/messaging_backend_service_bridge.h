// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_BRIDGE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_BRIDGE_H_

#import "components/collaboration/public/messaging/messaging_backend_service.h"

namespace collaboration::messaging {
struct PersistentMessage;
}  // namespace collaboration::messaging

// Protocol that corresponds to
// MessagingBackendService::PersistentMessageObserver API. Allows registering
// Objective-C objects to listen to MessagingBackendService events.
@protocol MessagingBackendServiceObserving <NSObject>

@optional

// Invoked once when the service is initialized.
- (void)onMessagingBackendServiceInitialized;

// Invoked when the frontend needs to display a specific persistent message.
- (void)displayPersistentMessage:
    (collaboration::messaging::PersistentMessage)message;

// Invoked when the frontend needs to hide a specific persistent message.
- (void)hidePersistentMessage:
    (collaboration::messaging::PersistentMessage)message;

@end

// Observer that bridges MessagingBackendService events to an Objective-C
// observer that implements the MessagingBackendServiceObserving protocol (the
// observer is *not* owned).
class MessagingBackendServiceBridge final
    : public collaboration::messaging::MessagingBackendService::
          PersistentMessageObserver {
 public:
  explicit MessagingBackendServiceBridge(
      id<MessagingBackendServiceObserving> observer);

  MessagingBackendServiceBridge(const MessagingBackendServiceBridge&) = delete;
  MessagingBackendServiceBridge& operator=(
      const MessagingBackendServiceBridge&) = delete;

  ~MessagingBackendServiceBridge() final;

 private:
  // MessagingBackendService::PersistentMessageObserver implementation.
  void OnMessagingBackendServiceInitialized() override;
  void DisplayPersistentMessage(
      collaboration::messaging::PersistentMessage message) override;
  void HidePersistentMessage(
      collaboration::messaging::PersistentMessage message) override;
  __weak id<MessagingBackendServiceObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_BRIDGE_H_
