// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/data_pipe_producer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "mojo/core/configuration.h"
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
  uint32_t write_offset;
  uint32_t available_capacity;
  uint8_t flags;
  uint64_t buffer_guid_high;
  uint64_t buffer_guid_low;
  char padding[7];
};

static_assert(sizeof(SerializedState) % 8 == 0,
              "Invalid SerializedState size.");

#pragma pack(pop)

}  // namespace

// A PortObserver which forwards to a DataPipeProducerDispatcher. This owns a
// reference to the dispatcher to ensure it lives as long as the observed port.
class DataPipeProducerDispatcher::PortObserverThunk
    : public NodeController::PortObserver {
 public:
  explicit PortObserverThunk(
      scoped_refptr<DataPipeProducerDispatcher> dispatcher)
      : dispatcher_(dispatcher) {}

 private:
  ~PortObserverThunk() override {}

  // NodeController::PortObserver:
  void OnPortStatusChanged() override { dispatcher_->OnPortStatusChanged(); }

  scoped_refptr<DataPipeProducerDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PortObserverThunk);
};

// static
scoped_refptr<DataPipeProducerDispatcher> DataPipeProducerDispatcher::Create(
    NodeController* node_controller,
    const ports::PortRef& control_port,
    base::UnsafeSharedMemoryRegion shared_ring_buffer,
    const MojoCreateDataPipeOptions& options,
    uint64_t pipe_id) {
  scoped_refptr<DataPipeProducerDispatcher> producer =
      new DataPipeProducerDispatcher(node_controller, control_port,
                                     std::move(shared_ring_buffer), options,
                                     pipe_id);
  base::AutoLock lock(producer->lock_);
  if (!producer->InitializeNoLock())
    return nullptr;
  return producer;
}

Dispatcher::Type DataPipeProducerDispatcher::GetType() const {
  return Type::DATA_PIPE_PRODUCER;
}

MojoResult DataPipeProducerDispatcher::Close() {
  base::AutoLock lock(lock_);
  DVLOG(1) << "Closing data pipe producer " << pipe_id_;
  return CloseNoLock();
}

MojoResult DataPipeProducerDispatcher::WriteData(
    const void* elements,
    uint32_t* num_bytes,
    const MojoWriteDataOptions& options) {
  base::AutoLock lock(lock_);
  if (!shared_ring_buffer_.IsValid() || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  if (peer_closed_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (*num_bytes % options_.element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  if ((options.flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE) &&
      (*num_bytes > available_capacity_)) {
    // Don't return "should wait" since you can't wait for a specified amount of
    // data.
    return MOJO_RESULT_OUT_OF_RANGE;
  }

  DCHECK_LE(available_capacity_, options_.capacity_num_bytes);
  uint32_t num_bytes_to_write = std::min(*num_bytes, available_capacity_);
  if (num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  *num_bytes = num_bytes_to_write;

  CHECK(ring_buffer_mapping_.IsValid());
  uint8_t* data = static_cast<uint8_t*>(ring_buffer_mapping_.memory());
  CHECK(data);

  const uint8_t* source = static_cast<const uint8_t*>(elements);
  CHECK(source);

  DCHECK_LE(write_offset_, options_.capacity_num_bytes);
  uint32_t tail_bytes_to_write =
      std::min(options_.capacity_num_bytes - write_offset_, num_bytes_to_write);
  uint32_t head_bytes_to_write = num_bytes_to_write - tail_bytes_to_write;

  DCHECK_GT(tail_bytes_to_write, 0u);
  memcpy(data + write_offset_, source, tail_bytes_to_write);
  if (head_bytes_to_write > 0)
    memcpy(data, source + tail_bytes_to_write, head_bytes_to_write);

  DCHECK_LE(num_bytes_to_write, available_capacity_);
  available_capacity_ -= num_bytes_to_write;
  write_offset_ =
      (write_offset_ + num_bytes_to_write) % options_.capacity_num_bytes;

  watchers_.NotifyState(GetHandleSignalsStateNoLock());

  base::AutoUnlock unlock(lock_);
  NotifyWrite(num_bytes_to_write);

  return MOJO_RESULT_OK;
}

MojoResult DataPipeProducerDispatcher::BeginWriteData(
    void** buffer,
    uint32_t* buffer_num_bytes) {
  base::AutoLock lock(lock_);
  if (!shared_ring_buffer_.IsValid() || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (in_two_phase_write_)
    return MOJO_RESULT_BUSY;
  if (peer_closed_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (available_capacity_ == 0) {
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_SHOULD_WAIT;
  }

  in_two_phase_write_ = true;
  *buffer_num_bytes = std::min(options_.capacity_num_bytes - write_offset_,
                               available_capacity_);
  DCHECK_GT(*buffer_num_bytes, 0u);

  CHECK(ring_buffer_mapping_.IsValid());
  uint8_t* data = static_cast<uint8_t*>(ring_buffer_mapping_.memory());
  *buffer = data + write_offset_;

  return MOJO_RESULT_OK;
}

MojoResult DataPipeProducerDispatcher::EndWriteData(
    uint32_t num_bytes_written) {
  base::AutoLock lock(lock_);
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!in_two_phase_write_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  // Note: Allow successful completion of the two-phase write even if the other
  // side has been closed.
  MojoResult rv = MOJO_RESULT_OK;
  if (num_bytes_written > available_capacity_ ||
      num_bytes_written % options_.element_num_bytes != 0 ||
      write_offset_ + num_bytes_written > options_.capacity_num_bytes) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
  } else {
    DCHECK_LE(num_bytes_written + write_offset_, options_.capacity_num_bytes);
    available_capacity_ -= num_bytes_written;
    write_offset_ =
        (write_offset_ + num_bytes_written) % options_.capacity_num_bytes;

    base::AutoUnlock unlock(lock_);
    NotifyWrite(num_bytes_written);
  }

  in_two_phase_write_ = false;

  // If we're now writable, we *became* writable (since we weren't writable
  // during the two-phase write), so notify watchers.
  watchers_.NotifyState(GetHandleSignalsStateNoLock());

  return rv;
}

HandleSignalsState DataPipeProducerDispatcher::GetHandleSignalsState() const {
  base::AutoLock lock(lock_);
  return GetHandleSignalsStateNoLock();
}

MojoResult DataPipeProducerDispatcher::AddWatcherRef(
    const scoped_refptr<WatcherDispatcher>& watcher,
    uintptr_t context) {
  base::AutoLock lock(lock_);
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  return watchers_.Add(watcher, context, GetHandleSignalsStateNoLock());
}

MojoResult DataPipeProducerDispatcher::RemoveWatcherRef(
    WatcherDispatcher* watcher,
    uintptr_t context) {
  base::AutoLock lock(lock_);
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  return watchers_.Remove(watcher, context);
}

void DataPipeProducerDispatcher::StartSerialize(uint32_t* num_bytes,
                                                uint32_t* num_ports,
                                                uint32_t* num_handles) {
  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  *num_bytes = sizeof(SerializedState);
  *num_ports = 1;
  *num_handles = 1;
}

bool DataPipeProducerDispatcher::EndSerialize(
    void* destination,
    ports::PortName* ports,
    PlatformHandle* platform_handles) {
  SerializedState* state = static_cast<SerializedState*>(destination);
  memcpy(&state->options, &options_, sizeof(MojoCreateDataPipeOptions));
  memset(state->padding, 0, sizeof(state->padding));

  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  state->pipe_id = pipe_id_;
  state->write_offset = write_offset_;
  state->available_capacity = available_capacity_;
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

bool DataPipeProducerDispatcher::BeginTransit() {
  base::AutoLock lock(lock_);
  if (in_transit_)
    return false;
  in_transit_ = !in_two_phase_write_;
  return in_transit_;
}

void DataPipeProducerDispatcher::CompleteTransitAndClose() {
  node_controller_->SetPortObserver(control_port_, nullptr);

  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  transferred_ = true;
  in_transit_ = false;
  CloseNoLock();
}

void DataPipeProducerDispatcher::CancelTransit() {
  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  in_transit_ = false;

  HandleSignalsState state = GetHandleSignalsStateNoLock();
  watchers_.NotifyState(state);
}

// static
scoped_refptr<DataPipeProducerDispatcher>
DataPipeProducerDispatcher::Deserialize(const void* data,
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
      state->write_offset >= state->options.capacity_num_bytes ||
      state->available_capacity > state->options.capacity_num_bytes) {
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

  scoped_refptr<DataPipeProducerDispatcher> dispatcher =
      new DataPipeProducerDispatcher(node_controller, port,
                                     std::move(ring_buffer), state->options,
                                     state->pipe_id);

  {
    base::AutoLock lock(dispatcher->lock_);
    dispatcher->write_offset_ = state->write_offset;
    dispatcher->available_capacity_ = state->available_capacity;
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

DataPipeProducerDispatcher::DataPipeProducerDispatcher(
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
      shared_ring_buffer_(std::move(shared_ring_buffer)),
      available_capacity_(options_.capacity_num_bytes) {}

DataPipeProducerDispatcher::~DataPipeProducerDispatcher() {
  DCHECK(is_closed_ && !in_transit_ && !shared_ring_buffer_.IsValid() &&
         !ring_buffer_mapping_.IsValid());
}

bool DataPipeProducerDispatcher::InitializeNoLock() {
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

MojoResult DataPipeProducerDispatcher::CloseNoLock() {
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

HandleSignalsState DataPipeProducerDispatcher::GetHandleSignalsStateNoLock()
    const {
  lock_.AssertAcquired();
  HandleSignalsState rv;
  if (!peer_closed_) {
    if (!in_two_phase_write_ && shared_ring_buffer_.IsValid() &&
        available_capacity_ > 0)
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    if (peer_remote_)
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_REMOTE;
    rv.satisfiable_signals |=
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_REMOTE;
  } else {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

void DataPipeProducerDispatcher::NotifyWrite(uint32_t num_bytes) {
  DVLOG(1) << "Data pipe producer " << pipe_id_
           << " notifying peer: " << num_bytes
           << " bytes written. [control_port=" << control_port_.name() << "]";

  SendDataPipeControlMessage(node_controller_, control_port_,
                             DataPipeCommand::DATA_WAS_WRITTEN, num_bytes);
}

void DataPipeProducerDispatcher::OnPortStatusChanged() {
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

void DataPipeProducerDispatcher::UpdateSignalsStateNoLock() {
  lock_.AssertAcquired();

  const bool was_peer_closed = peer_closed_;
  const bool was_peer_remote = peer_remote_;
  size_t previous_capacity = available_capacity_;

  ports::PortStatus port_status;
  int rv = node_controller_->node()->GetStatus(control_port_, &port_status);
  peer_remote_ = rv == ports::OK && port_status.peer_remote;
  if (rv != ports::OK || !port_status.receiving_messages) {
    DVLOG(1) << "Data pipe producer " << pipe_id_ << " is aware of peer closure"
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

        if (m->command != DataPipeCommand::DATA_WAS_READ) {
          DLOG(ERROR) << "Unexpected message from consumer.";
          peer_closed_ = true;
          break;
        }

        if (static_cast<size_t>(available_capacity_) + m->num_bytes >
            options_.capacity_num_bytes) {
          DLOG(ERROR) << "Consumer claims to have read too many bytes.";
          break;
        }

        DVLOG(1) << "Data pipe producer " << pipe_id_ << " is aware that "
                 << m->num_bytes
                 << " bytes were read. [control_port=" << control_port_.name()
                 << "]";

        available_capacity_ += m->num_bytes;
      }
    } while (message_event);
  }

  if (peer_closed_ != was_peer_closed ||
      available_capacity_ != previous_capacity ||
      was_peer_remote != peer_remote_) {
    watchers_.NotifyState(GetHandleSignalsStateNoLock());
  }
}

}  // namespace core
}  // namespace mojo
