// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_HANDLER_REGISTRY_IMPL_H_
#define IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_HANDLER_REGISTRY_IMPL_H_

#import <map>
#import <set>
#import <vector>

#import "components/sharing_message/sharing_handler_registry.h"

class SharingMessageHandler;
class SharingMessageSender;

// Interface for handling incoming SharingMessage.
class IOSSharingHandlerRegistryImpl : public SharingHandlerRegistry {
 public:
  IOSSharingHandlerRegistryImpl(SharingMessageSender* message_sender);
  ~IOSSharingHandlerRegistryImpl() override;

  // SharingHandlerRegistry:
  SharingMessageHandler* GetSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;
  void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;
  void UnregisterSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;
};

#endif  // IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_HANDLER_REGISTRY_IMPL_H_
