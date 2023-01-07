// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_MESSAGE_HELPER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_MESSAGE_HELPER_H_

#include <memory>

#include "base/component_export.h"

namespace mojo {

class Message;
class MessageReceiver;
class MessageReceiverWithResponder;

namespace internal {

// Helpers to send a given mojo message to a given receiver.
// Extracted in a separate function to ensure that operations like emitting
// trace events can be performed without affecting the binary size of the
// generated bindings.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SendMojoMessage(MessageReceiverWithResponder& receiver,
                     Message& message,
                     std::unique_ptr<MessageReceiver> responder);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SendMojoMessage(MessageReceiver& receiver, Message& message);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_MESSAGE_HELPER_H_
