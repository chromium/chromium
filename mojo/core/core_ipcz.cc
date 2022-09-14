// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_ipcz.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "mojo/core/ipcz_driver/invitation.h"
#include "mojo/core/ipcz_driver/mojo_trap.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/shared_buffer_mapping.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

namespace {

// The ipcz-based implementation of Mojo message objects. ipcz API exposes no
// notion of message objects, so this is merely heap storage for data and ipcz
// handles.
class MojoMessage {
 public:
  // Even with an input size of 0, MojoAppendMessageData is expected to allocate
  // *some* storage for message data. This constant therefore sets a lower bound
  // on payload allocation size. 32 bytes is chosen since it's the smallest
  // possible Mojo bindings message size (v0 header + 8 byte payload)
  static constexpr size_t kMinBufferSize = 32;

  MojoMessage() = default;
  MojoMessage(std::vector<uint8_t> data, std::vector<IpczHandle> handles) {
    SetContents(std::move(data), std::move(handles), IPCZ_INVALID_HANDLE);
  }

  ~MojoMessage() {
    for (IpczHandle handle : handles_) {
      if (handle != IPCZ_INVALID_HANDLE) {
        GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
      }
    }

    if (validator_ != IPCZ_INVALID_HANDLE) {
      GetIpczAPI().Close(validator_, IPCZ_NO_FLAGS, nullptr);
    }

    if (destructor_) {
      destructor_(context_);
    }
  }

  static MojoMessage* FromHandle(MojoMessageHandle handle) {
    return reinterpret_cast<MojoMessage*>(handle);
  }

  static std::unique_ptr<MojoMessage> TakeFromHandle(MojoMessageHandle handle) {
    return base::WrapUnique(FromHandle(handle));
  }

  MojoMessageHandle handle() const {
    return reinterpret_cast<MojoMessageHandle>(this);
  }

  base::span<uint8_t> data() { return data_; }
  std::vector<IpczHandle>& handles() { return handles_; }
  uintptr_t context() const { return context_; }

  IpczHandle validator() const { return validator_; }

  bool SetContents(std::vector<uint8_t> data,
                   std::vector<IpczHandle> handles,
                   IpczHandle validator) {
    const size_t size = data.size();
    if (size >= kMinBufferSize) {
      data_storage_ = std::move(data);
    } else {
      data_storage_.resize(kMinBufferSize);
      std::copy(data.begin(), data.end(), data_storage_.begin());
    }

    validator_ = validator;
    data_ = base::make_span(data_storage_).first(size);
    size_committed_ = true;
    if (handles.empty()) {
      return true;
    }

    // If there are any serialized DataPipe objects, accumulate them so we can
    // pluck their portals off the end of `handles`. Their portals were
    // attached the end of `handles` when the sender finalized the message in
    // MojoWriteMessageIpcz().
    std::vector<ipcz_driver::DataPipe*> data_pipes;
    for (IpczHandle handle : handles) {
      if (auto* data_pipe = ipcz_driver::DataPipe::FromBox(handle)) {
        data_pipes.push_back(data_pipe);
      }
    }

    if (handles.size() / 2 < data_pipes.size()) {
      // There must be at least enough handles for each DataPipe box AND its
      // portal.
      return false;
    }

    // The last N handles are portals for the pipes in `data_pipes`, in order.
    // Remove them from the message's handles and give them to their data pipes.
    const size_t first_data_pipe_portal = handles.size() - data_pipes.size();
    for (size_t i = 0; i < data_pipes.size(); ++i) {
      const IpczHandle handle = handles[first_data_pipe_portal + i];
      if (ipcz_driver::ObjectBase::FromBox(handle)) {
        // The handle in this position needs to be a portal. If it's a driver
        // object, something is wrong.
        return false;
      }

      data_pipes[i]->AdoptPortal(handle);
    }
    handles.resize(first_data_pipe_portal);
    handles_ = std::move(handles);
    return true;
  }

  MojoResult AppendData(uint32_t additional_num_bytes,
                        const MojoHandle* handles,
                        uint32_t num_handles,
                        void** buffer,
                        uint32_t* buffer_size,
                        bool commit_size) {
    if (context_ || size_committed_) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    const size_t new_data_size = data_.size() + additional_num_bytes;
    const size_t required_storage_size =
        std::max(new_data_size, kMinBufferSize);
    if (required_storage_size > data_storage_.size()) {
      data_storage_.resize(std::max(data_.size() * 2, required_storage_size));
    }
    data_ = base::make_span(data_storage_).first(new_data_size);

    handles_.reserve(handles_.size() + num_handles);
    for (MojoHandle handle : base::make_span(handles, num_handles)) {
      handles_.push_back(handle);
    }
    if (buffer) {
      *buffer = data_storage_.data();
    }
    if (buffer_size) {
      *buffer_size = base::checked_cast<uint32_t>(data_storage_.size());
    }
    size_committed_ = commit_size;
    return MOJO_RESULT_OK;
  }

  IpczResult GetData(void** buffer,
                     uint32_t* num_bytes,
                     MojoHandle* handles,
                     uint32_t* num_handles,
                     bool consume_handles) {
    if (context_ || !size_committed_) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }
    if (consume_handles && handles_consumed_) {
      return MOJO_RESULT_NOT_FOUND;
    }

    if (buffer) {
      *buffer = data_storage_.data();
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

    std::copy(handles_.begin(), handles_.end(), handles);
    handles_.clear();
    handles_consumed_ = true;
    return MOJO_RESULT_OK;
  }

  // Finalizes the Message by ensuring that any attached DataPipe objects also
  // attach their portals alongside the existing attachments. This operation is
  // balanced within SetContents(), where DataPipes extract their portals from
  // the tail end of the attached handles.
  void AttachDataPipePortals() {
    const size_t base_num_handles = handles_.size();
    for (size_t i = 0; i < base_num_handles; ++i) {
      if (auto* data_pipe = ipcz_driver::DataPipe::FromBox(handles_[i])) {
        handles_.push_back(data_pipe->TakePortal());
      }
    }
  }

  MojoResult SetContext(uintptr_t context,
                        MojoMessageContextSerializer serializer,
                        MojoMessageContextDestructor destructor) {
    if (context_) {
      return MOJO_RESULT_ALREADY_EXISTS;
    }
    if (!data_storage_.empty() || !handles_.empty()) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    context_ = context;
    serializer_ = serializer;
    destructor_ = destructor;
    return MOJO_RESULT_OK;
  }

  MojoResult Serialize() {
    if (!data_storage_.empty() || !handles_.empty()) {
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

 private:
  IpczHandle validator_ = IPCZ_INVALID_HANDLE;
  std::vector<uint8_t> data_storage_;
  base::span<uint8_t> data_;
  std::vector<IpczHandle> handles_;
  bool handles_consumed_ = false;
  bool size_committed_ = false;

  // Unserialized message state. These values are provided by the application
  // calling MojoSetMessageContext() for lazy serialization. `context_` is an
  // arbitrary opaque value. `serializer_` is invoked when the application must
  // produce a serialized message, with `context_` as an input. `destructor_` is
  // if non-null is called to clean up any application state associated with
  // `context_`.
  //
  // If `context_` is zero, then no unserialized message context has been set by
  // the application.
  //
  // NOTE: Although lazy serialization is technically supported through these
  // APIs, MojoIpcz always eagerly serializes such messages within
  // MojoWriteMessageIpcz() even if the peer is local.
  uintptr_t context_ = 0;
  MojoMessageContextSerializer serializer_ = nullptr;
  MojoMessageContextDestructor destructor_ = nullptr;
};

// Tracks active Mojo memory mappings by base address, since that's how the Mojo
// API identifies them for unmapping.
class MappingTable {
 public:
  MappingTable() = default;
  ~MappingTable() = default;

  void Add(scoped_refptr<ipcz_driver::SharedBufferMapping> mapping) {
    base::AutoLock lock(lock_);
    void* address = mapping->memory();
    mappings_.emplace(address, std::move(mapping));
  }

  MojoResult Remove(void* address) {
    base::AutoLock lock(lock_);
    if (!mappings_.erase(address)) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    return MOJO_RESULT_OK;
  }

 private:
  base::Lock lock_;
  std::map<void*, scoped_refptr<ipcz_driver::SharedBufferMapping>> mappings_
      GUARDED_BY(lock_);
};

MappingTable& GetMappingTable() {
  static base::NoDestructor<MappingTable> table;
  return *table;
}

// ipcz get and put operations differ slightly in their return code semantics as
// compared to Mojo read and write operations. These helpers perform the
// translation.
MojoResult GetMojoReadResultForIpczGet(IpczResult result) {
  if (result == IPCZ_RESULT_UNAVAILABLE) {
    // The peer is still open but there are not currently any parcels to read.
    return MOJO_RESULT_SHOULD_WAIT;
  }
  if (result == IPCZ_RESULT_NOT_FOUND) {
    // There are no more parcels to read and the peer is closed.
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  return result;
}

MojoResult GetMojoWriteResultForIpczPut(IpczResult result) {
  if (result == IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    // For put operations with limits, which are used to emulate data pipe
    // producer writes, this indicates that the caller needs to try again later
    // due to the pipe being at capacity.
    return MOJO_RESULT_SHOULD_WAIT;
  }
  if (result == IPCZ_RESULT_NOT_FOUND) {
    // The peer is closed.
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  return result;
}

extern "C" {

MojoResult MojoInitializeIpcz(const struct MojoInitializeOptions* options) {
  NOTREACHED();
  return MOJO_RESULT_OK;
}

MojoTimeTicks MojoGetTimeTicksNowIpcz() {
  return base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
}

MojoResult MojoCloseIpcz(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
}

MojoResult MojoQueryHandleSignalsStateIpcz(
    MojoHandle handle,
    MojoHandleSignalsState* signals_state) {
  if (handle == MOJO_HANDLE_INVALID || !signals_state) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  // If `handle` refers to a data pipe, its exposed signals differ slightly from
  // those of a message pipe.
  auto* data_pipe = ipcz_driver::DataPipe::FromBox(handle);
  scoped_refptr<ipcz_driver::DataPipe::PortalWrapper> data_portal;
  if (data_pipe) {
    data_portal = data_pipe->GetPortal();
    if (!data_portal) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    handle = data_portal->handle();
  }

  IpczPortalStatus status = {sizeof(status)};
  IpczResult result =
      GetIpczAPI().QueryPortalStatus(handle, IPCZ_NO_FLAGS, nullptr, &status);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  signals_state->satisfiable_signals = MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  if (!data_pipe) {
    // Data pipes do not expose signals related to message quotas.
    signals_state->satisfiable_signals |= MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;
  }
  signals_state->satisfied_signals = 0;
  if (status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) {
    signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  } else {
    signals_state->satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_REMOTE;
    if (!data_pipe || data_pipe->is_producer()) {
      signals_state->satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    }
    if (!data_pipe || status.num_remote_bytes < data_pipe->byte_capacity()) {
      signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    }
  }
  if ((status.flags & IPCZ_PORTAL_STATUS_DEAD) == 0) {
    if (!data_pipe) {
      signals_state->satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
    } else if (data_pipe->is_consumer()) {
      signals_state->satisfiable_signals |=
          MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
    }
  }
  if (status.num_local_parcels > 0) {
    signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
    if (data_pipe && data_pipe->is_consumer() && data_pipe->HasNewData()) {
      signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
    }
  }
  return MOJO_RESULT_OK;
}

MojoResult MojoCreateMessagePipeIpcz(
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  return GetIpczAPI().OpenPortals(GetIpczNode(), IPCZ_NO_FLAGS, nullptr,
                                  message_pipe_handle0, message_pipe_handle1);
}

MojoResult MojoWriteMessageIpcz(MojoHandle message_pipe_handle,
                                MojoMessageHandle message,
                                const MojoWriteMessageOptions* options) {
  auto m = MojoMessage::TakeFromHandle(message);
  if (!m || !message_pipe_handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  m->Serialize();
  m->AttachDataPipePortals();
  IpczResult result = GetIpczAPI().Put(
      message_pipe_handle, m->data().data(), m->data().size(),
      m->handles().data(), m->handles().size(), IPCZ_NO_FLAGS, nullptr);
  if (result == IPCZ_RESULT_NOT_FOUND) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  if (result == IPCZ_RESULT_OK) {
    // Ensure the handles don't get freed on MojoMessage destruction, as their
    // ownership was relinquished in Put() above.
    m->handles().clear();
  }

  return GetMojoWriteResultForIpczPut(result);
}

MojoResult MojoReadMessageIpcz(MojoHandle message_pipe_handle,
                               const MojoReadMessageOptions* options,
                               MojoMessageHandle* message) {
  std::vector<uint8_t> data;
  std::vector<MojoHandle> handles;
  size_t num_bytes = 0;
  size_t num_handles = 0;
  IpczHandle validator;
  IpczResult result =
      GetIpczAPI().Get(message_pipe_handle, IPCZ_NO_FLAGS, nullptr, nullptr,
                       &num_bytes, nullptr, &num_handles, &validator);
  if (result == IPCZ_RESULT_OK) {
    auto new_message = std::make_unique<MojoMessage>();
    new_message->SetContents({}, {}, validator);
    *message = new_message.release()->handle();
    return MOJO_RESULT_OK;
  }

  if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    return GetMojoReadResultForIpczGet(result);
  }

  data.resize(num_bytes);
  handles.resize(num_handles);
  result =
      GetIpczAPI().Get(message_pipe_handle, IPCZ_NO_FLAGS, nullptr, data.data(),
                       &num_bytes, handles.data(), &num_handles, &validator);
  if (result != IPCZ_RESULT_OK) {
    return GetMojoReadResultForIpczGet(result);
  }

  auto m = std::make_unique<MojoMessage>();
  if (!m->SetContents(std::move(data), std::move(handles), validator)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  *message = m.release()->handle();
  return MOJO_RESULT_OK;
}

MojoResult MojoFuseMessagePipesIpcz(
    MojoHandle handle0,
    MojoHandle handle1,
    const MojoFuseMessagePipesOptions* options) {
  if (handle0 == MOJO_HANDLE_INVALID || handle1 == MOJO_HANDLE_INVALID) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  const IpczResult result =
      GetIpczAPI().MergePortals(handle0, handle1, IPCZ_NO_FLAGS, nullptr);
  if (result != IPCZ_RESULT_OK) {
    // On failure, MojoFuseMessagePipes is expected to close the message pipe
    // endpoints it was given.
    MojoCloseIpcz(handle0);
    MojoCloseIpcz(handle1);
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  return result;
}

MojoResult MojoCreateMessageIpcz(const MojoCreateMessageOptions* options,
                                 MojoMessageHandle* message) {
  if (!message) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  auto new_message = std::make_unique<MojoMessage>();
  *message = new_message.release()->handle();
  return MOJO_RESULT_OK;
}

MojoResult MojoDestroyMessageIpcz(MojoMessageHandle message) {
  if (message == MOJO_MESSAGE_HANDLE_INVALID) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  std::unique_ptr<MojoMessage> scoped_message(MojoMessage::FromHandle(message));
  return scoped_message ? MOJO_RESULT_OK : MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoSerializeMessageIpcz(
    MojoMessageHandle message,
    const MojoSerializeMessageOptions* options) {
  if (auto* m = MojoMessage::FromHandle(message)) {
    return m->Serialize();
  }
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoAppendMessageDataIpcz(
    MojoMessageHandle message,
    uint32_t additional_payload_size,
    const MojoHandle* handles,
    uint32_t num_handles,
    const MojoAppendMessageDataOptions* options,
    void** buffer,
    uint32_t* buffer_size) {
  if (auto* m = MojoMessage::FromHandle(message)) {
    const bool commit_size =
        options && (options->flags & MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE);
    return m->AppendData(additional_payload_size, handles, num_handles, buffer,
                         buffer_size, commit_size);
  }
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoGetMessageDataIpcz(MojoMessageHandle message,
                                  const MojoGetMessageDataOptions* options,
                                  void** buffer,
                                  uint32_t* num_bytes,
                                  MojoHandle* handles,
                                  uint32_t* num_handles) {
  if (auto* m = MojoMessage::FromHandle(message)) {
    const bool consume_handles =
        !options ||
        ((options->flags & MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES) == 0);
    return m->GetData(buffer, num_bytes, handles, num_handles, consume_handles);
  }
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoSetMessageContextIpcz(
    MojoMessageHandle message_handle,
    uintptr_t context,
    MojoMessageContextSerializer serializer,
    MojoMessageContextDestructor destructor,
    const MojoSetMessageContextOptions* options) {
  auto* message = MojoMessage::FromHandle(message_handle);
  if (!message) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return message->SetContext(context, serializer, destructor);
}

MojoResult MojoGetMessageContextIpcz(
    MojoMessageHandle message_handle,
    const MojoGetMessageContextOptions* options,
    uintptr_t* context) {
  auto* message = MojoMessage::FromHandle(message_handle);
  if (!message) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  if (!message->context()) {
    return MOJO_RESULT_NOT_FOUND;
  }
  *context = message->context();
  return MOJO_RESULT_OK;
}

MojoResult MojoNotifyBadMessageIpcz(
    MojoMessageHandle message_handle,
    const char* error,
    uint32_t error_num_bytes,
    const MojoNotifyBadMessageOptions* options) {
  auto* message = MojoMessage::FromHandle(message_handle);
  if (!message) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  const std::string error_string(error, error_num_bytes);
  if (message->validator() != IPCZ_INVALID_HANDLE) {
    // Mojo prefixes bad message reports with this string if they're for
    // messages from a remote node. We duplicate it here since many tests expect
    // observation of prefixed error messages.
    const char kPrefix[] = "Received bad user message: ";
    auto prefixed_error_message =
        std::make_unique<std::string>(base::StrCat({kPrefix, error_string}));
    const IpczResult result = GetIpczAPI().Reject(
        message->validator(),
        reinterpret_cast<uintptr_t>(prefixed_error_message.get()),
        IPCZ_NO_FLAGS, nullptr);
    if (result == IPCZ_RESULT_OK) {
      // Ownership taken by driver.
      std::ignore = prefixed_error_message.release();
      return IPCZ_RESULT_OK;
    }
    DCHECK_EQ(result, IPCZ_RESULT_FAILED_PRECONDITION);
  }

  // The parcel was not from a remote node in this case.
  ipcz_driver::Invitation::InvokeDefaultProcessErrorHandler(error_string);
  return MOJO_RESULT_OK;
}

MojoResult MojoCreateDataPipeIpcz(const MojoCreateDataPipeOptions* options,
                                  MojoHandle* data_pipe_producer_handle,
                                  MojoHandle* data_pipe_consumer_handle) {
  using DataPipe = ipcz_driver::DataPipe;

  // Mojo Core defaults to 64 kB capacity and 1-byte elements, and many callers
  // assume these defaults.
  constexpr uint32_t kDefaultCapacity = 64 * 1024;
  DataPipe::Config config = {.element_size = 1,
                             .byte_capacity = kDefaultCapacity};
  if (options) {
    config.element_size = options->element_num_bytes;
    if (options->capacity_num_bytes) {
      config.byte_capacity = options->capacity_num_bytes;
    }
  }
  DataPipe::Pair pipe = DataPipe::CreatePair(config);
  *data_pipe_producer_handle = DataPipe::Box(std::move(pipe.producer));
  *data_pipe_consumer_handle = DataPipe::Box(std::move(pipe.consumer));
  return MOJO_RESULT_OK;
}

MojoResult MojoWriteDataIpcz(MojoHandle data_pipe_producer_handle,
                             const void* elements,
                             uint32_t* num_bytes,
                             const MojoWriteDataOptions* options) {
  auto* pipe = ipcz_driver::DataPipe::FromBox(data_pipe_producer_handle);
  if (!pipe || !num_bytes || pipe->byte_capacity() == 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return GetMojoWriteResultForIpczPut(
      pipe->WriteData(elements, *num_bytes,
                      options ? options->flags : MOJO_WRITE_DATA_FLAG_NONE));
}

MojoResult MojoBeginWriteDataIpcz(MojoHandle data_pipe_producer_handle,
                                  const MojoBeginWriteDataOptions* options,
                                  void** buffer,
                                  uint32_t* buffer_num_bytes) {
  auto* pipe = ipcz_driver::DataPipe::FromBox(data_pipe_producer_handle);
  if (!pipe || !buffer || !buffer_num_bytes || pipe->byte_capacity() == 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  if (options && options->struct_size < sizeof(*options)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  const MojoBeginWriteDataFlags flags =
      options ? options->flags : MOJO_BEGIN_WRITE_DATA_FLAG_NONE;
  return GetMojoWriteResultForIpczPut(
      pipe->BeginWriteData(*buffer, *buffer_num_bytes, flags));
}

MojoResult MojoEndWriteDataIpcz(MojoHandle data_pipe_producer_handle,
                                uint32_t num_bytes_produced,
                                const MojoEndWriteDataOptions* options) {
  auto* pipe = ipcz_driver::DataPipe::FromBox(data_pipe_producer_handle);
  if (!pipe || pipe->byte_capacity() == 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return GetMojoWriteResultForIpczPut(pipe->EndWriteData(num_bytes_produced));
}

MojoResult MojoReadDataIpcz(MojoHandle data_pipe_consumer_handle,
                            const MojoReadDataOptions* options,
                            void* elements,
                            uint32_t* num_bytes) {
  const MojoReadDataFlags flags =
      options ? options->flags : MOJO_READ_DATA_FLAG_NONE;
  auto* pipe = ipcz_driver::DataPipe::FromBox(data_pipe_consumer_handle);
  if (!pipe || !num_bytes || pipe->byte_capacity() > 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return GetMojoReadResultForIpczGet(
      pipe->ReadData(elements, *num_bytes, flags));
}

MojoResult MojoBeginReadDataIpcz(MojoHandle data_pipe_consumer_handle,
                                 const MojoBeginReadDataOptions* options,
                                 const void** buffer,
                                 uint32_t* buffer_num_bytes) {
  auto* pipe = ipcz_driver::DataPipe::FromBox(data_pipe_consumer_handle);
  if (!pipe || !buffer || !buffer_num_bytes || pipe->byte_capacity() > 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return GetMojoReadResultForIpczGet(
      pipe->BeginReadData(*buffer, *buffer_num_bytes));
}

MojoResult MojoEndReadDataIpcz(MojoHandle data_pipe_consumer_handle,
                               uint32_t num_bytes_consumed,
                               const MojoEndReadDataOptions* options) {
  auto* pipe = ipcz_driver::DataPipe::FromBox(data_pipe_consumer_handle);
  if (!pipe || pipe->byte_capacity() > 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return GetMojoReadResultForIpczGet(pipe->EndReadData(num_bytes_consumed));
}

MojoResult MojoCreateSharedBufferIpcz(
    uint64_t num_bytes,
    const MojoCreateSharedBufferOptions* options,
    MojoHandle* shared_buffer_handle) {
  auto region = base::WritableSharedMemoryRegion::Create(num_bytes);
  if (!region.IsValid()) {
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *shared_buffer_handle = ipcz_driver::SharedBuffer::MakeBoxed(
      base::WritableSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)));
  return MOJO_RESULT_OK;
}

MojoResult MojoDuplicateBufferHandleIpcz(
    MojoHandle buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  auto* buffer = ipcz_driver::SharedBuffer::FromBox(buffer_handle);
  if (!buffer || !new_buffer_handle ||
      (options && options->struct_size < sizeof(*options))) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  const bool read_only =
      options &&
      (options->flags & MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY) != 0;
  auto [new_buffer, result] = buffer->Duplicate(read_only);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  *new_buffer_handle = ipcz_driver::SharedBuffer::Box(std::move(new_buffer));
  return MOJO_RESULT_OK;
}

MojoResult MojoMapBufferIpcz(MojoHandle buffer_handle,
                             uint64_t offset,
                             uint64_t num_bytes,
                             const MojoMapBufferOptions* options,
                             void** address) {
  auto* buffer = ipcz_driver::SharedBuffer::FromBox(buffer_handle);
  if (!buffer) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  auto mapping = ipcz_driver::SharedBufferMapping::Create(
      buffer->region(), static_cast<size_t>(offset),
      static_cast<size_t>(num_bytes));
  if (!mapping) {
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }
  *address = mapping->memory();
  GetMappingTable().Add(std::move(mapping));
  return MOJO_RESULT_OK;
}

MojoResult MojoUnmapBufferIpcz(void* address) {
  return GetMappingTable().Remove(address);
}

MojoResult MojoGetBufferInfoIpcz(MojoHandle buffer_handle,
                                 const MojoGetBufferInfoOptions* options,
                                 MojoSharedBufferInfo* info) {
  auto* buffer = ipcz_driver::SharedBuffer::FromBox(buffer_handle);
  if (!buffer || !info || info->struct_size < sizeof(*info)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  info->size = buffer->region().GetSize();
  return MOJO_RESULT_OK;
}

MojoResult MojoCreateTrapIpcz(MojoTrapEventHandler handler,
                              const MojoCreateTrapOptions* options,
                              MojoHandle* trap_handle) {
  if (!handler || !trap_handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  *trap_handle = ipcz_driver::MojoTrap::MakeBoxed(handler);
  return MOJO_RESULT_OK;
}

MojoResult MojoAddTriggerIpcz(MojoHandle trap_handle,
                              MojoHandle handle,
                              MojoHandleSignals signals,
                              MojoTriggerCondition condition,
                              uintptr_t context,
                              const MojoAddTriggerOptions* options) {
  auto* trap = ipcz_driver::MojoTrap::FromBox(trap_handle);
  if (!trap) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return trap->AddTrigger(handle, signals, condition, context);
}

MojoResult MojoRemoveTriggerIpcz(MojoHandle trap_handle,
                                 uintptr_t context,
                                 const MojoRemoveTriggerOptions* options) {
  auto* trap = ipcz_driver::MojoTrap::FromBox(trap_handle);
  if (!trap) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return trap->RemoveTrigger(context);
}

MojoResult MojoArmTrapIpcz(MojoHandle trap_handle,
                           const MojoArmTrapOptions* options,
                           uint32_t* num_blocking_events,
                           MojoTrapEvent* blocking_events) {
  auto* trap = ipcz_driver::MojoTrap::FromBox(trap_handle);
  if (!trap) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return trap->Arm(blocking_events, num_blocking_events);
}

MojoResult MojoWrapPlatformHandleIpcz(
    const MojoPlatformHandle* platform_handle,
    const MojoWrapPlatformHandleOptions* options,
    MojoHandle* mojo_handle) {
  if (!platform_handle || !mojo_handle ||
      platform_handle->struct_size < sizeof(*platform_handle)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  auto handle = PlatformHandle::FromMojoPlatformHandle(platform_handle);
  *mojo_handle =
      ipcz_driver::WrappedPlatformHandle::MakeBoxed(std::move(handle));
  return MOJO_RESULT_OK;
}

MojoResult MojoUnwrapPlatformHandleIpcz(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformHandleOptions* options,
    MojoPlatformHandle* platform_handle) {
  if (!mojo_handle || !platform_handle ||
      platform_handle->struct_size < sizeof(*platform_handle)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  auto wrapper = ipcz_driver::WrappedPlatformHandle::Unbox(mojo_handle);
  if (!wrapper) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  PlatformHandle::ToMojoPlatformHandle(std::move(wrapper->handle()),
                                       platform_handle);
  return MOJO_RESULT_OK;
}

MojoResult MojoWrapPlatformSharedMemoryRegionIpcz(
    const MojoPlatformHandle* platform_handles,
    uint32_t num_platform_handles,
    uint64_t num_bytes,
    const MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode access_mode,
    const MojoWrapPlatformSharedMemoryRegionOptions* options,
    MojoHandle* mojo_handle) {
  if (!platform_handles || !num_bytes || !guid || !mojo_handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  auto buffer = ipcz_driver::SharedBuffer::CreateForMojoWrapper(
      base::make_span(platform_handles, num_platform_handles), num_bytes, *guid,
      access_mode);
  if (!buffer) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  *mojo_handle = ipcz_driver::SharedBuffer::Box(std::move(buffer));
  return MOJO_RESULT_OK;
}

MojoResult MojoUnwrapPlatformSharedMemoryRegionIpcz(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformSharedMemoryRegionOptions* options,
    MojoPlatformHandle* platform_handles,
    uint32_t* num_platform_handles,
    uint64_t* num_bytes,
    MojoSharedBufferGuid* mojo_guid,
    MojoPlatformSharedMemoryRegionAccessMode* access_mode) {
  if (!mojo_handle || !platform_handles || !num_platform_handles ||
      !mojo_guid) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  auto* buffer = ipcz_driver::SharedBuffer::FromBox(mojo_handle);
  if (!buffer) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  using Mode = base::subtle::PlatformSharedMemoryRegion::Mode;
  const Mode mode = buffer->region().GetMode();
  const base::UnguessableToken guid = buffer->region().GetGUID();
  const uint32_t size = static_cast<uint32_t>(buffer->region().GetSize());

  uint32_t capacity = *num_platform_handles;
  uint32_t required_handles = 1;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
  if (buffer->region().GetMode() ==
      base::subtle::PlatformSharedMemoryRegion::Mode::kWritable) {
    required_handles = 2;
  }
#endif
  *num_platform_handles = required_handles;
  if (capacity < required_handles) {
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  PlatformHandle handles[2];
  base::subtle::ScopedPlatformSharedMemoryHandle region_handle =
      buffer->region().PassPlatformHandle();
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
  handles[0] = PlatformHandle(std::move(region_handle.fd));
  handles[1] = PlatformHandle(std::move(region_handle.readonly_fd));
#else
  handles[0] = PlatformHandle(std::move(region_handle));
#endif

  for (size_t i = 0; i < required_handles; ++i) {
    PlatformHandle::ToMojoPlatformHandle(std::move(handles[i]),
                                         &platform_handles[i]);
  }

  *num_bytes = size;
  mojo_guid->high = guid.GetHighForSerialization();
  mojo_guid->low = guid.GetLowForSerialization();

  switch (mode) {
    case Mode::kReadOnly:
      *access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY;
      break;
    case Mode::kWritable:
      *access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE;
      break;
    case Mode::kUnsafe:
      *access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE;
      break;
    default:
      *access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY;
      NOTREACHED();
  }

  std::ignore = ipcz_driver::SharedBuffer::Unbox(mojo_handle);
  return MOJO_RESULT_OK;
}

MojoResult MojoCreateInvitationIpcz(const MojoCreateInvitationOptions* options,
                                    MojoHandle* invitation_handle) {
  if (!invitation_handle ||
      (options && options->struct_size < sizeof(*options))) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  *invitation_handle = ipcz_driver::Invitation::MakeBoxed();
  return MOJO_RESULT_OK;
}

MojoResult MojoAttachMessagePipeToInvitationIpcz(
    MojoHandle invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoAttachMessagePipeToInvitationOptions* options,
    MojoHandle* message_pipe_handle) {
  auto* invitation = ipcz_driver::Invitation::FromBox(invitation_handle);
  if (!invitation || !message_pipe_handle ||
      (options && options->struct_size < sizeof(*options))) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return invitation->Attach(
      base::make_span(static_cast<const uint8_t*>(name), name_num_bytes),
      message_pipe_handle);
}

MojoResult MojoExtractMessagePipeFromInvitationIpcz(
    MojoHandle invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoExtractMessagePipeFromInvitationOptions* options,
    MojoHandle* message_pipe_handle) {
  auto* invitation = ipcz_driver::Invitation::FromBox(invitation_handle);
  if (!invitation || !message_pipe_handle ||
      (options && options->struct_size < sizeof(*options))) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return invitation->Extract(
      base::make_span(static_cast<const uint8_t*>(name), name_num_bytes),
      message_pipe_handle);
}

MojoResult MojoSendInvitationIpcz(
    MojoHandle invitation_handle,
    const MojoPlatformProcessHandle* process_handle,
    const MojoInvitationTransportEndpoint* transport_endpoint,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    const MojoSendInvitationOptions* options) {
  auto* invitation = ipcz_driver::Invitation::FromBox(invitation_handle);
  if (!invitation) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  const MojoResult result =
      invitation->Send(process_handle, transport_endpoint, error_handler,
                       error_handler_context, options);
  if (result == MOJO_RESULT_OK) {
    // On success, the invitation is consumed.
    GetIpczAPI().Close(invitation_handle, IPCZ_NO_FLAGS, nullptr);
  }
  return result;
}

MojoResult MojoAcceptInvitationIpcz(
    const MojoInvitationTransportEndpoint* transport_endpoint,
    const MojoAcceptInvitationOptions* options,
    MojoHandle* invitation_handle) {
  if (!transport_endpoint || !invitation_handle ||
      (options && options->struct_size < sizeof(*options))) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  *invitation_handle =
      ipcz_driver::Invitation::Accept(transport_endpoint, options);
  return MOJO_RESULT_OK;
}

MojoResult MojoSetQuotaIpcz(MojoHandle handle,
                            MojoQuotaType type,
                            uint64_t limit,
                            const MojoSetQuotaOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoQueryQuotaIpcz(MojoHandle handle,
                              MojoQuotaType type,
                              const MojoQueryQuotaOptions* options,
                              uint64_t* current_limit,
                              uint64_t* current_usage) {
  IpczPortalStatus status = {.size = sizeof(status)};
  const IpczResult result =
      GetIpczAPI().QueryPortalStatus(handle, IPCZ_NO_FLAGS, nullptr, &status);
  DCHECK_EQ(result, IPCZ_RESULT_OK);
  if (type == MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH && current_usage) {
    *current_usage = status.num_local_parcels;
  } else if (type == MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE &&
             current_usage) {
    *current_usage = status.num_local_bytes;
  } else if (current_usage) {
    *current_usage = 0;
  }
  if (current_limit) {
    *current_limit = 0;
  }
  return MOJO_RESULT_OK;
}

MojoResult MojoShutdownIpcz(const MojoShutdownOptions* options) {
  NOTREACHED();
  return MOJO_RESULT_OK;
}

MojoResult MojoSetDefaultProcessErrorHandlerIpcz(
    MojoDefaultProcessErrorHandler handler,
    const MojoSetDefaultProcessErrorHandlerOptions* options) {
  ipcz_driver::Invitation::SetDefaultProcessErrorHandler(handler);
  return MOJO_RESULT_OK;
}

}  // extern "C"

MojoSystemThunks2 g_mojo_ipcz_thunks = {
    sizeof(MojoSystemThunks2),
    MojoInitializeIpcz,
    MojoGetTimeTicksNowIpcz,
    MojoCloseIpcz,
    MojoQueryHandleSignalsStateIpcz,
    MojoCreateMessagePipeIpcz,
    MojoWriteMessageIpcz,
    MojoReadMessageIpcz,
    MojoFuseMessagePipesIpcz,
    MojoCreateMessageIpcz,
    MojoDestroyMessageIpcz,
    MojoSerializeMessageIpcz,
    MojoAppendMessageDataIpcz,
    MojoGetMessageDataIpcz,
    MojoSetMessageContextIpcz,
    MojoGetMessageContextIpcz,
    MojoNotifyBadMessageIpcz,
    MojoCreateDataPipeIpcz,
    MojoWriteDataIpcz,
    MojoBeginWriteDataIpcz,
    MojoEndWriteDataIpcz,
    MojoReadDataIpcz,
    MojoBeginReadDataIpcz,
    MojoEndReadDataIpcz,
    MojoCreateSharedBufferIpcz,
    MojoDuplicateBufferHandleIpcz,
    MojoMapBufferIpcz,
    MojoUnmapBufferIpcz,
    MojoGetBufferInfoIpcz,
    MojoCreateTrapIpcz,
    MojoAddTriggerIpcz,
    MojoRemoveTriggerIpcz,
    MojoArmTrapIpcz,
    MojoWrapPlatformHandleIpcz,
    MojoUnwrapPlatformHandleIpcz,
    MojoWrapPlatformSharedMemoryRegionIpcz,
    MojoUnwrapPlatformSharedMemoryRegionIpcz,
    MojoCreateInvitationIpcz,
    MojoAttachMessagePipeToInvitationIpcz,
    MojoExtractMessagePipeFromInvitationIpcz,
    MojoSendInvitationIpcz,
    MojoAcceptInvitationIpcz,
    MojoSetQuotaIpcz,
    MojoQueryQuotaIpcz,
    MojoShutdownIpcz,
    MojoSetDefaultProcessErrorHandlerIpcz};

}  // namespace

const MojoSystemThunks2* GetMojoIpczImpl() {
  return &g_mojo_ipcz_thunks;
}

}  // namespace mojo::core
