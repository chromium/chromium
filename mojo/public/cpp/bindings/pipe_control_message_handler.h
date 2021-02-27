// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_HANDLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class PipeControlMessageHandlerDelegate;

// Handler for messages defined in pipe_control_messages.mojom.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) PipeControlMessageHandler
    : public MessageReceiver {
 public:
  explicit PipeControlMessageHandler(
      PipeControlMessageHandlerDelegate* delegate);
  ~PipeControlMessageHandler() override;

  // Sets the description for this handler. Used only when reporting validation
  // errors.
  void SetDescription(const std::string& description);

  // NOTE: |message| must have passed message header validation.
  static bool IsPipeControlMessage(const Message* message);

  // MessageReceiver implementation:

  // NOTE: |message| must:
  //   - have passed message header validation; and
  //   - be a pipe control message (i.e., IsPipeControlMessage() returns true).
  // If the method returns false, the message pipe should be closed.
  bool Accept(Message* message) override;

 private:
  // |message| must have passed message header validation.
  bool Validate(Message* message);
  bool RunOrClosePipe(Message* message);

  std::string description_;
  PipeControlMessageHandlerDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(PipeControlMessageHandler);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_HANDLER_H_
