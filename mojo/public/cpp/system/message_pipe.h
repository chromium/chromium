// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a C++ wrapping around the Mojo C API for message pipes,
// replacing the prefix of "Mojo" with a "mojo" namespace, and using more
// strongly-typed representations of |MojoHandle|s.
//
// Please see "mojo/public/c/system/message_pipe.h" for complete documentation
// of the API.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_MESSAGE_PIPE_H_

#include <stdint.h>

#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A strongly-typed representation of a |MojoHandle| to one end of a message
// pipe.
class MessagePipeHandle : public Handle {
 public:
  MessagePipeHandle() {}
  explicit MessagePipeHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(MessagePipeHandle) == sizeof(Handle),
              "Bad size for C++ MessagePipeHandle");

typedef ScopedHandleBase<MessagePipeHandle> ScopedMessagePipeHandle;
static_assert(sizeof(ScopedMessagePipeHandle) == sizeof(MessagePipeHandle),
              "Bad size for C++ ScopedMessagePipeHandle");

// Creates a message pipe. See |MojoCreateMessagePipe()| for complete
// documentation.
inline MojoResult CreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                                    ScopedMessagePipeHandle* message_pipe0,
                                    ScopedMessagePipeHandle* message_pipe1) {
  DCHECK(message_pipe0);
  DCHECK(message_pipe1);
  MessagePipeHandle handle0;
  MessagePipeHandle handle1;
  MojoResult rv = MojoCreateMessagePipe(
      options, handle0.mutable_value(), handle1.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  message_pipe0->reset(handle0);
  message_pipe1->reset(handle1);
  return rv;
}

// A helper for writing a serialized message to a message pipe. Use this for
// convenience in lieu of the lower-level MojoWriteMessage API, but beware that
// it does incur an extra copy of the message payload.
//
// See documentation for MojoWriteMessage for return code details.
MOJO_CPP_SYSTEM_EXPORT MojoResult
WriteMessageRaw(MessagePipeHandle message_pipe,
                const void* bytes,
                size_t num_bytes,
                const MojoHandle* handles,
                size_t num_handles,
                MojoWriteMessageFlags flags);

// A helper for reading serialized messages from a pipe. Use this for
// convenience in lieu of the lower-level MojoReadMessage API, but beware that
// it does incur an extra copy of the message payload.
//
// See documentation for MojoReadMessage for return code details. In addition to
// those return codes, this may return |MOJO_RESULT_ABORTED| if the message was
// unable to be serialized into the provided containers.
MOJO_CPP_SYSTEM_EXPORT MojoResult
ReadMessageRaw(MessagePipeHandle message_pipe,
               std::vector<uint8_t>* payload,
               std::vector<ScopedHandle>* handles,
               MojoReadMessageFlags flags);

// Writes to a message pipe. Takes ownership of |message| and any attached
// handles.
inline MojoResult WriteMessageNew(MessagePipeHandle message_pipe,
                                  ScopedMessageHandle message,
                                  MojoWriteMessageFlags flags) {
  MojoWriteMessageOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  return MojoWriteMessage(message_pipe.value(), message.release().value(),
                          &options);
}

// Reads from a message pipe. See |MojoReadMessage()| for complete
// documentation.
inline MojoResult ReadMessageNew(MessagePipeHandle message_pipe,
                                 ScopedMessageHandle* message,
                                 MojoReadMessageFlags flags) {
  MojoReadMessageOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoMessageHandle raw_message;
  MojoResult rv = MojoReadMessage(message_pipe.value(), &options, &raw_message);
  if (rv != MOJO_RESULT_OK)
    return rv;

  message->reset(MessageHandle(raw_message));
  return MOJO_RESULT_OK;
}

// Fuses two message pipes together at the given handles. See
// |MojoFuseMessagePipes()| for complete documentation.
inline MojoResult FuseMessagePipes(ScopedMessagePipeHandle message_pipe0,
                                   ScopedMessagePipeHandle message_pipe1) {
  return MojoFuseMessagePipes(message_pipe0.release().value(),
                              message_pipe1.release().value(), nullptr);
}

// A wrapper class that automatically creates a message pipe and owns both
// handles.
class MessagePipe {
 public:
  MessagePipe();
  explicit MessagePipe(const MojoCreateMessagePipeOptions& options);
  ~MessagePipe();

  ScopedMessagePipeHandle handle0;
  ScopedMessagePipeHandle handle1;
};

inline MessagePipe::MessagePipe() {
  [[maybe_unused]] MojoResult result =
      CreateMessagePipe(nullptr, &handle0, &handle1);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK(handle0.is_valid());
  DCHECK(handle1.is_valid());
}

inline MessagePipe::MessagePipe(const MojoCreateMessagePipeOptions& options) {
  [[maybe_unused]] MojoResult result =
      CreateMessagePipe(&options, &handle0, &handle1);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK(handle0.is_valid());
  DCHECK(handle1.is_valid());
}

inline MessagePipe::~MessagePipe() {
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_MESSAGE_PIPE_H_
