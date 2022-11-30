// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_message_helper.h"

#include <utility>

#include "base/logging.h"
#include "ipc/ipc_mojo_handle_attachment.h"

namespace IPC {

// static
bool MojoMessageHelper::WriteMessagePipeTo(
    base::Pickle* message,
    mojo::ScopedMessagePipeHandle handle) {
  message->WriteAttachment(new internal::MojoHandleAttachment(
      mojo::ScopedHandle::From(std::move(handle))));
  return true;
}

// static
bool MojoMessageHelper::ReadMessagePipeFrom(
    const base::Pickle* message,
    base::PickleIterator* iter,
    mojo::ScopedMessagePipeHandle* handle) {
  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!message->ReadAttachment(iter, &attachment)) {
    LOG(ERROR) << "Failed to read attachment for message pipe.";
    return false;
  }

  MessageAttachment::Type type =
      static_cast<MessageAttachment*>(attachment.get())->GetType();
  if (type != MessageAttachment::Type::MOJO_HANDLE) {
    LOG(ERROR) << "Unxpected attachment type:" << static_cast<int>(type);
    return false;
  }

  handle->reset(mojo::MessagePipeHandle(
      static_cast<internal::MojoHandleAttachment*>(attachment.get())
          ->TakeHandle()
          .release()
          .value()));
  return true;
}

MojoMessageHelper::MojoMessageHelper() = default;

}  // namespace IPC
