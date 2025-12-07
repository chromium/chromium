// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/atomic_sequence_num.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace IPC {

Message::Message() : base::Pickle(sizeof(Header)) {}

Message::Message(const char* data, size_t data_len)
    : base::Pickle(base::Pickle::kUnownedData,
                   base::as_bytes(UNSAFE_TODO(base::span(data, data_len)))) {}

Message::Message(const Message& other) : base::Pickle(other) {
  attachment_set_ = other.attachment_set_;
}

Message::~Message() = default;

Message& Message::operator=(const Message& other) {
  *static_cast<base::Pickle*>(this) = other;
  attachment_set_ = other.attachment_set_;
  return *this;
}

void Message::EnsureMessageAttachmentSet() {
  if (!attachment_set_.get())
    attachment_set_ = new MessageAttachmentSet;
}


bool Message::WriteAttachment(
    scoped_refptr<base::Pickle::Attachment> attachment) {
  size_t index;
  bool success = attachment_set()->AddAttachment(
      base::WrapRefCounted(static_cast<MessageAttachment*>(attachment.get())),
      &index);
  DCHECK(success);

  // Write the index of the descriptor so that we don't have to
  // keep the current descriptor as extra decoding state when deserialising.
  WriteInt(static_cast<int>(index));

  return success;
}

bool Message::ReadAttachment(
    base::PickleIterator* iter,
    scoped_refptr<base::Pickle::Attachment>* attachment) const {
  int index;
  if (!iter->ReadInt(&index))
    return false;

  MessageAttachmentSet* attachment_set = attachment_set_.get();
  if (!attachment_set)
    return false;

  *attachment = attachment_set->GetAttachmentAt(index);

  return nullptr != attachment->get();
}

bool Message::HasAttachments() const {
  return attachment_set_.get() && !attachment_set_->empty();
}

}  // namespace IPC
