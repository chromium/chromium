// Copyright 2019 The Chromium Authors
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
      id_(std::exchange(message_port.id_, base::UnguessableToken::Null())),
      sequence_number_(std::exchange(message_port.sequence_number_,
                                     kInvalidSequenceNumber)) {}

MessagePortDescriptor& MessagePortDescriptor::operator=(
    MessagePortDescriptor&& message_port) {
  Reset();

  handle_ = std::move(message_port.handle_);
  id_ = std::exchange(message_port.id_, base::UnguessableToken::Null());
  sequence_number_ =
      std::exchange(message_port.sequence_number_, kInvalidSequenceNumber);

  return *this;
}

MessagePortDescriptor::~MessagePortDescriptor() {
  Reset();
}

MojoHandle MessagePortDescriptor::GetMojoHandleForTesting() const {
  if (!handle_.get())
    return MOJO_HANDLE_INVALID;
  return handle_.get().value();
}

bool MessagePortDescriptor::IsValid() const {
  // |handle_| can be valid or invalid, depending on if we're entangled or
  // not. But everything else should be consistent.
  EnsureValidSerializationState();
  DCHECK_EQ(id_.is_empty(), sequence_number_ == kInvalidSequenceNumber);
  return !id_.is_empty() && sequence_number_ != kInvalidSequenceNumber;
}

bool MessagePortDescriptor::IsEntangled() const {
  EnsureNotSerialized();
  // This descriptor is entangled if it's valid, but its handle has been loaned
  // out.
  return IsValid() && !handle_.is_valid();
}

bool MessagePortDescriptor::IsDefault() const {
  EnsureValidSerializationState();
  if (IsValid())
    return false;

  // This is almost the converse of IsValid, except that we additionally expect
  // the |handle_| to be empty as well (which IsValid doesn't verify).
  DCHECK(!handle_.is_valid());
  return true;
}

void MessagePortDescriptor::Reset() {
#if DCHECK_IS_ON()
  EnsureValidSerializationState();
  serialization_state_ = {};
#endif

  if (IsValid()) {
    // Call NotifyDestroyed before clearing members, as the notification needs
    // to access them.
    NotifyDestroyed();

    // Ensure that MessagePipeDescriptor-wrapped handles are fully accounted for
    // over their entire lifetime.
    DCHECK(handle_.is_valid());

    handle_.reset();
    id_ = base::UnguessableToken::Null();
    sequence_number_ = kInvalidSequenceNumber;
  }
}

void MessagePortDescriptor::InitializeFromSerializedValues(
    mojo::ScopedMessagePipeHandle handle,
    const base::UnguessableToken& id,
    uint64_t sequence_number) {
#if DCHECK_IS_ON()
  EnsureValidSerializationState();
  serialization_state_ = {};

  // This is only called by deserialization code and thus should only be called
  // on a default initialized descriptor.
  DCHECK(IsDefault());
#endif

  handle_ = std::move(handle);
  id_ = id;
  sequence_number_ = sequence_number;

  // Init should only create a valid not-entangled descriptor, or a default
  // descriptor.
  DCHECK((IsValid() && !IsEntangled()) || IsDefault());
}

mojo::ScopedMessagePipeHandle
MessagePortDescriptor::TakeHandleForSerialization() {
#if DCHECK_IS_ON()
  DCHECK(handle_.is_valid());  // Ensures not entangled.
  DCHECK(!serialization_state_.took_handle_for_serialization_);
  serialization_state_.took_handle_for_serialization_ = true;
#endif
  return std::move(handle_);
}

base::UnguessableToken MessagePortDescriptor::TakeIdForSerialization() {
#if DCHECK_IS_ON()
  DCHECK(!id_.is_empty());
  DCHECK(serialization_state_.took_handle_for_serialization_ ||
         handle_.is_valid());  // Ensures not entangled.
  DCHECK(!serialization_state_.took_id_for_serialization_);
  serialization_state_.took_id_for_serialization_ = true;
#endif
  return std::exchange(id_, base::UnguessableToken::Null());
}

uint64_t MessagePortDescriptor::TakeSequenceNumberForSerialization() {
#if DCHECK_IS_ON()
  DCHECK_NE(kInvalidSequenceNumber, sequence_number_);
  DCHECK(serialization_state_.took_handle_for_serialization_ ||
         handle_.is_valid());  // Ensures not entangled.
  DCHECK(!serialization_state_.took_sequence_number_for_serialization_);
  serialization_state_.took_sequence_number_for_serialization_ = true;
#endif
  return std::exchange(sequence_number_, kInvalidSequenceNumber);
}

mojo::ScopedMessagePipeHandle MessagePortDescriptor::TakeHandleToEntangle(
    ExecutionContext* execution_context) {
  EnsureNotSerialized();
  DCHECK(handle_.is_valid());
  NotifyAttached(execution_context);
  return std::move(handle_);
}

mojo::ScopedMessagePipeHandle
MessagePortDescriptor::TakeHandleToEntangleWithEmbedder() {
  EnsureNotSerialized();
  DCHECK(handle_.is_valid());
  NotifyAttachedToEmbedder();
  return std::move(handle_);
}

void MessagePortDescriptor::GiveDisentangledHandle(
    mojo::ScopedMessagePipeHandle handle) {
  EnsureNotSerialized();
  // Ideally, we should only ever be given back the same handle that was taken
  // from us.
  // NOTE: It is possible that this can happen if the handle is bound to a
  // Connector, and the Connector subsequently encounters an error, force closes
  // the pipe, and the transparently binds another dangling pipe. This can be
  // caught by having the descriptor own the connector and observer connection
  // errors, but this can only occur once descriptors are being used everywhere.
  handle_ = std::move(handle);

  // If we've been given back a null handle, then the handle we vended out was
  // closed due to error (this can happen in Java code). For now, simply create
  // a dangling handle to replace it. This allows the IsEntangled() and
  // IsValid() logic to work as is.
  // TODO(chrisha): Clean this up once we make this own a connector, and endow
  // it with knowledge of the connector error state. There's no need for us to
  // hold on to a dangling pipe endpoint, and we can send a NotifyClosed()
  // earlier.
  if (!handle_.is_valid()) {
    mojo::MessagePipe pipe;
    handle_ = std::move(pipe.handle0);
  }

  NotifyDetached();
}

MessagePortDescriptor::MessagePortDescriptor(
    mojo::ScopedMessagePipeHandle handle)
    : handle_(std::move(handle)),
      id_(base::UnguessableToken::Create()),
      sequence_number_(kFirstValidSequenceNumber) {
}

void MessagePortDescriptor::NotifyAttached(
    ExecutionContext* execution_context) {
  EnsureNotSerialized();
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortAttached(
        id_, sequence_number_++, execution_context);
  }
}

void MessagePortDescriptor::NotifyAttachedToEmbedder() {
  EnsureNotSerialized();
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortAttachedToEmbedder(
        id_, sequence_number_++);
  }
}

void MessagePortDescriptor::NotifyDetached() {
  EnsureNotSerialized();
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortDetached(id_,
                                                          sequence_number_++);
  }
}

void MessagePortDescriptor::NotifyDestroyed() {
  EnsureNotSerialized();
  DCHECK(!id_.is_empty());
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortDestroyed(id_,
                                                           sequence_number_++);
  }
}

void MessagePortDescriptor::EnsureNotSerialized() const {
#if DCHECK_IS_ON()
  DCHECK(!serialization_state_.took_handle_for_serialization_ &&
         !serialization_state_.took_id_for_serialization_ &&
         !serialization_state_.took_sequence_number_for_serialization_);
#endif
}

void MessagePortDescriptor::EnsureValidSerializationState() const {
#if DCHECK_IS_ON()
  // Either everything was serialized, or nothing was.
  DCHECK((serialization_state_.took_handle_for_serialization_ ==
          serialization_state_.took_id_for_serialization_) &&
         (serialization_state_.took_handle_for_serialization_ ==
          serialization_state_.took_sequence_number_for_serialization_));
#endif
}

MessagePortDescriptorPair::MessagePortDescriptorPair() {
  mojo::MessagePipe pipe;
  port0_ = MessagePortDescriptor(std::move(pipe.handle0));
  port1_ = MessagePortDescriptor(std::move(pipe.handle1));

  // Notify the instrumentation that these ports are newly created and peers of
  // each other.
  if (g_instrumentation_delegate) {
    g_instrumentation_delegate->NotifyMessagePortPairCreated(port0_.id(),
                                                             port1_.id());
  }
}

MessagePortDescriptorPair::~MessagePortDescriptorPair() = default;

}  // namespace blink
