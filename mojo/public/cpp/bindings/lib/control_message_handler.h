// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_CONTROL_MESSAGE_HANDLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_CONTROL_MESSAGE_HANDLER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class InterfaceEndpointClient;

namespace internal {

// Handlers for request messages defined in interface_control_messages.mojom.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ControlMessageHandler
    : public MessageReceiverWithResponderStatus {
 public:
  static bool IsControlMessage(const Message* message);

  ControlMessageHandler(InterfaceEndpointClient* owner,
                        uint32_t interface_version);
  ~ControlMessageHandler() override;

  // Call the following methods only if IsControlMessage() returned true.
  bool Accept(Message* message) override;
  bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiverWithStatus> responder) override;

 private:
  bool Run(Message* message,
           std::unique_ptr<MessageReceiverWithStatus> responder);
  bool RunOrClosePipe(Message* message);

  InterfaceEndpointClient* const owner_;
  uint32_t interface_version_;
  SerializationContext context_;

  DISALLOW_COPY_AND_ASSIGN(ControlMessageHandler);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_CONTROL_MESSAGE_HANDLER_H_
