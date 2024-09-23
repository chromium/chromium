// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <algorithm>

#include "base/containers/span.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#include "mojo/core/user_message_impl.h"

// Message deserialization may register handles in the global handle table. We
// need to initialize Core for that to be OK.
struct Environment {
  Environment() { mojo::core::InitializeCore(); }
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  static Environment environment;

  // Try using our fuzz input as the payload of an otherwise well-formed user
  // message event.
  std::unique_ptr<mojo::core::ports::UserMessageEvent> event;
  MojoResult result =
      mojo::core::UserMessageImpl::CreateEventForNewSerializedMessage(
          static_cast<uint32_t>(size), nullptr, 0, &event);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK(event);
  auto* message = event->GetMessage<mojo::core::UserMessageImpl>();
  std::copy(data, data + size,
            static_cast<unsigned char*>(message->user_payload()));
  mojo::core::Channel::MessagePtr serialized_event =
      mojo::core::UserMessageImpl::FinalizeEventMessage(std::move(event));
  mojo::core::NodeController::DeserializeMessageAsEventForFuzzer(
      std::move(serialized_event));

  return 0;
}
