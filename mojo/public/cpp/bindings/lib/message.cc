// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/bindings/message.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <atomic>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/unserialized_message_context.h"

namespace mojo {

BASE_FEATURE(kMojoMessageAlwaysUseLatestVersion,
             "MojoMessageAlwaysUseLatestVersion",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

BASE_FEATURE(kMojoBindingsInlineSLS,
             "MojoBindingsInlineSLS",
             base::FEATURE_ENABLED_BY_DEFAULT);

base::GenericSequenceLocalStorageSlot<internal::MessageDispatchContext*>&
GetSLSMessageDispatchContext() {
  static base::GenericSequenceLocalStorageSlot<
      internal::MessageDispatchContext*>
      sls;
  return sls;
}

base::SmallSequenceLocalStorageSlot<internal::MessageDispatchContext*>&
GetSmallSLSMessageDispatchContext() {
  static base::SmallSequenceLocalStorageSlot<internal::MessageDispatchContext*>
      sls;
  return sls;
}

thread_local base::MetricsSubSampler g_sub_sampler;

void SetMessageDispatchContext(internal::MessageDispatchContext* context) {
  if (base::FeatureList::IsEnabled(kMojoBindingsInlineSLS)) {
    GetSmallSLSMessageDispatchContext().emplace(context);
  } else {
    GetSLSMessageDispatchContext().emplace(context);
  }
}

internal::MessageDispatchContext* GetMessageDispatchContext() {
  if (base::FeatureList::IsEnabled(kMojoBindingsInlineSLS)) {
    return GetSmallSLSMessageDispatchContext().GetOrCreateValue();
  } else {
    return GetSLSMessageDispatchContext().GetOrCreateValue();
  }
}

void DoNotifyBadMessage(Message message, std::string_view error) {
  message.NotifyBadMessage(error);
}

template <typename HeaderType>
void AllocateHeaderFromBuffer(internal::Buffer* buffer, HeaderType** header) {
  *header = buffer->AllocateAndGet<HeaderType>();
  (*header)->num_bytes = sizeof(HeaderType);
}

uint64_t GetTraceId(uint32_t name, uint32_t trace_nonce) {
  // |name| is used as additional sources of entropy to reduce the
  // changes of collision.
  return (static_cast<uint64_t>(name) << 32) |
         static_cast<uint64_t>(trace_nonce);
}

void WriteMessageHeaderV1(uint32_t name,
                          uint32_t flags,
                          uint32_t trace_nonce,
                          internal::Buffer* payload_buffer) {
  internal::MessageHeaderV1* header;
  AllocateHeaderFromBuffer(payload_buffer, &header);
  header->version = 1;
  header->name = name;
  header->flags = flags;
  header->trace_nonce = trace_nonce;
}

void WriteMessageHeader(uint32_t name,
                        uint32_t flags,
                        uint32_t trace_nonce,
                        size_t payload_interface_id_count,
                        internal::Buffer* payload_buffer,
                        int64_t creation_timeticks_us) {
  if (creation_timeticks_us > 0 ||
      base::FeatureList::IsEnabled(kMojoMessageAlwaysUseLatestVersion)) {
    // Version 3
    internal::MessageHeaderV3* header;
    AllocateHeaderFromBuffer(payload_buffer, &header);
    header->version = 3;
    header->name = name;
    header->flags = flags;
    header->trace_nonce = trace_nonce;
    // The payload immediately follows the header.
    header->payload.Set(header + 1);
    header->creation_timeticks_us = creation_timeticks_us;
  } else if (payload_interface_id_count > 0) {
    // Version 2
    internal::MessageHeaderV2* header;
    AllocateHeaderFromBuffer(payload_buffer, &header);
    header->version = 2;
    header->name = name;
    header->flags = flags;
    header->trace_nonce = trace_nonce;
    // The payload immediately follows the header.
    header->payload.Set(header + 1);
  } else if (flags &
             (Message::kFlagExpectsResponse | Message::kFlagIsResponse)) {
    // Version 1
    WriteMessageHeaderV1(name, flags, trace_nonce, payload_buffer);
  } else {
    internal::MessageHeader* header;
    AllocateHeaderFromBuffer(payload_buffer, &header);
    header->version = 0;
    header->name = name;
    header->flags = flags;
    header->trace_nonce = trace_nonce;
  }
}

void CreateSerializedMessageObject(uint32_t name,
                                   uint32_t flags,
                                   uint32_t trace_nonce,
                                   size_t payload_size,
                                   size_t payload_interface_id_count,
                                   MojoCreateMessageFlags create_message_flags,
                                   std::vector<ScopedHandle>* handles,
                                   ScopedMessageHandle* out_handle,
                                   internal::Buffer* out_buffer,
                                   size_t estimated_payload_size,
                                   int64_t creation_timeticks_us) {
  ScopedMessageHandle handle;
  MojoResult rv = CreateMessage(&handle, create_message_flags);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(handle.is_valid());

  void* buffer;
  uint32_t buffer_size;
  const size_t total_size = internal::ComputeSerializedMessageSize(
      flags, payload_size, payload_interface_id_count, creation_timeticks_us);
  const size_t total_allocation_size = internal::EstimateSerializedMessageSize(
      name, payload_size, total_size, estimated_payload_size);

  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(total_size));
  DCHECK(!handles ||
         base::IsValueInRangeForNumericType<uint32_t>(handles->size()));

  if (estimated_payload_size > payload_size) {
    rv = MojoReserveMessageCapacity(
        handle->value(), static_cast<uint32_t>(total_allocation_size), nullptr);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
  }

  rv = MojoAppendMessageData(
      handle->value(), static_cast<uint32_t>(total_size),
      handles ? reinterpret_cast<MojoHandle*>(handles->data()) : nullptr,
      handles ? static_cast<uint32_t>(handles->size()) : 0, nullptr, &buffer,
      &buffer_size);
  // TODO(crbug.com/40785088): Relax this assertion or fail more gracefully.
  CHECK_EQ(MOJO_RESULT_OK, rv);
  if (handles) {
    // Handle ownership has been taken by MojoAppendMessageData.
    for (size_t i = 0; i < handles->size(); ++i)
      std::ignore = handles->at(i).release();
  }

  internal::Buffer payload_buffer(handle.get(), total_size, buffer,
                                  buffer_size);

  // Make sure we zero the memory first!
  memset(payload_buffer.data(), 0, buffer_size);
  WriteMessageHeader(name, flags, trace_nonce, payload_interface_id_count,
                     &payload_buffer, creation_timeticks_us);

  *out_handle = std::move(handle);
  *out_buffer = std::move(payload_buffer);
}

void SerializeUnserializedContext(MojoMessageHandle message,
                                  uintptr_t context_value) {
  auto* context =
      reinterpret_cast<internal::UnserializedMessageContext*>(context_value);

  // Note that `message` is merely borrowed here. Ownership is released below.
  Message new_message(ScopedMessageHandle(MessageHandle(message)),
                      *context->header());
  context->Serialize(new_message);

  // TODO(crbug.com/41338252): Support lazy serialization of associated endpoint
  // handles.
  new_message.SerializeHandles(/*group_controller=*/nullptr);

  // Finalize the serialized message state and release ownership back to the
  // caller.
  std::ignore = new_message.TakeMojoMessage().release();
}

void DestroyUnserializedContext(uintptr_t context) {
  delete reinterpret_cast<internal::UnserializedMessageContext*>(context);
}

Message CreateUnserializedMessage(
    std::unique_ptr<internal::UnserializedMessageContext> context,
    MojoCreateMessageFlags create_message_flags) {
  context->header()->trace_nonce =
      static_cast<uint32_t>(base::trace_event::GetNextGlobalTraceId());
  ScopedMessageHandle handle;
  MojoResult rv = CreateMessage(&handle, create_message_flags);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(handle.is_valid());

  rv = MojoSetMessageContext(
      handle->value(), reinterpret_cast<uintptr_t>(context.release()),
      &SerializeUnserializedContext, &DestroyUnserializedContext, nullptr);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  return Message::CreateFromMessageHandle(&handle);
}

}  // namespace

Message::Message() = default;

Message::Message(Message&& other)
    : handle_(std::move(other.handle_)),
      payload_buffer_(std::move(other.payload_buffer_)),
      handles_(std::move(other.handles_)),
      associated_endpoint_handles_(
          std::move(other.associated_endpoint_handles_)),
      receiver_connection_group_(other.receiver_connection_group_),
      transferable_(other.transferable_),
      serialized_(other.serialized_),
      heap_profiler_tag_(other.heap_profiler_tag_) {
  other.transferable_ = false;
  other.serialized_ = false;
#if defined(ENABLE_IPC_FUZZER)
  interface_name_ = other.interface_name_;
  method_name_ = other.method_name_;
#endif
}

Message::Message(std::unique_ptr<internal::UnserializedMessageContext> context,
                 MojoCreateMessageFlags create_message_flags)
    : Message(CreateUnserializedMessage(std::move(context),
                                        create_message_flags)) {}

Message::Message(uint32_t name,
                 uint32_t flags,
                 size_t payload_size,
                 size_t payload_interface_id_count,
                 MojoCreateMessageFlags create_message_flags,
                 std::vector<ScopedHandle>* handles,
                 size_t estimated_payload_size) {
  int64_t creation_timeticks_us = 0;
  // Sub-sample end to end time histogram on the sender side to reduce overhead.
  if (base::TimeTicks::IsConsistentAcrossProcesses() &&
      g_sub_sampler.ShouldSample(0.001)) {
    creation_timeticks_us =
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  }
  uint32_t trace_nonce =
      static_cast<uint32_t>(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("mojom"), "mojo::Message::Message",
              perfetto::Flow::Global(::mojo::GetTraceId(name, trace_nonce)),
              "name", name, "flags", flags, "trace_nonce", trace_nonce);

  CreateSerializedMessageObject(
      name, flags, trace_nonce, payload_size, payload_interface_id_count,
      create_message_flags, handles, &handle_, &payload_buffer_,
      estimated_payload_size, creation_timeticks_us);
  transferable_ = true;
  serialized_ = true;
}

Message::Message(uint32_t name,
                 uint32_t flags,
                 size_t payload_size,
                 size_t payload_interface_id_count,
                 std::vector<ScopedHandle>* handles,
                 size_t estimated_payload_size)
    : Message(name,
              flags,
              payload_size,
              payload_interface_id_count,
              MOJO_CREATE_MESSAGE_FLAG_NONE,
              handles,
              estimated_payload_size) {}

Message::Message(uint32_t name,
                 uint32_t flags,
                 MojoCreateMessageFlags create_message_flags,
                 size_t estimated_payload_size)
    : Message(name,
              flags,
              0,
              0,
              create_message_flags,
              nullptr,
              estimated_payload_size) {}

Message::Message(uint32_t name, uint32_t flags, size_t estimated_payload_size)
    : Message(name,
              flags,
              MOJO_CREATE_MESSAGE_FLAG_NONE,
              estimated_payload_size) {}

Message::Message(ScopedMessageHandle handle,
                 const internal::MessageHeaderV1& header)
    : handle_(std::move(handle)), transferable_(true) {
  const uint32_t trace_nonce =
      static_cast<uint32_t>(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT(
      "mojom", "mojo::Message::Message_FromHandle",
      perfetto::Flow::Global(::mojo::GetTraceId(header.name, trace_nonce)),
      "this", this);

  void* buffer;
  uint32_t buffer_size;
  MojoResult attach_result = MojoAppendMessageData(
      handle_.get().value(), 0, nullptr, 0, nullptr, &buffer, &buffer_size);
  if (attach_result != MOJO_RESULT_OK)
    return;

  payload_buffer_ = internal::Buffer(handle_.get(), 0, buffer, buffer_size);
  WriteMessageHeaderV1(header.name, header.flags, trace_nonce,
                       &payload_buffer_);

  // We need to copy additional header data which may have been set after
  // original message construction, as this codepath may be reached at some
  // arbitrary time between message send and message dispatch.
  static_cast<internal::MessageHeader*>(buffer)->interface_id =
      header.interface_id;
  if (header.flags &
      (Message::kFlagExpectsResponse | Message::kFlagIsResponse)) {
    DCHECK_GE(header.version, 1u);
    static_cast<internal::MessageHeaderV1*>(buffer)->request_id =
        header.request_id;
  }
}

Message::Message(base::span<const uint8_t> payload,
                 base::span<ScopedHandle> handles) {
  MojoResult rv = CreateMessage(&handle_, MOJO_CREATE_MESSAGE_FLAG_NONE);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(handle_.is_valid());

  void* buffer;
  uint32_t buffer_size;
  CHECK(base::IsValueInRangeForNumericType<uint32_t>(payload.size()));
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(handles.size()));
  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  rv = MojoAppendMessageData(
      handle_->value(), static_cast<uint32_t>(payload.size()),
      reinterpret_cast<MojoHandle*>(handles.data()),
      static_cast<uint32_t>(handles.size()), &options, &buffer, &buffer_size);

  // TODO(crbug.com/40785088): Relax this assertion or fail more gracefully.
  CHECK_EQ(MOJO_RESULT_OK, rv);

  // Handle ownership has been taken by MojoAppendMessageData.
  for (auto& handle : handles)
    std::ignore = handle.release();

  payload_buffer_ = internal::Buffer(buffer, payload.size(), payload.size());
  base::ranges::copy(payload, static_cast<uint8_t*>(payload_buffer_.data()));
  transferable_ = true;
  serialized_ = true;
}

// static
Message Message::CreateFromMessageHandle(ScopedMessageHandle* message_handle) {
  DCHECK(message_handle);
  const MessageHandle& handle = message_handle->get();
  DCHECK(handle.is_valid());

  uintptr_t context_value = 0;
  MojoResult get_context_result =
      MojoGetMessageContext(handle.value(), nullptr, &context_value);
  if (get_context_result == MOJO_RESULT_NOT_FOUND) {
    // It's a serialized message. Extract handles if possible.
    uint32_t num_bytes;
    void* buffer;
    uint32_t num_handles = 0;
    std::vector<ScopedHandle> handles;
    MojoResult rv = MojoGetMessageData(handle.value(), nullptr, &buffer,
                                       &num_bytes, nullptr, &num_handles);
    if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED) {
      handles.resize(num_handles);
      rv = MojoGetMessageData(handle.value(), nullptr, &buffer, &num_bytes,
                              reinterpret_cast<MojoHandle*>(handles.data()),
                              &num_handles);
    }

    if (rv != MOJO_RESULT_OK) {
      // Failed to deserialize handles. Return a null message and leave the
      // |*message_handle| intact.
      return Message();
    }

    return Message(std::move(*message_handle), std::move(handles),
                   internal::Buffer(buffer, num_bytes, num_bytes),
                   true /* serialized */);
  }

  DCHECK_EQ(MOJO_RESULT_OK, get_context_result);
  auto* context =
      reinterpret_cast<internal::UnserializedMessageContext*>(context_value);
  // Dummy data address so common header accessors still behave properly. The
  // choice is V1 reflects unserialized message capabilities: we may or may
  // not need to support request IDs (which require at least V1), but we never
  // (for now, anyway) need to support associated interface handles (V2).
  internal::Buffer payload_buffer(context->header(),
                                  sizeof(internal::MessageHeaderV1),
                                  sizeof(internal::MessageHeaderV1));
  return Message(std::move(*message_handle), {}, std::move(payload_buffer),
                 false /* serialized */);
}

Message::~Message() = default;

Message& Message::operator=(Message&& other) {
  handle_ = std::move(other.handle_);
  payload_buffer_ = std::move(other.payload_buffer_);
  handles_ = std::move(other.handles_);
  associated_endpoint_handles_ = std::move(other.associated_endpoint_handles_);
  receiver_connection_group_ = other.receiver_connection_group_;
  transferable_ = other.transferable_;
  other.transferable_ = false;
  serialized_ = other.serialized_;
  other.serialized_ = false;
  heap_profiler_tag_ = other.heap_profiler_tag_;
#if defined(ENABLE_IPC_FUZZER)
  interface_name_ = other.interface_name_;
  method_name_ = other.method_name_;
#endif
  return *this;
}

void Message::Reset() {
  handle_.reset();
  payload_buffer_.Reset();
  handles_.clear();
  associated_endpoint_handles_.clear();
  receiver_connection_group_ = nullptr;
  transferable_ = false;
  serialized_ = false;
  heap_profiler_tag_ = nullptr;
}

const uint8_t* Message::payload() const {
  if (version() < 2)
    return data() + header()->num_bytes;

  DCHECK(!header_v2()->payload.is_null());
  return static_cast<const uint8_t*>(header_v2()->payload.Get());
}

uint32_t Message::payload_num_bytes() const {
  DCHECK_GE(data_num_bytes(), header()->num_bytes);
  size_t num_bytes;
  if (version() < 2) {
    num_bytes = data_num_bytes() - header()->num_bytes;
  } else {
    auto payload_begin =
        reinterpret_cast<uintptr_t>(header_v2()->payload.Get());
    auto payload_end =
        reinterpret_cast<uintptr_t>(header_v2()->payload_interface_ids.Get());
    if (!payload_end)
      payload_end = reinterpret_cast<uintptr_t>(data() + data_num_bytes());
    DCHECK_GE(payload_end, payload_begin);
    num_bytes = payload_end - payload_begin;
  }
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(num_bytes));
  return static_cast<uint32_t>(num_bytes);
}

uint32_t Message::payload_num_interface_ids() const {
  auto* array_pointer =
      version() < 2 ? nullptr : header_v2()->payload_interface_ids.Get();
  return array_pointer ? static_cast<uint32_t>(array_pointer->size()) : 0;
}

const uint32_t* Message::payload_interface_ids() const {
  auto* array_pointer =
      version() < 2 ? nullptr : header_v2()->payload_interface_ids.Get();
  return array_pointer ? array_pointer->storage() : nullptr;
}

ScopedMessageHandle Message::TakeMojoMessage() {
  // If there are associated endpoints transferred, SerializeHandles() must be
  // called before this method.
  DCHECK(associated_endpoint_handles()->empty());
  DCHECK(transferable_);
  payload_buffer_.Seal();
  auto handle = std::move(handle_);
  Reset();
  return handle;
}

void Message::NotifyBadMessage(std::string_view error) {
  DCHECK(handle_.is_valid());
  mojo::NotifyBadMessage(handle_.get(), error);
}

void Message::SerializeHandles(AssociatedGroupController* group_controller) {
  if (mutable_handles()->empty() &&
      mutable_associated_endpoint_handles()->empty()) {
    // No handles attached, so no extra serialization work.
    return;
  }

  if (mutable_associated_endpoint_handles()->empty()) {
    // Attaching only non-associated handles is easier since we don't have to
    // modify the message header. Faster path for that.
    bool attached = payload_buffer_.AttachHandles(mutable_handles());

    // TODO(crbug.com/40785088): Relax this assertion or fail more gracefully.
    CHECK(attached);

    return;
  }

  // Allocate a new message with enough space to hold all attached handles. Copy
  // this message's contents into the new one and use it to replace ourself.
  //
  // TODO(rockot): We could avoid the extra full message allocation by instead
  // growing the buffer and carefully moving its contents around. This errs on
  // the side of less complexity with probably only marginal performance cost.
  uint32_t payload_size = payload_num_bytes();
  const size_t num_endpoint_handles = associated_endpoint_handles()->size();
  mojo::Message new_message(name(), header()->flags, payload_size,
                            num_endpoint_handles, mutable_handles());
  new_message.header()->interface_id = header()->interface_id;
  new_message.header()->trace_nonce = header()->trace_nonce;
  if (header()->version >= 1) {
    new_message.header_v1()->request_id = header_v1()->request_id;
  }
  new_message.set_receiver_connection_group(receiver_connection_group());
  *new_message.mutable_associated_endpoint_handles() =
      std::move(*mutable_associated_endpoint_handles());
  memcpy(new_message.payload_buffer()->AllocateAndGet(payload_size), payload(),
         payload_size);
  *this = std::move(new_message);

  DCHECK(group_controller);
  DCHECK_GE(version(), 2u);
  DCHECK(header_v2()->payload_interface_ids.is_null());
  DCHECK(payload_buffer_.is_valid());
  DCHECK(handle_.is_valid());

  internal::MessageFragment<internal::Array_Data<uint32_t>> handles_fragment(
      *this);
  handles_fragment.AllocateArrayData(num_endpoint_handles);
  header_v2()->payload_interface_ids.Set(handles_fragment.data());

  for (size_t i = 0; i < num_endpoint_handles; ++i) {
    ScopedInterfaceEndpointHandle& handle =
        (*mutable_associated_endpoint_handles())[i];
    DCHECK(handle.pending_association());
    handles_fragment->storage()[i] =
        group_controller->AssociateInterface(std::move(handle));
  }
  mutable_associated_endpoint_handles()->clear();
}

bool Message::DeserializeAssociatedEndpointHandles(
    AssociatedGroupController* group_controller) {
  if (!serialized_)
    return true;

  auto& endpoint_handles = *mutable_associated_endpoint_handles();
  endpoint_handles.clear();

  uint32_t num_ids = payload_num_interface_ids();
  if (num_ids == 0)
    return true;

  endpoint_handles.reserve(num_ids);
  uint32_t* ids = header_v2()->payload_interface_ids.Get()->storage();
  bool result = true;
  for (uint32_t i = 0; i < num_ids; ++i) {
    auto handle = group_controller->CreateLocalEndpointHandle(ids[i]);
    if (IsValidInterfaceId(ids[i]) && !handle.is_valid()) {
      // |ids[i]| itself is valid but handle creation failed. In that case, mark
      // deserialization as failed but continue to deserialize the rest of
      // handles.
      result = false;
    }

    endpoint_handles.push_back(std::move(handle));
    ids[i] = kInvalidInterfaceId;
  }
  return result;
}

void Message::NotifyPeerClosureForSerializedHandles(
    AssociatedGroupController* group_controller) {
  const uint32_t num_ids = payload_num_interface_ids();
  if (num_ids == 0) {
    return;
  }

  const uint32_t* ids = header_v2()->payload_interface_ids.Get()->storage();
  for (uint32_t i = 0; i < num_ids; ++i) {
    group_controller->NotifyLocalEndpointOfPeerClosure(ids[i]);
  }
}

void Message::SerializeIfNecessary() {
  MojoResult rv = MojoSerializeMessage(handle_->value(), nullptr);
  if (rv == MOJO_RESULT_FAILED_PRECONDITION)
    return;

  // Reconstruct this Message instance from the serialized message's handle.
  ScopedMessageHandle handle = std::move(handle_);
  *this = CreateFromMessageHandle(&handle);
}

std::unique_ptr<internal::UnserializedMessageContext>
Message::TakeUnserializedContext(uintptr_t tag) {
  DCHECK(handle_.is_valid());
  uintptr_t context_value = 0;
  MojoResult rv =
      MojoGetMessageContext(handle_->value(), nullptr, &context_value);
  if (rv == MOJO_RESULT_NOT_FOUND)
    return nullptr;
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  auto* context =
      reinterpret_cast<internal::UnserializedMessageContext*>(context_value);
  if (context->tag() != tag)
    return nullptr;

  // Detach the context from the message.
  rv = MojoSetMessageContext(handle_->value(), 0, nullptr, nullptr, nullptr);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  return base::WrapUnique(context);
}

Message::Message(ScopedMessageHandle message_handle,
                 std::vector<ScopedHandle> attached_handles,
                 internal::Buffer payload_buffer,
                 bool serialized)
    : handle_(std::move(message_handle)),
      payload_buffer_(std::move(payload_buffer)),
      transferable_(!serialized || attached_handles.empty()),
      serialized_(serialized) {
  *mutable_handles() = std::move(attached_handles);
}

uint64_t Message::GetTraceId() const {
  return ::mojo::GetTraceId(header()->name, header()->trace_nonce);
}

void Message::WriteIntoTrace(perfetto::TracedValue ctx) const {
  perfetto::TracedDictionary dict = std::move(ctx).WriteDictionary();

  if (header()) {
    dict.Add("name", header()->name);
    dict.Add("flags", header()->flags);
    dict.Add("trace_nonce", header()->trace_nonce);
  }
}

int64_t Message::creation_timeticks_us() const {
  if (version() < 3) {
    return 0;
  }
  return header_v3()->creation_timeticks_us;
}

bool MessageReceiver::PrefersSerializedMessages() {
  return false;
}

PassThroughFilter::PassThroughFilter() {}

PassThroughFilter::~PassThroughFilter() {}

bool PassThroughFilter::Accept(Message* message) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("mojom"), "PassThroughFilter::Accept");
  return true;
}

void ReportBadMessage(std::string_view error) {
  internal::MessageDispatchContext* context =
      internal::MessageDispatchContext::current();
  DCHECK(context);
  context->GetBadMessageCallback().Run(error);
}

ReportBadMessageCallback GetBadMessageCallback() {
  internal::MessageDispatchContext* context =
      internal::MessageDispatchContext::current();
  DCHECK(context);
  return context->GetBadMessageCallback();
}

bool IsInMessageDispatch() {
  return internal::MessageDispatchContext::current();
}

namespace internal {

MessageHeaderV2::MessageHeaderV2() = default;

MessageDispatchContext::MessageDispatchContext(Message* message)
    : outer_context_(current()), message_(message) {
  SetMessageDispatchContext(this);
}

MessageDispatchContext::~MessageDispatchContext() {
  DCHECK_EQ(current(), this);
  SetMessageDispatchContext(outer_context_);
}

// static
MessageDispatchContext* MessageDispatchContext::current() {
  return GetMessageDispatchContext();
}

ReportBadMessageCallback MessageDispatchContext::GetBadMessageCallback() {
  DCHECK(!message_->IsNull());
  return base::BindOnce(&DoNotifyBadMessage, std::move(*message_));
}

}  // namespace internal

}  // namespace mojo
