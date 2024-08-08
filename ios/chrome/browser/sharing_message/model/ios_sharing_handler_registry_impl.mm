// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_handler_registry_impl.h"

#import "base/notreached.h"
#import "build/build_config.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/sharing_message/ack_message_handler.h"
#import "components/sharing_message/ping_message_handler.h"
#import "components/sharing_message/sharing_device_registration.h"
#import "components/sharing_message/sharing_message_handler.h"
#import "components/sharing_message/sharing_message_sender.h"

IOSSharingHandlerRegistryImpl::IOSSharingHandlerRegistryImpl(
    SharingMessageSender* message_sender) {}

IOSSharingHandlerRegistryImpl::~IOSSharingHandlerRegistryImpl() = default;

SharingMessageHandler* IOSSharingHandlerRegistryImpl::GetSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  return nullptr;
}

void IOSSharingHandlerRegistryImpl::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  NOTREACHED();
}

void IOSSharingHandlerRegistryImpl::UnregisterSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  NOTREACHED();
}
