// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/extensions_guest_view_container_dispatcher.h"

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

namespace extensions {

ExtensionsGuestViewContainerDispatcher::
    ExtensionsGuestViewContainerDispatcher() {
}

ExtensionsGuestViewContainerDispatcher::
    ~ExtensionsGuestViewContainerDispatcher() {
}

bool ExtensionsGuestViewContainerDispatcher::HandlesMessage(
    const IPC::Message& message) {
  return GuestViewContainerDispatcher::HandlesMessage(message) ||
         (IPC_MESSAGE_CLASS(message) == ExtensionsGuestViewMsgStart);
}

}  // namespace extensions
