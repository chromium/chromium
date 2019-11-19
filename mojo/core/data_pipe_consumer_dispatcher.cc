// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/data_pipe_consumer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "mojo/core/core.h"
#include "mojo/core/data_pipe_control_message.h"
#include "mojo/core/node_controller.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/core/request_context.h"
#include "mojo/core/user_message_impl.h"
#include "mojo/public/c/system/data_pipe.h"

namespace mojo {
namespace core {

namespace {

const uint8_t kFlagPeerClosed = 0x01;

#pragma pack(push, 1)

struct SerializedState {
  MojoCreateDataPipeOptions options;
  uint64_t pipe_id;
  uint32_t read_offset;
  uint32_t bytes_available;
  uint8_t flags;
  uint64_t buffer_guid_high;
  uint64_t buffer_guid_low;
  char padding[7];
};

static_assert(sizeof(SerializedState) % 8 == 0,
              "Invalid SerializedState size.");

#pragma pack(pop)

}  // namespace

// A PortObserver which forwards to a DataPipeConsumerDispatcher. This owns a
// reference to the dispatcher to ensure it lives as long as the observed port.
class DataPipeConsumerDispatcher::PortObserverThunk
    : public NodeController::PortObserver {
 public:
  explicit PortObserverThunk(
      scoped_refptr<DataPipeConsumerDispatcher> dispatcher)
      : dispatcher_(dispatcher) {}

 private:
  ~PortObserverThunk() override {}

  // NodeController::PortObserver:
  void OnPortStatusChanged() override { dispatcher_->OnPortStatusChanged(); }

  scoped_refptr<DataPipeConsumerDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PortObserverThunk);
};

// static
scoped_refptr<DataPipeConsumerDispatcher> DataPipeConsumerDispatcher::Create(
    NodeController* node_controller,
    const ports::PortRef& control_port,
    base::UnsafeSharedMemoryRegion shared_ring_buffer,
    const MojoCreateDataPipeOptions& options,
    uint64_t pipe_id) {
  scoped_refptr<DataPipeConsumerDispatcher> consumer =
      new DataPipeConsumerDispatcher(node_controller, control_port,
                                     std::move(shared_ring_buffer), options,
                                     pipe_id);
  base::AutoLock lock(consumer->lock_);
  if (!consumer->InitializeNoLock())
    return nullptr;
  return consumer;
}

Dispatcher::Type DataPipeConsumerDispatcher::GetType() const {
  return Type::DATA_PIPE_CONSUMER;
}

MojoResult DataPipeConsumerDispatcher::Close() {
  base::AutoLock lock(lock_);
  DVLOG(1) << "Closing data pipe consumer " << pipe_id_;
  return CloseNoLock();
}

MojoResult DataPipeConsumerDispatcher::ReadData(
    const MojoReadDataOptions& options,
    void* elements,
    uint32_t* num_bytes) {
  base::AutoLock lock(lock_);

  if (!shared_ring_buffer_.IsValid() || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  const bool had_new_data = new_data_available_;
  new_data_available_ = false;

  if ((options.flags & MOJO_READ_DATA_FLAG_QUERY)) {
    if ((options.flags & MOJO_READ_DATA_FLAG_PEEK) ||
        (options.flags & MOJO_READ_DATA_FLAG_DISCARD))
      return MOJO_RESULT_INVALID_ARGUMENT;
    DCHECK(!(options.flags & MOJO_READ_DATA_FLAG_DISCARD));  // Handled above.
    DVLOG_IF(2, elements) << "Query mode: ignoring non-null |elements|";
    *num_bytes = static_cast<uint32_t>(bytes_available_);

    if (had_new_data)
      watchers_.NotifyState(GetHandleSignalsStateNoLock());
    return MOJO_RESULT_OK;
  }

  bool discard = false;
  if ((options.flags & MOJO_READ_DATA_FLAG_DISCARD)) {
    // These flags are mutally exclusive.
    if (options.flags & MOJO_READ_DATA_FLAG_PEEK)
      return MOJO_RESULT_INVALID_ARGUMENT;
    DVLOG_IF(2, elements) << "Discard mode: ignoring non-null |elements|";
    discard = true;
  }

  uint32_t max_num_bytes_to_read = *num_bytes;
  if (max_num_bytes_to_read % options_.element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  bool all_or_none = options.flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_read = all_or_none ? max_num_bytes_to_read : 0;

  if (min_num_bytes_to_read > bytes_available_) {
    if (had_new_data)
      watchers_.NotifyState(GetHandleSignalsStateNoLock());
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_OUT_OF_RANGE;
  }

  uint32_t bytes_to_read = std::min(max_num_bytes_to_read, bytes_available_);
  if (bytes_to_read == 0) {
    if (had_new_data)
      watchers_.NotifyState(GetHandleSignalsStateNoLock());
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_SHOULD_WAIT;
  }

  if (!discard) {
    const uint8_t* data =
        static_cast<const uint8_t*>(ring_buffer_mapping_.memory());
    CHECK(data);

    uint8_t* destination = static_cast<uint8_t*>(elements);
    CHECK(destination);

    DCHECK_LE(read_offset_, options_.capacity_num_bytes);
    uint32_t tail_bytes_to_copy =
        std::min(options_.capacity_num_bytes - read_offset_, bytes_to_read);
    uint32_t head_bytes_to_copy = bytes_to_read - tail_bytes_to_copy;
    if (tail_bytes_to_copy > 0)
      memcpy(destination, data + read_offset_, tail_bytes_to_copy);
    if (head_bytes_to_copy > 0)
      memcpy(destination + tail_bytes_to_copy, data, head_bytes_to_copy);
  }
  *num_bytes = bytes_to_read;

  bool peek = !!(options.flags & MOJO_READ_DATA_FLAG_PEEK);
  if (discard || !peek) {
    read_offset_ = (read_offset_ + bytes_to_read) % options_.capacity_num_bytes;
    bytes_available_ -= bytes_to_read;

    base::AutoUnlock unlock(lock_);
    NotifyRead(bytes_to_read);
  }

  // We may have just read the last available data and thus changed the signals
  // state.
  watchers_.NotifyState(GetHandleSignalsStateNoLock());

  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::BeginReadData(
    const void** buffer,
    uint32_t* buffer_num_bytes) {
  base::AutoLock lock(lock_);
  if (!shared_ring_buffer_.IsValid() || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  const bool had_new_data = new_data_available_;
  new_data_available_ = false;

  if (bytes_available_ == 0) {
    if (had_new_data)
      watchers_.NotifyState(GetHandleSignalsStateNoLock());
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_SHOULD_WAIT;
  }

  DCHECK_LT(read_offset_, options_.capacity_num_bytes);
  uint32_t bytes_to_read =
      std::min(bytes_available_, options_.capacity_num_bytes - read_offset_);

  CHECK(ring_buffer_mapping_.IsValid());
  uint8_t* data = static_cast<uint8_t*>(ring_buffer_mapping_.memory());
  CHECK(data);

  in_two_phase_read_ = true;
  *buffer = data + read_offset_;
  *buffer_num_bytes = bytes_to_read;
  two_phase_max_bytes_read_ = bytes_to_read;

  if (had_new_data)
    watchers_.NotifyState(GetHandleSignalsStateNoLock());

  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::EndReadData(uint32_t num_bytes_read) {
  base::AutoLock lock(lock_);
  if (!in_two_phase_read_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  CHECK(shared_ring_buffer_.IsValid());

  MojoResult rv;
  if (num_bytes_read > two_phase_max_bytes_read_ ||
      num_bytes_read % options_.element_num_bytes != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
  } else {
    rv = MOJO_RESULT_OK;
    read_offset_ =
        (read_offset_ + num_bytes_read) % options_.capacity_num_bytes;

    DCHECK_GE(bytes_available_, num_bytes_read);
    bytes_available_ -= num_bytes_read;

    base::AutoUnlock unlock(lock_);
    NotifyRead(num_bytes_read);
  }

  in_two_phase_read_ = false;
  two_phase_max_bytes_read_ = 0;

  watchers_.NotifyState(GetHandleSignalsStateNoLock());

  return rv;
}

HandleSignalsState DataPipeConsumerDispatcher::GetHandleSignalsState() const {
  base::AutoLock lock(lock_);
  return GetHandleSignalsStateNoLock();
}

MojoResult DataPipeConsumerDispatcher::AddWatcherRef(
    const scoped_refptr<WatcherDispatcher>& watcher,
    uintptr_t context) {
  base::AutoLock lock(lock_);
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  return watchers_.Add(watcher, context, GetHandleSignalsStateNoLock());
}

MojoResult DataPipeConsumerDispatcher::RemoveWatcherRef(
    WatcherDispatcher* watcher,
    uintptr_t context) {
  base::AutoLock lock(lock_);
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  return watchers_.Remove(watcher, context);
}

void DataPipeConsumerDispatcher::StartSerialize(uint32_t* num_bytes,
                                                uint32_t* num_ports,
                                                uint32_t* num_handles) {
  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  *num_bytes = static_cast<uint32_t>(sizeof(SerializedState));
  *num_ports = 1;
  *num_handles = 1;
}

bool DataPipeConsumerDispatcher::EndSerialize(
    void* destination,
    ports::PortName* ports,
    PlatformHandle* platform_handles) {
  SerializedState* state = static_cast<SerializedState*>(destination);
  memcpy(&state->options, &options_, sizeof(MojoCreateDataPipeOptions));
  memset(state->padding, 0, sizeof(state->padding));

  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  state->pipe_id = pipe_id_;
  state->read_offset = read_offset_;
  state->bytes_available = bytes_available_;
  state->flags = peer_closed_ ? kFlagPeerClosed : 0;

  auto region_handle =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_ring_buffer_));
  const base::UnguessableToken& guid = region_handle.GetGUID();
  state->buffer_guid_high = guid.GetHighForSerialization();
  state->buffer_guid_low = guid.GetLowForSerialization();

  ports[0] = control_port_.name();

  PlatformHandle handle;
  PlatformHandle ignored_handle;
  ExtractPlatformHandlesFromSharedMemoryRegionHandle(
      region_handle.PassPlatformHandle(), &handle, &ignored_handle);
  if (!handle.is_valid() || ignored_handle.is_valid())
    return false;

  platform_handles[0] = std::move(handle);
  return true;
}

bool DataPipeConsumerDispatcher::BeginTransit() {
  base::AutoLock lock(lock_);
  if (in_transit_)
    return false;
  in_transit_ = !in_two_phase_read_;
  return in_transit_;
}

void DataPipeConsumerDispatcher::CompleteTransitAndClose() {
  node_controller_->SetPortObserver(control_port_, nullptr);

  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  in_transit_ = false;
  transferred_ = true;
  CloseNoLock();
}

void DataPipeConsumerDispatcher::CancelTransit() {
  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  in_transit_ = false;
  UpdateSignalsStateNoLock();
}

// static
scoped_refptr<DataPipeConsumerDispatcher>
DataPipeConsumerDispatcher::Deserialize(const void* data,
                                        size_t num_bytes,
                                        const ports::PortName* ports,
                                        size_t num_ports,
                                        PlatformHandle* handles,
                                        size_t num_handles) {
  if (num_ports != 1 || num_handles != 1 ||
      num_bytes != sizeof(SerializedState)) {
    return nullptr;
  }

  const SerializedState* state = static_cast<const SerializedState*>(data);
  if (!state->options.capacity_num_bytes || !state->options.element_num_bytes ||
      state->options.capacity_num_bytes < state->options.element_num_bytes ||
      state->read_offset >= state->options.capacity_num_bytes ||
      state->bytes_available > state->options.capacity_num_bytes) {
    return nullptr;
  }

  NodeController* node_controller = Core::Get()->GetNodeController();
  ports::PortRef port;
  if (node_controller->node()->GetPort(ports[0], &port) != ports::OK)
    return nullptr;

  auto region_handle = CreateSharedMemoryRegionHandleFromPlatformHandles(
      std::move(handles[0]), PlatformHandle());
  auto region = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(region_handle),
      base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
      state->options.capacity_num_bytes,
      base::UnguessableToken::Deserialize(state->buffer_guid_high,
                                          state->buffer_guid_low));
  auto ring_buffer =
      base::UnsafeSharedMemoryRegion::Deserialize(std::move(region));
  if (!ring_buffer.IsValid()) {
    DLOG(ERROR) << "Failed to deserialize shared buffer handle.";
    return nullptr;
  }

  scoped_refptr<DataPipeConsumerDispatcher> dispatcher =
      new DataPipeConsumerDispatcher(node_controller, port,
                                     std::move(ring_buffer), state->options,
                                     state->pipe_id);

  {
    base::AutoLock lock(dispatcher->lock_);
    dispatcher->read_offset_ = state->read_offset;
    dispatcher->bytes_available_ = state->bytes_available;
    dispatcher->new_data_available_ = state->bytes_available > 0;
    dispatcher->peer_closed_ = state->flags & kFlagPeerClosed;
    if (!dispatcher->InitializeNoLock())
      return nullptr;
    if (state->options.capacity_num_bytes >
        dispatcher->ring_buffer_mapping_.mapped_size()) {
      return nullptr;
    }
    dispatcher->UpdateSignalsStateNoLock();
  }

  return dispatcher;
}

DataPipeConsumerDispatcher::DataPipeConsumerDispatcher(
    NodeController* node_controller,
    const ports::PortRef& control_port,
    base::UnsafeSharedMemoryRegion shared_ring_buffer,
    const MojoCreateDataPipeOptions& options,
    uint64_t pipe_id)
    : options_(options),
      node_controller_(node_controller),
      control_port_(control_port),
      pipe_id_(pipe_id),
      watchers_(this),
      shared_ring_buffer_(std::move(shared_ring_buffer)) {}

DataPipeConsumerDispatcher::~DataPipeConsumerDispatcher() {
  DCHECK(is_closed_ && !shared_ring_buffer_.IsValid() &&
         !ring_buffer_mapping_.IsValid() && !in_transit_);
}

bool DataPipeConsumerDispatcher::InitializeNoLock() {
  lock_.AssertAcquired();
  if (!shared_ring_buffer_.IsValid())
    return false;

  DCHECK(!ring_buffer_mapping_.IsValid());
  ring_buffer_mapping_ = shared_ring_buffer_.Map();
  if (!ring_buffer_mapping_.IsValid()) {
    DLOG(ERROR) << "Failed to map shared buffer.";
    shared_ring_buffer_ = base::UnsafeSharedMemoryRegion();
    return false;
  }

  base::AutoUnlock unlock(lock_);
  node_controller_->SetPortObserver(
      control_port_, base::MakeRefCounted<PortObserverThunk>(this));

  return true;
}

MojoResult DataPipeConsumerDispatcher::CloseNoLock() {
  lock_.AssertAcquired();
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  is_closed_ = true;
  ring_buffer_mapping_ = base::WritableSharedMemoryMapping();
  shared_ring_buffer_ = base::UnsafeSharedMemoryRegion();

  watchers_.NotifyClosed();
  if (!transferred_) {
    base::AutoUnlock unlock(lock_);
    node_controller_->ClosePort(control_port_);
  }

  return MOJO_RESULT_OK;
}

HandleSignalsState DataPipeConsumerDispatcher::GetHandleSignalsStateNoLock()
    const {
  lock_.AssertAcquired();

  HandleSignalsState rv;
  if (shared_ring_buffer_.IsValid() && bytes_available_) {
    if (!in_two_phase_read_) {
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
      if (new_data_available_)
        rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
    }
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  } else if (!peer_closed_ && shared_ring_buffer_.IsValid()) {
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }

  if (shared_ring_buffer_.IsValid()) {
    if (new_data_available_ || !peer_closed_)
      rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
  }

  if (peer_closed_) {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  } else {
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_REMOTE;
    if (peer_remote_)
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_REMOTE;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;

  return rv;
}

void DataPipeConsumerDispatcher::NotifyRead(uint32_t num_bytes) {
  DVLOG(1) << "Data pipe consumer " << pipe_id_
           << " notifying peer: " << num_bytes
           << " bytes read. [control_port=" << control_port_.name() << "]";

  SendDataPipeControlMessage(node_controller_, control_port_,
                             DataPipeCommand::DATA_WAS_READ, num_bytes);
}

void DataPipeConsumerDispatcher::OnPortStatusChanged() {
  DCHECK(RequestContext::current());

  base::AutoLock lock(lock_);

  // We stop observing the control port as soon it's transferred, but this can
  // race with events which are raised right before that happens. This is fine
  // to ignore.
  if (transferred_)
    return;

  DVLOG(1) << "Control port status changed for data pipe producer " << pipe_id_;

  UpdateSignalsStateNoLock();
}

void DataPipeConsumerDispatcher::UpdateSignalsStateNoLock() {
  lock_.AssertAcquired();

  const bool was_peer_closed = peer_closed_;
  const bool was_peer_remote = peer_remote_;
  size_t previous_bytes_available = bytes_available_;

  ports::PortStatus port_status;
  int rv = node_controller_->node()->GetStatus(control_port_, &port_status);
  peer_remote_ = rv == ports::OK && port_status.peer_remote;
  if (rv != ports::OK || !port_status.receiving_messages) {
    DVLOG(1) << "Data pipe consumer " << pipe_id_ << " is aware of peer closure"
             << " [control_port=" << control_port_.name() << "]";
    peer_closed_ = true;
  } else if (rv == ports::OK && port_status.has_messages && !in_transit_) {
    std::unique_ptr<ports::UserMessageEvent> message_event;
    do {
      rv = node_controller_->node()->GetMessage(control_port_, &message_event,
                                                nullptr);
      if (rv != ports::OK)
        peer_closed_ = true;
      if (message_event) {
        auto* message = message_event->GetMessage<UserMessageImpl>();
        if (message->user_payload_size() < sizeof(DataPipeControlMessage)) {
          peer_closed_ = true;
          break;
        }

        const DataPipeControlMessage* m =
            static_cast<const DataPipeControlMessage*>(message->user_payload());

        if (m->command != DataPipeCommand::DATA_WAS_WRITTEN) {
          DLOG(ERROR) << "Unexpected control message from producer.";
          peer_closed_ = true;
          break;
        }

        if (static_cast<size_t>(bytes_available_) + m->num_bytes >
            options_.capacity_num_bytes) {
          DLOG(ERROR) << "Producer claims to have written too many bytes.";
          peer_closed_ = true;
          break;
        }

        DVLOG(1) << "Data pipe consumer " << pipe_id_ << " is aware that "
                 << m->num_bytes << " bytes were written. [control_port="
                 << control_port_.name() << "]";

        bytes_available_ += m->num_bytes;
      }
    } while (message_event);
  }

  bool has_new_data = bytes_available_ != previous_bytes_available;
  if (has_new_data)
    new_data_available_ = true;

  if (peer_closed_ != was_peer_closed || has_new_data ||
      peer_remote_ != was_peer_remote) {
    watchers_.NotifyState(GetHandleSignalsStateNoLock());
  }
}

}  // namespace core
}  // namespace mojo
