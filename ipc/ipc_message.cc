// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ipc/ipc_message.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/atomic_sequence_num.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace {

base::AtomicSequenceNumber g_ref_num;

// Create a reference number for identifying IPC messages in traces. The return
// values has the reference number stored in the upper 24 bits, leaving the low
// 8 bits set to 0 for use as flags.
inline uint32_t GetRefNumUpper24() {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();
  uint32_t pid = trace_log ? trace_log->process_id() : 0;
  uint32_t count = g_ref_num.GetNext();
  // The 24 bit hash is composed of 14 bits of the count and 10 bits of the
  // Process ID. With the current trace event buffer cap, the 14-bit count did
  // not appear to wrap during a trace. Note that it is not a big deal if
  // collisions occur, as this is only used for debugging and trace analysis.
  return ((pid << 14) | (count & 0x3fff)) << 8;
}

}  // namespace

namespace IPC {

//------------------------------------------------------------------------------

Message::~Message() = default;

Message::Message() : base::Pickle(sizeof(Header)) {
  header()->routing = header()->type = 0;
  header()->flags = GetRefNumUpper24();
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  header()->num_fds = 0;
  header()->pad = 0;
#endif
  Init();
}

Message::Message(int32_t routing_id, uint32_t type, PriorityValue priority)
    : base::Pickle(sizeof(Header)) {
  header()->routing = routing_id;
  header()->type = type;
  DCHECK((priority & 0xffffff00) == 0);
  header()->flags = priority | GetRefNumUpper24();
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  header()->num_fds = 0;
  header()->pad = 0;
#endif
  Init();
}

Message::Message(const char* data, size_t data_len)
    : base::Pickle(base::Pickle::kUnownedData,
                   base::as_bytes(base::span(data, data_len))) {
  Init();
}

Message::Message(const Message& other) : base::Pickle(other) {
  Init();
  attachment_set_ = other.attachment_set_;
}

void Message::Init() {
  dispatch_error_ = false;
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  received_time_ = 0;
  dont_log_ = false;
  log_data_ = nullptr;
#endif
}

Message& Message::operator=(const Message& other) {
  *static_cast<base::Pickle*>(this) = other;
  attachment_set_ = other.attachment_set_;
  return *this;
}

void Message::SetHeaderValues(int32_t routing, uint32_t type, uint32_t flags) {
  // This should only be called when the message is already empty.
  DCHECK(payload_size() == 0);

  header()->routing = routing;
  header()->type = type;
  header()->flags = flags;
}

void Message::EnsureMessageAttachmentSet() {
  if (!attachment_set_.get())
    attachment_set_ = new MessageAttachmentSet;
}

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
void Message::set_sent_time(int64_t time) {
  DCHECK((header()->flags & HAS_SENT_TIME_BIT) == 0);
  header()->flags |= HAS_SENT_TIME_BIT;
  WriteInt64(time);
}

int64_t Message::sent_time() const {
  if ((header()->flags & HAS_SENT_TIME_BIT) == 0)
    return 0;

  const char* data = end_of_payload();
  data -= sizeof(int64_t);
  return *(reinterpret_cast<const int64_t*>(data));
}

void Message::set_received_time(int64_t time) const {
  received_time_ = time;
}
#endif  // BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

Message::NextMessageInfo::NextMessageInfo()
    : message_size(0), message_found(false), pickle_end(nullptr),
      message_end(nullptr) {}
Message::NextMessageInfo::~NextMessageInfo() = default;

// static
void Message::FindNext(const char* range_start,
                       const char* range_end,
                       NextMessageInfo* info) {
  DCHECK(info);
  info->message_found = false;
  info->message_size = 0;

  size_t pickle_size = 0;
  if (!base::Pickle::PeekNext(sizeof(Header),
                              range_start, range_end, &pickle_size))
    return;

  bool have_entire_pickle =
      static_cast<size_t>(range_end - range_start) >= pickle_size;

  info->message_size = pickle_size;

  if (!have_entire_pickle)
    return;

  const char* pickle_end = range_start + pickle_size;

  info->message_end = pickle_end;

  info->pickle_end = pickle_end;
  info->message_found = true;
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
