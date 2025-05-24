// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"

MessagingBackendServiceBridge::MessagingBackendServiceBridge(
    id<MessagingBackendServiceObserving> observer)
    : observer_(observer) {}

MessagingBackendServiceBridge::~MessagingBackendServiceBridge() {}

void MessagingBackendServiceBridge::OnMessagingBackendServiceInitialized() {
  const SEL selector = @selector(onMessagingBackendServiceInitialized);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ onMessagingBackendServiceInitialized];
}

void MessagingBackendServiceBridge::DisplayPersistentMessage(
    collaboration::messaging::PersistentMessage message) {
  const SEL selector = @selector(displayPersistentMessage:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ displayPersistentMessage:message];
}

void MessagingBackendServiceBridge::HidePersistentMessage(
    collaboration::messaging::PersistentMessage message) {
  const SEL selector = @selector(hidePersistentMessage:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ hidePersistentMessage:message];
}
