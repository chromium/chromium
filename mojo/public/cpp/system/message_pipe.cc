// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/system/message_pipe.h"

#include <algorithm>
#include <cstring>

#include "base/numerics/safe_math.h"

namespace mojo {

MojoResult WriteMessageRaw(MessagePipeHandle message_pipe,
                           const void* bytes,
                           size_t num_bytes,
                           const MojoHandle* handles,
                           size_t num_handles,
                           MojoWriteMessageFlags flags) {
  ScopedMessageHandle message_handle;
  MojoResult rv = CreateMessage(&message_handle, MOJO_CREATE_MESSAGE_FLAG_NONE);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  MojoAppendMessageDataOptions append_options;
  append_options.struct_size = sizeof(append_options);
  append_options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  void* buffer;
  uint32_t buffer_size;
  rv = MojoAppendMessageData(message_handle->value(),
                             base::checked_cast<uint32_t>(num_bytes), handles,
                             base::checked_cast<uint32_t>(num_handles),
                             &append_options, &buffer, &buffer_size);
  if (rv != MOJO_RESULT_OK)
    return MOJO_RESULT_ABORTED;

  DCHECK(buffer);
  DCHECK_GE(buffer_size, base::checked_cast<uint32_t>(num_bytes));
  if (num_bytes > 0) {
    memcpy(buffer, bytes, num_bytes);
  }

  MojoWriteMessageOptions write_options;
  write_options.struct_size = sizeof(write_options);
  write_options.flags = flags;
  return MojoWriteMessage(message_pipe.value(),
                          message_handle.release().value(), &write_options);
}

MojoResult ReadMessageRaw(MessagePipeHandle message_pipe,
                          std::vector<uint8_t>* payload,
                          std::vector<ScopedHandle>* handles,
                          MojoReadMessageFlags flags) {
  ScopedMessageHandle message_handle;
  MojoResult rv = ReadMessageNew(message_pipe, &message_handle, flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  rv = MojoSerializeMessage(message_handle->value(), nullptr);
  if (rv != MOJO_RESULT_OK && rv != MOJO_RESULT_FAILED_PRECONDITION)
    return MOJO_RESULT_ABORTED;

  void* buffer = nullptr;
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  rv = MojoGetMessageData(message_handle->value(), nullptr, &buffer, &num_bytes,
                          nullptr, &num_handles);
  if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    DCHECK(handles);
    handles->resize(num_handles);
    rv = MojoGetMessageData(
        message_handle->value(), nullptr, &buffer, &num_bytes,
        reinterpret_cast<MojoHandle*>(handles->data()), &num_handles);
  }

  if (num_bytes) {
    DCHECK(buffer);
    uint8_t* payload_data = reinterpret_cast<uint8_t*>(buffer);
    payload->resize(num_bytes);
    std::copy(payload_data, payload_data + num_bytes, payload->begin());
  } else if (payload) {
    payload->clear();
  }

  if (handles && !num_handles)
    handles->clear();

  if (rv != MOJO_RESULT_OK)
    return MOJO_RESULT_ABORTED;

  return MOJO_RESULT_OK;
}

}  // namespace mojo
