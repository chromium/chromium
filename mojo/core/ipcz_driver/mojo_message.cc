// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/mojo_message.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// Growth factor for reallocations.
constexpr int kGrowthFactor = 2;

// Data pipe attachments come in two parts within a message's handle list: the
// DataPipe object wherever it was placed by the sender, and its control portal
// as a separate attachment at the end of the handle list. For a message with
// two data pipes endpoints (X and Y) and two message pipe endpoints(A and B),
// sent in the order AXBY, a well-formed message will have 6 total handles
// attached:
//
// Message Pipe A   Message Pipe B   DataPipe X's portal
//      |               |              |
//     0:A     1:X     2:B     3:Y    4:x    5:y
//              |               |             |
//          DataPipe X       DataPipe Y      DataPipe Y's portal
//
// This function validates that each DataPipe in `handles` has an associated
// portal, and it fixes up `handles` by stripping those portals off the end of
// the list and passing ownership to their corresponding DataPipe object.
//
// Returns true if and only if the handle list is well-formed in this regard.
//
// TODO(crbug.com/40877163): Since boxes now support application objects,
// DataPipe can be migrated out of the driver and we can avoid this whole
// serialization hack.
bool FixUpDataPipeHandles(std::vector<IpczHandle>& handles) {
  absl::InlinedVector<DataPipe*, 2> data_pipes;
  for (IpczHandle handle : handles) {
    if (auto* data_pipe = DataPipe::FromBox(handle)) {
      data_pipes.push_back(data_pipe);
    }
  }

  if (handles.size() < data_pipes.size() * 2) {
    // Not enough handles.
    return false;
  }

  // The last N handles must be portals for the pipes in `data_pipes`, in order.
  // Remove them from the message's handles and give them to their data pipes.
  const size_t first_data_pipe_portal = handles.size() - data_pipes.size();
  for (size_t i = 0; i < data_pipes.size(); ++i) {
    const IpczHandle handle = handles[first_data_pipe_portal + i];
    if (!data_pipes[i]->AdoptPortal(ScopedIpczHandle(handle))) {
      // Not a portal, so not a valid MojoMessage parcel.
      return false;
    }
  }
  handles.resize(first_data_pipe_portal);
  return true;
}

}  // namespace

MojoMessage::MojoMessage() = default;

MojoMessage::MojoMessage(std::vector<uint8_t> data,
                         std::vector<IpczHandle> handles)
    : handles_(std::move(handles)) {
  data_storage_.reset(static_cast<uint8_t*>(operator new(data.size())));
  data_storage_size_ = data.size();
  base::ranges::copy(data, data_storage_.get());
}

MojoMessage::~MojoMessage() {
  for (IpczHandle handle : handles_) {
    if (handle != IPCZ_INVALID_HANDLE) {
      GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
    }
  }

  if (destructor_) {
    destructor_(context_);
  }
}

void MojoMessage::SetParcel(ScopedIpczHandle parcel) {
  DCHECK(!data_storage_);
  DCHECK(!parcel_.is_valid());

  parcel_ = std::move(parcel);

  const volatile void* data;
  size_t num_bytes;
  size_t num_handles = 0;
  IpczTransaction transaction;
  IpczResult result =
      GetIpczAPI().BeginGet(parcel_.get(), IPCZ_NO_FLAGS, nullptr, &data,
                            &num_bytes, nullptr, &num_handles, &transaction);
  if (result == IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    handles_.resize(num_handles);
    result = GetIpczAPI().BeginGet(parcel_.get(), IPCZ_NO_FLAGS, nullptr, &data,
                                   &num_bytes, handles_.data(), &num_handles,
                                   &transaction);
  }

  // We always pass a parcel object in, so Begin/EndGet() must always succeed.
  DCHECK_EQ(result, IPCZ_RESULT_OK);
  if (num_bytes > 0) {
    data_storage_.reset(static_cast<uint8_t*>(operator new(num_bytes)));

    // Copy into private memory, out of the potentially shared and volatile
    // `data` buffer. Note that it's fine to cast away volatility here since we
    // aren't concerned with consistency across reads: a well-behaved peer won't
    // modify the buffer concurrently, so the copied bytes will always be
    // correct in that case; and in any other case we don't care what's copied,
    // as long as all subsequent reads operate on the private copy and not on
    // `data`.
    memcpy(data_storage_.get(), const_cast<const void*>(data), num_bytes);
  } else {
    data_storage_.reset();
  }
  data_ = {data_storage_.get(), num_bytes};
  data_storage_size_ = num_bytes;

  result = GetIpczAPI().EndGet(parcel_.get(), transaction, IPCZ_NO_FLAGS,
                               nullptr, nullptr);
  DCHECK_EQ(result, IPCZ_RESULT_OK);
  if (!FixUpDataPipeHandles(handles_)) {
    // The handle list was malformed. Although this is a validation error, it
    // is not safe to trigger MojoNotifyBadMessage from within MojoReadMessage,
    // as this may result in unexpected application re-entrancy. Instead we wipe
    // out all handles, which will effectively trigger a validation failure
    // further up the stack when the application sees (e.g. via bindings
    // validation) that expected handles are missing.
    handles_.clear();
  }
}

MojoResult MojoMessage::ReserveCapacity(uint32_t payload_buffer_size,
                                        uint32_t* buffer_size) {
  DCHECK(!parcel_.is_valid());
  if (context_ || size_committed_ || !data_.empty()) {
    // TODO(andreaorru): support reserving additional capacity
    // in the middle of the serialization.
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  data_storage_size_ = std::max(payload_buffer_size, uint32_t{kMinBufferSize});
  DataPtr new_storage(static_cast<uint8_t*>(operator new(data_storage_size_)));
  data_storage_ = std::move(new_storage);
  data_ = base::make_span(data_storage_.get(), 0u);

  if (buffer_size) {
    *buffer_size = base::checked_cast<uint32_t>(data_storage_size_);
  }
  return MOJO_RESULT_OK;
}

MojoResult MojoMessage::AppendData(uint32_t additional_num_bytes,
                                   const MojoHandle* handles,
                                   uint32_t num_handles,
                                   void** buffer,
                                   uint32_t* buffer_size,
                                   bool commit_size) {
  DCHECK(!parcel_.is_valid());
  if (context_ || size_committed_) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  const size_t data_size = data_.size();
  const size_t new_data_size = data_size + additional_num_bytes;
  const size_t required_storage_size = std::max(new_data_size, kMinBufferSize);
  if (required_storage_size > data_storage_size_) {
    const size_t copy_size = std::min(new_data_size, data_storage_size_);
    data_storage_size_ =
        std::max(data_size * kGrowthFactor, required_storage_size);
    DataPtr new_storage(
        static_cast<uint8_t*>(operator new(data_storage_size_)));
    base::ranges::copy(base::make_span(data_storage_.get(), copy_size),
                       new_storage.get());
    data_storage_ = std::move(new_storage);
  }
  data_ = base::make_span(data_storage_.get(), new_data_size);

  handles_.reserve(handles_.size() + num_handles);
  for (MojoHandle handle : base::make_span(handles, num_handles)) {
    handles_.push_back(handle);
  }
  if (buffer) {
    *buffer = data_storage_.get();
  }
  if (buffer_size) {
    *buffer_size = base::checked_cast<uint32_t>(data_storage_size_);
  }
  size_committed_ = commit_size;
  return MOJO_RESULT_OK;
}

IpczResult MojoMessage::GetData(void** buffer,
                                uint32_t* num_bytes,
                                MojoHandle* handles,
                                uint32_t* num_handles,
                                bool consume_handles) {
  if (context_ || (!parcel_.is_valid() && !size_committed_)) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  if (consume_handles && handles_consumed_) {
    return MOJO_RESULT_NOT_FOUND;
  }

  if (buffer) {
    *buffer = data_.data();
  }
  if (num_bytes) {
    *num_bytes = base::checked_cast<uint32_t>(data_.size());
  }

  if (!consume_handles || handles_.empty()) {
    return MOJO_RESULT_OK;
  }

  uint32_t capacity = num_handles ? *num_handles : 0;
  uint32_t required_capacity = base::checked_cast<uint32_t>(handles_.size());
  if (num_handles) {
    *num_handles = required_capacity;
  }
  if (!handles || capacity < required_capacity) {
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  base::ranges::copy(handles_, handles);
  handles_.clear();
  handles_consumed_ = true;
  return MOJO_RESULT_OK;
}

void MojoMessage::AttachDataPipePortals() {
  const size_t base_num_handles = handles_.size();
  for (size_t i = 0; i < base_num_handles; ++i) {
    if (auto* data_pipe = ipcz_driver::DataPipe::FromBox(handles_[i])) {
      handles_.push_back(data_pipe->TakePortal().release());
    }
  }
}

MojoResult MojoMessage::SetContext(uintptr_t context,
                                   MojoMessageContextSerializer serializer,
                                   MojoMessageContextDestructor destructor) {
  if (context_ && context) {
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (parcel_.is_valid() || data_storage_ || !handles_.empty()) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  context_ = context;
  serializer_ = serializer;
  destructor_ = destructor;
  return MOJO_RESULT_OK;
}

MojoResult MojoMessage::Serialize() {
  if (parcel_.is_valid() || data_storage_ || !handles_.empty()) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  if (!serializer_) {
    return MOJO_RESULT_NOT_FOUND;
  }

  const uintptr_t context = std::exchange(context_, 0);
  const MojoMessageContextSerializer serializer =
      std::exchange(serializer_, nullptr);
  const MojoMessageContextDestructor destructor =
      std::exchange(destructor_, nullptr);
  serializer(handle(), context);
  if (destructor) {
    destructor(context);
  }
  return MOJO_RESULT_OK;
}

// static
IpczResult MojoMessage::SerializeForIpcz(uintptr_t object,
                                         uint32_t,
                                         const void*,
                                         volatile void* data,
                                         size_t* num_bytes,
                                         IpczHandle* handles,
                                         size_t* num_handles) {
  return reinterpret_cast<MojoMessage*>(object)->SerializeForIpczImpl(
      data, num_bytes, handles, num_handles);
}

// static
void MojoMessage::DestroyForIpcz(uintptr_t object, uint32_t, const void*) {
  delete reinterpret_cast<MojoMessage*>(object);
}

// static
ScopedIpczHandle MojoMessage::Box(std::unique_ptr<MojoMessage> message) {
  const IpczBoxContents contents = {
      .size = sizeof(contents),
      .type = IPCZ_BOX_TYPE_APPLICATION_OBJECT,
      .object = {.application_object = message->handle()},
      .serializer = &SerializeForIpcz,
      .destructor = &DestroyForIpcz,
  };
  ScopedIpczHandle box;
  const IpczResult result =
      GetIpczAPI().Box(GetIpczNode(), &contents, IPCZ_NO_FLAGS, nullptr,
                       ScopedIpczHandle::Receiver(box));
  CHECK_EQ(IPCZ_RESULT_OK, result);
  std::ignore = message.release();
  return box;
}

// static
std::unique_ptr<MojoMessage> MojoMessage::UnwrapFrom(MojoMessage& message) {
  if (!message.data().empty() || message.handles().size() != 1) {
    // Wrapped messages are always a single box with no additional data.
    return nullptr;
  }

  const IpczHandle box = message.handles()[0];
  IpczBoxContents contents = {.size = sizeof(contents)};
  const IpczResult peek_result =
      GetIpczAPI().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &contents);
  if (peek_result != IPCZ_RESULT_OK) {
    return nullptr;
  }

  if (contents.type != IPCZ_BOX_TYPE_APPLICATION_OBJECT &&
      contents.type != IPCZ_BOX_TYPE_SUBPARCEL) {
    // Wrapped messages must always be represented either by a direct
    // application object reference, or a serialized subparcel.
    return nullptr;
  }

  const IpczResult unbox_result =
      GetIpczAPI().Unbox(box, IPCZ_NO_FLAGS, nullptr, &contents);
  DCHECK_EQ(IPCZ_RESULT_OK, unbox_result);

  // The box is now gone. Reset the handle within the input message so that it
  // doesn't attempt to close the box on destruction.
  message.handles()[0] = IPCZ_INVALID_HANDLE;

  if (contents.type == IPCZ_BOX_TYPE_APPLICATION_OBJECT) {
    // The wrapped message was never serialized, so we can recover it as-is.
    return base::WrapUnique(
        reinterpret_cast<MojoMessage*>(contents.object.application_object));
  }

  DCHECK_EQ(contents.type, IPCZ_BOX_TYPE_SUBPARCEL);
  ScopedIpczHandle subparcel(contents.object.subparcel);
  size_t num_bytes = 0;
  size_t num_handles = 0;
  const IpczResult get_query_result =
      GetIpczAPI().Get(subparcel.get(), IPCZ_NO_FLAGS, nullptr, nullptr,
                       &num_bytes, nullptr, &num_handles, nullptr);
  if (get_query_result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    return nullptr;
  }

  void* buffer;
  std::vector<IpczHandle> handles(num_handles);
  auto new_message = std::make_unique<ipcz_driver::MojoMessage>();
  const MojoResult append_result = new_message->AppendData(
      base::checked_cast<uint32_t>(num_bytes), nullptr, 0, &buffer, nullptr,
      /*commit_size=*/true);
  if (append_result != MOJO_RESULT_OK) {
    return nullptr;
  }

  // Retrieve all data and handles from the subparcel and move them into the
  // new MojoMessage object.
  new_message->handles().resize(num_handles);
  const IpczResult get_result = GetIpczAPI().Get(
      subparcel.get(), IPCZ_NO_FLAGS, nullptr, buffer, &num_bytes,
      new_message->handles().data(), &num_handles, nullptr);
  if (get_result != IPCZ_RESULT_OK) {
    return nullptr;
  }

  return new_message;
}

IpczResult MojoMessage::SerializeForIpczImpl(volatile void* data,
                                             size_t* num_bytes,
                                             IpczHandle* handles,
                                             size_t* num_handles) {
  // NOTE: MOJO_RESULT_FAILED_PRECONDITION here indicates that the message is
  // already internally serialized, so it's not an error and we can
  // proceed with extracting the serialized contents below.
  const MojoResult result = Serialize();
  if (result != MOJO_RESULT_OK && result != MOJO_RESULT_FAILED_PRECONDITION) {
    // Any other result indicates that the message cannot be serialized.
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  const size_t required_byte_capacity = data_.size();
  const size_t required_handle_capacity = handles_.size();
  const size_t byte_capacity = num_bytes ? *num_bytes : 0;
  const size_t handle_capacity = num_handles ? *num_handles : 0;
  if (num_bytes) {
    *num_bytes = required_byte_capacity;
  }
  if (num_handles) {
    *num_handles = required_handle_capacity;
  }
  if (byte_capacity < required_byte_capacity ||
      handle_capacity < required_handle_capacity) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }
  if ((byte_capacity && !data) || (handle_capacity && !handles)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  // TODO(crbug.com/40270656): Do a volatile-friendly copy here.
  memcpy(const_cast<void*>(data), data_.data(), data_.size());
  for (size_t i = 0; i < handles_.size(); ++i) {
    handles[i] = std::exchange(handles_[i], IPCZ_INVALID_HANDLE);
  }
  return IPCZ_RESULT_OK;
}

}  // namespace mojo::core::ipcz_driver
