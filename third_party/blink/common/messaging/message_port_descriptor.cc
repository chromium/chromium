// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

#include <utility>

namespace blink {

namespace {

MessagePortDescriptor::InstrumentationDelegate* g_instrumentation_delegate =
    nullptr;

}  // namespace

// static
const size_t MessagePortDescriptor::kInvalidSequenceNumber;

// static
const size_t MessagePortDescriptor::kFirstValidSequenceNumber;

// static
void MessagePortDescriptor::SetInstrumentationDelegate(
    InstrumentationDelegate* delegate) {
  // There should only ever be one delegate, and this only should toggle from
  // being set to not being set and vice-versa. The toggling only ever occurs
  // during tests; in production a single instrumentation delegate is installed
  // early during Blink startup and left in place forever afterwards.
  DCHECK(!delegate ^ !g_instrumentation_delegate);
  g_instrumentation_delegate = delegate;
}

MessagePortDescriptor::MessagePortDescriptor() = default;

MessagePortDescriptor::MessagePortDescriptor(
    MessagePortDescriptor&& message_port)
    : handle_(std::move(message_port.handle_)),
#if DCHECK_IS_ON()
      raw_handle_(
          std::exchange(message_port.raw_handle_, mojo::MessagePipeHandle())),
#endif
      id_(std::exchange(message_port.id_, base::UnguessableToken::Null())),
      sequence_number_(std::exchange(message_port.sequence_number_,
                                     kInvalidSequenceNumber)) {
#if DCHECK_IS_ON()
  DCHECK_EQ(raw_handle_.value(), handle_.get().value());
#endif
}

MessagePortDescriptor::~MessagePortDescriptor() {
  Reset();
}

MessagePortDescriptor& MessagePortDescriptor::operator=(
    MessagePortDescriptor&& message_port) {
  Reset();

  handle_ = std::move(message_port.handle_);
#if DCHECK_IS_ON()
  raw_handle_ =
      std::exchange(message_port.raw_handle_, mojo::MessagePipeHandle());
  DCHECK_EQ(raw_handle_.value(), handle_.get().value());
#endif
  id_ = std::exchange(message_port.id_, base::UnguessableToken::Null());
  sequence_number_ =
      std::exchange(message_port.sequence_number_, kInvalidSequenceNumber);

  return *this;
}

bool MessagePortDescriptor::IsValid() const {
  // |handle_| can be valid or invalid, depending on if we're entangled or
  // not. But everything else should be consistent.
#if DCHECK_IS_ON()
  DCHECK_EQ(id_.is_empty(), !raw_handle_.is_valid());
  DCHECK_EQ(sequence_number_ == kInvalidSequenceNumber,
            !raw_handle_.is_valid());
  return raw_handle_.is_valid();
#else
  DCHECK_EQ(id_.is_empty(), sequence_number_ == kInvalidSequenceNumber);
  return !id_.is_empty() && sequence_number_ != kInvalidSequenceNumber;
#endif
}

bool MessagePortDescriptor::IsEntangled() const {
  // This descriptor is entangled if it's valid, but its handle has been loaned
  // out.
  return IsValid() && !handle_.is_valid();
}

bool MessagePortDescriptor::IsDefault() const {
  if (IsValid())
    return false;

  // This is almost the converse of IsValid, except that we additionally expect
  // the |handle_| to be empty as well (which IsValid doesn't verify).
  DCHECK(!handle_.is_valid());
  return true;
}

void MessagePortDescriptor::Reset() {
  if (IsValid()) {
    // Call NotifyDestroyed before clearing members, as the notification needs
    // to access them.
    NotifyDestroyed();

    // Ensure that MessagePipeDescriptor-wrapped handles are fully accounted for
    // over their entire lifetime.
    DCHECK(handle_.is_valid());
#if DCHECK_IS_ON()
    DCHECK(raw_handle_.is_valid());
    DCHECK_EQ(raw_handle_.value(), handle_.get().value());
    raw_handle_ = mojo::MessagePipeHandle();
#endif

    handle_.reset();
    id_ = base::UnguessableToken::Null();
    sequence_number_ = kInvalidSequenceNumber;
  }
}

void MessagePortDescriptor::Init(mojo::ScopedMessagePipeHandle handle,
                                 base::UnguessableToken id,
                                 uint64_t sequence_number) {
  // Init is only called by deserialization code and thus should only be called
  // on a default initialized descriptor.
  DCHECK(IsDefault());

  handle_ = std::move(handle);
#if DCHECK_IS_ON()
  raw_handle_ = handle_.get();
#endif
  id_ = id;
  sequence_number_ = sequence_number;

  // Init should only create a valid not-entangled descriptor, or a default
  // descriptor.
  DCHECK((IsValid() && !IsEntangled()) || IsDefault());
}

mojo::ScopedMessagePipeHandle MessagePortDescriptor::TakeHandle() {
  DCHECK(handle_.is_valid());
#if DCHECK_IS_ON()
  DCHECK(raw_handle_.is_valid());
  DCHECK_EQ(raw_handle_.value(), handle_.get().value());
  raw_handle_ = mojo::MessagePipeHandle();
#endif
  return std::move(handle_);
}

base::UnguessableToken MessagePortDescriptor::TakeId() {
  DCHECK(!id_.is_empty());
  return std::exchange(id_, base::UnguessableToken::Null());
}

uint64_t MessagePortDescriptor::TakeSequenceNumber() {
  DCHECK_NE(kInvalidSequenceNumber, sequence_number_);
  return std::exchange(sequence_number_, kInvalidSequenceNumber);
}

mojo::ScopedMessagePipeHandle MessagePortDescriptor::TakeHandleToEntangle(
    const base::UnguessableToken& execution_context_id) {
  DCHECK(handle_.is_valid());
  NotifyAttached(execution_context_id);
  // Do not use TakeHandle, because it also resets |raw_handle_|. In DCHECK
  // builds we use |raw_handle_| to ensure that the same handle is given back to
  // us via "GiveDisentangledHandle".
  return std::move(handle_);
}

void MessagePortDescriptor::GiveDisentangledHandle(
    mojo::ScopedMessagePipeHandle handle) {
  // We should only ever be given back the same handle that was taken from us.
  DCHECK(!handle_.is_valid());
#if DCHECK_IS_ON()
  DCHECK_EQ(raw_handle_.value(), handle.get().value());
#endif
  handle_ = std::move(handle);
  NotifyDetached();
}

MessagePortDescriptor::MessagePortDescriptor(
    mojo::ScopedMessagePipeHandle handle)
    : handle_(std::move(handle)),
#if DCHECK_IS_ON()
      raw_handle_(handle_.get()),
#endif
      id_(base::UnguessableToken::Create()),
      sequence_number_(kFirstValidSequenceNumber) {
}

void MessagePortDescriptor::NotifyAttached(
    const base::UnguessableToken& execution_context_id) {
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortAttached(
        id_, sequence_number_++, execution_context_id);
  }
}

void MessagePortDescriptor::NotifyDetached() {
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortDetached(id_,
                                                          sequence_number_++);
  }
}

void MessagePortDescriptor::NotifyDestroyed() {
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortDestroyed(id_,
                                                           sequence_number_++);
  }
}

MessagePortDescriptorPair::MessagePortDescriptorPair() {
  mojo::MessagePipe pipe;
  port0_ = MessagePortDescriptor(std::move(pipe.handle0));
  port1_ = MessagePortDescriptor(std::move(pipe.handle1));

  // Notify the instrumentation that these ports are newly created and peers of
  // each other.
  if (g_instrumentation_delegate)
    g_instrumentation_delegate->NotifyMessagePortPairCreated(*this);
}

MessagePortDescriptorPair::~MessagePortDescriptorPair() = default;

}  // namespace blink
