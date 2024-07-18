// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/data_pipe.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <tuple>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_math.h"
#include "base/synchronization/lock.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/ring_buffer.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// The wire representation of a serialized DataPipe endpoint.
struct IPCZ_ALIGN(8) DataPipeHeader {
  // The size of this structure, in bytes. Used for versioning.
  uint32_t size;

  DataPipe::EndpointType endpoint_type;

  // The size in bytes of an element of this pipe.
  uint32_t element_size;

  // Padding for alignment to an 8-byte boundary.
  uint32_t padding;

  RingBuffer::SerializedState ring_buffer_state;
};
static_assert(sizeof(DataPipeHeader) == 24, "Invalid DataPipeHeader size");

// Attempts to put a single (32-bit) integer into the given `portal`. Returns
// true if successful, or false to indicate that the peer portal is closed.
bool SendPeerUpdate(IpczHandle portal, size_t num_bytes) {
  if (num_bytes == 0) {
    // Do not send messages for empty reads or writes. This ensures that
    // endpoints can reliably infer new (non-zero) data or capacity by the mere
    // presence of one or more unread parcels.
    return true;
  }

  const uint32_t num_bytes_checked = base::checked_cast<uint32_t>(num_bytes);
  const IpczResult result =
      GetIpczAPI().Put(portal, &num_bytes_checked, sizeof(num_bytes_checked),
                       nullptr, 0, IPCZ_NO_FLAGS, nullptr);
  return result == IPCZ_RESULT_OK;
}

// Drains control messages from a DataPipe's portal. Each control message is a
// 32-bit unsigned integer conveying the size of a transaction. This accumulates
// the sum of any received sizes and returns it, along with a bit indicating
// whether the control portal is dead.
struct DrainResult {
  size_t num_bytes_changed;
  bool dead;
};
DrainResult DrainPeerUpdates(IpczHandle portal) {
  size_t num_bytes_changed = 0;
  for (;;) {
    uint32_t value;
    size_t num_bytes = sizeof(value);
    const IpczResult result =
        GetIpczAPI().Get(portal, IPCZ_GET_PARTIAL, nullptr, &value, &num_bytes,
                         nullptr, nullptr, nullptr);
    switch (result) {
      case IPCZ_RESULT_UNAVAILABLE:
        // No more parcels and peer is still alive.
        return {.num_bytes_changed = num_bytes_changed, .dead = false};

      case IPCZ_RESULT_NOT_FOUND:
        // Peer is gone.
        return {.num_bytes_changed = num_bytes_changed, .dead = true};

      case IPCZ_RESULT_OK: {
        if (num_bytes < sizeof(value)) {
          // Missing data. Treat as if closed.
          return {.num_bytes_changed = num_bytes_changed, .dead = true};
        }

        if (!base::CheckAdd(num_bytes_changed, value)
                 .AssignIfValid(&num_bytes_changed)) {
          // Stop accumulating on overflow to avoid losing information. This is
          // not an error condition and subsequent operations may continue to
          // drain control messages.
          return {.num_bytes_changed = num_bytes_changed, .dead = false};
        }

        continue;
      }

      case IPCZ_RESULT_ALREADY_EXISTS:
        // Unlikely: we raced with a flush on another thread. Try again.
        continue;

      default:
        // Unexpected behavior. Treat as if closed.
        return {.num_bytes_changed = 0, .dead = true};
    }
  }
}

}  // namespace

DataPipe::PortalWrapper::PortalWrapper(ScopedIpczHandle handle)
    : handle_(std::move(handle)) {}

DataPipe::PortalWrapper::~PortalWrapper() = default;

DataPipe::DataPipe(EndpointType endpoint_type,
                   const Config& config,
                   scoped_refptr<SharedBuffer> buffer,
                   scoped_refptr<SharedBufferMapping> mapping)
    : endpoint_type_(endpoint_type),
      element_size_(config.element_size),
      buffer_(std::move(buffer)),
      data_(std::move(mapping)) {
  DCHECK_GT(element_size_, 0u);
  DCHECK_LE(element_size_, std::numeric_limits<uint32_t>::max());
  DCHECK_GT(config.byte_capacity, 0u);
  DCHECK_LE(config.byte_capacity, std::numeric_limits<uint32_t>::max());
  DCHECK_EQ(config.byte_capacity, buffer_->region().GetSize());
  DCHECK_EQ(config.byte_capacity, data_.capacity());
}

DataPipe::~DataPipe() {
  Close();
}

// static
std::optional<DataPipe::Pair> DataPipe::CreatePair(const Config& config) {
  ScopedIpczHandle producer;
  ScopedIpczHandle consumer;
  const IpczResult result =
      GetIpczAPI().OpenPortals(GetIpczNode(), IPCZ_NO_FLAGS, nullptr,
                               ScopedIpczHandle::Receiver(producer),
                               ScopedIpczHandle::Receiver(consumer));
  DCHECK_EQ(result, IPCZ_RESULT_OK);

  base::UnsafeSharedMemoryRegion consumer_region =
      base::UnsafeSharedMemoryRegion::Create(config.byte_capacity);
  if (!consumer_region.IsValid()) {
    return std::nullopt;
  }

  base::UnsafeSharedMemoryRegion producer_region = consumer_region.Duplicate();
  if (!producer_region.IsValid()) {
    return std::nullopt;
  }

  scoped_refptr<SharedBuffer> consumer_buffer =
      SharedBuffer::MakeForRegion(std::move(consumer_region));
  scoped_refptr<SharedBuffer> producer_buffer =
      SharedBuffer::MakeForRegion(std::move(producer_region));
  auto consumer_mapping =
      SharedBufferMapping::Create(consumer_buffer->region());
  auto producer_mapping =
      SharedBufferMapping::Create(producer_buffer->region());
  if (!consumer_mapping || !producer_mapping) {
    return std::nullopt;
  }

  Pair pair;
  pair.consumer = base::MakeRefCounted<DataPipe>(
      EndpointType::kConsumer, config, std::move(consumer_buffer),
      std::move(consumer_mapping));
  pair.consumer->AdoptPortal(std::move(consumer));

  pair.producer = base::MakeRefCounted<DataPipe>(
      EndpointType::kProducer, config, std::move(producer_buffer),
      std::move(producer_mapping));
  pair.producer->AdoptPortal(std::move(producer));
  return pair;
}

bool DataPipe::AdoptPortal(ScopedIpczHandle portal) {
  if (!portal.is_valid()) {
    return false;
  }

  IpczPortalStatus status = {.size = sizeof(status)};
  if (GetIpczAPI().QueryPortalStatus(portal.get(), IPCZ_NO_FLAGS, nullptr,
                                     &status) != IPCZ_RESULT_OK) {
    return false;
  }

  base::AutoLock lock(lock_);
  DCHECK(!portal_);
  portal_ = base::MakeRefCounted<PortalWrapper>(std::move(portal));
  is_peer_closed_ = (status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) != 0;
  return true;
}

scoped_refptr<DataPipe::PortalWrapper> DataPipe::GetPortal() {
  base::AutoLock lock(lock_);
  return portal_;
}

ScopedIpczHandle DataPipe::TakePortal() {
  scoped_refptr<PortalWrapper> portal;

  base::AutoLock lock(lock_);
  portal.swap(portal_);
  return portal->TakeHandle();
}

MojoResult DataPipe::WriteData(const void* elements,
                               uint32_t& num_bytes,
                               MojoWriteDataFlags flags) {
  if (num_bytes % element_size_ != 0) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  FlushUpdatesFromPeer();
  const base::span<const uint8_t> input_bytes =
      base::make_span(static_cast<const uint8_t*>(elements), num_bytes);
  scoped_refptr<PortalWrapper> portal;
  size_t write_size;
  {
    base::AutoLock lock(lock_);
    if (!portal_) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    if (two_phase_writer_) {
      return MOJO_RESULT_BUSY;
    }
    if (is_peer_closed_) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    if (flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE) {
      if (!data_.WriteAll(input_bytes)) {
        return input_bytes.empty() ? MOJO_RESULT_SHOULD_WAIT
                                   : MOJO_RESULT_OUT_OF_RANGE;
      }
      write_size = input_bytes.size();
    } else {
      write_size = data_.Write(input_bytes);
      if (write_size == 0) {
        return input_bytes.empty() ? MOJO_RESULT_OK : MOJO_RESULT_SHOULD_WAIT;
      }
    }

    portal = portal_;
  }

  num_bytes = base::checked_cast<uint32_t>(write_size);
  if (!SendPeerUpdate(portal->handle(), write_size)) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::BeginWriteData(void*& data,
                                    uint32_t& num_bytes,
                                    MojoBeginWriteDataFlags flags) {
  FlushUpdatesFromPeer();
  base::AutoLock lock(lock_);
  if (two_phase_writer_) {
    return MOJO_RESULT_BUSY;
  }
  if (is_peer_closed_) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  RingBuffer::DirectWriter writer(data_);
  if (writer.bytes().empty()) {
    return MOJO_RESULT_SHOULD_WAIT;
  }

  two_phase_writer_.emplace(std::move(writer));
  data = two_phase_writer_->bytes().data();
  num_bytes = base::checked_cast<uint32_t>(two_phase_writer_->bytes().size());
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::EndWriteData(size_t num_bytes_produced) {
  scoped_refptr<PortalWrapper> portal;
  {
    base::AutoLock lock(lock_);
    if (!two_phase_writer_) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    RingBuffer::DirectWriter writer(std::move(*two_phase_writer_));
    two_phase_writer_.reset();
    if (num_bytes_produced % element_size_ != 0) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    if (!portal_) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    if (num_bytes_produced == 0) {
      return MOJO_RESULT_OK;
    }

    if (!std::move(writer).Commit(num_bytes_produced)) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    portal = portal_;
  }

  SendPeerUpdate(portal->handle(), num_bytes_produced);
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ReadData(void* elements,
                              uint32_t& num_bytes,
                              MojoReadDataFlags flags) {
  const bool query = flags & MOJO_READ_DATA_FLAG_QUERY;
  const bool peek = flags & MOJO_READ_DATA_FLAG_PEEK;
  const bool discard = flags & MOJO_READ_DATA_FLAG_DISCARD;
  const bool allow_partial = !(flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE);

  // Filter for assorted configurations that aren't used in practice and which
  // therefore do not require support here.
  if ((peek && discard) || (query && (peek || discard))) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  FlushUpdatesFromPeer();
  base::span<uint8_t> output_bytes;
  if (!(discard || query)) {
    // `elements` can be null in the `discard` or `query` cases and would result
    // in creating a non-empty, dangling `span` (hitting a `DCHECK` in `span`'s
    // constructor).  OTOH, `elements` and `num_bytes` need to describe a valid
    // span in all the other cases.
    if (!elements && num_bytes > 0) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    output_bytes = base::span(static_cast<uint8_t*>(elements), num_bytes);
  }

  size_t read_size = num_bytes;
  scoped_refptr<PortalWrapper> portal;
  {
    base::AutoLock lock(lock_);
    if (two_phase_reader_) {
      return MOJO_RESULT_BUSY;
    }

    const size_t data_size = data_.data_size();
    if (query) {
      num_bytes = base::checked_cast<uint32_t>(data_size);
      return MOJO_RESULT_OK;
    }

    if (num_bytes % element_size_ != 0 || !portal_) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    has_new_data_ = false;
    if (!allow_partial) {
      bool success;
      if (discard) {
        success = data_.DiscardAll(num_bytes);
      } else if (peek) {
        success = data_.PeekAll(output_bytes);
      } else {
        success = data_.ReadAll(output_bytes);
      }
      if (!success) {
        return is_peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                               : MOJO_RESULT_OUT_OF_RANGE;
      }
      read_size = output_bytes.size();
    } else {
      if (data_size == 0) {
        return is_peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                               : MOJO_RESULT_SHOULD_WAIT;
      }
      if (discard) {
        read_size = std::min(read_size, data_size);
        data_.Discard(read_size);
      } else if (peek) {
        read_size = data_.Peek(output_bytes);
      } else {
        read_size = data_.Read(output_bytes);
      }
      num_bytes = base::checked_cast<uint32_t>(read_size);
    }

    if (peek || read_size == 0) {
      return MOJO_RESULT_OK;
    }

    portal = portal_;
  }

  SendPeerUpdate(portal->handle(), read_size);
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::BeginReadData(const void*& buffer,
                                   uint32_t& buffer_num_bytes) {
  FlushUpdatesFromPeer();
  base::AutoLock lock(lock_);
  if (two_phase_reader_) {
    return MOJO_RESULT_BUSY;
  }

  if (!portal_) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  RingBuffer::DirectReader reader(data_);
  if (reader.bytes().empty()) {
    return is_peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                           : MOJO_RESULT_SHOULD_WAIT;
  }

  two_phase_reader_.emplace(std::move(reader));
  buffer = two_phase_reader_->bytes().data();
  buffer_num_bytes =
      base::checked_cast<uint32_t>(two_phase_reader_->bytes().size());
  has_new_data_ = false;
  return IPCZ_RESULT_OK;
}

MojoResult DataPipe::EndReadData(size_t num_bytes_consumed) {
  scoped_refptr<PortalWrapper> portal;
  {
    base::AutoLock lock(lock_);
    if (!two_phase_reader_) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    RingBuffer::DirectReader reader(std::move(*two_phase_reader_));
    two_phase_reader_.reset();
    if (num_bytes_consumed % element_size_ != 0) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    if (num_bytes_consumed == 0) {
      return MOJO_RESULT_OK;
    }

    if (!std::move(reader).Consume(num_bytes_consumed)) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    portal = portal_;
  }

  SendPeerUpdate(portal->handle(), num_bytes_consumed);
  return MOJO_RESULT_OK;
}

void DataPipe::Close() {
  // Drop our reference to the wrapper. The portal will be closed as soon as
  // this stack unwinds and, if applicable, after any other threads or done
  // using it.
  scoped_refptr<PortalWrapper> portal;
  base::AutoLock lock(lock_);
  portal_.swap(portal);
}

bool DataPipe::IsSerializable() const {
  base::AutoLock lock(lock_);
  return !in_transit_;
}

bool DataPipe::GetSerializedDimensions(Transport& transmitter,
                                       size_t& num_bytes,
                                       size_t& num_handles) {
  base::AutoLock lock(lock_);
  if (!buffer_->GetSerializedDimensions(transmitter, num_bytes, num_handles)) {
    return false;
  }
  num_bytes = base::CheckAdd(num_bytes, sizeof(DataPipeHeader)).ValueOrDie();
  return true;
}

bool DataPipe::Serialize(Transport& transmitter,
                         base::span<uint8_t> data,
                         base::span<PlatformHandle> handles) {
  base::AutoLock lock(lock_);

  // NOTE: Drivers cannot serialize their objects to other ipcz objects (such as
  // portals) through the driver API. Instead, mojo-ipcz serializes and
  // deserializes a DataPipe's portal within WriteMessage() and ReadMessage()
  // in core_ipcz.cc. Here we only serialize the header and the backing
  // SharedBuffer object.
  DCHECK_GE(data.size(), sizeof(DataPipeHeader));
  auto& header = *reinterpret_cast<DataPipeHeader*>(data.data());
  memset(&header, 0, sizeof(header));
  header.size = sizeof(header);
  header.endpoint_type = endpoint_type_;
  header.element_size = base::checked_cast<uint32_t>(element_size_);
  data_.Serialize(header.ring_buffer_state);

  auto buffer_data = data.subspan(sizeof(DataPipeHeader));
  if (!buffer_->Serialize(transmitter, buffer_data, handles)) {
    return false;
  }

  buffer_ = nullptr;
  in_transit_ = true;
  return true;
}

// static
scoped_refptr<DataPipe> DataPipe::Deserialize(
    base::span<const uint8_t> data,
    base::span<PlatformHandle> handles) {
  if (data.size() < sizeof(DataPipeHeader)) {
    return nullptr;
  }

  const auto& header = *reinterpret_cast<const DataPipeHeader*>(data.data());
  const size_t header_size = header.size;
  if (header_size < sizeof(header) || header_size % 8 != 0) {
    return nullptr;
  }

  scoped_refptr<SharedBuffer> buffer =
      SharedBuffer::Deserialize(data.subspan(header_size), handles);
  if (!buffer) {
    return nullptr;
  }

  const size_t buffer_size = buffer->region().GetSize();
  const size_t element_size = header.element_size;
  if (element_size == 0 || buffer_size % element_size != 0) {
    return nullptr;
  }

  scoped_refptr<SharedBufferMapping> mapping =
      SharedBufferMapping::Create(buffer->region());
  if (!mapping) {
    return nullptr;
  }

  auto endpoint = base::MakeRefCounted<DataPipe>(
      header.endpoint_type,
      Config{.element_size = element_size, .byte_capacity = buffer_size},
      std::move(buffer), std::move(mapping));
  if (!endpoint->DeserializeRingBuffer(header.ring_buffer_state)) {
    return nullptr;
  }
  return endpoint;
}

bool DataPipe::GetSignals(MojoHandleSignalsState& signals_state) {
  signals_state = {};
  MojoHandleSignals& satisfied = signals_state.satisfied_signals;
  MojoHandleSignals& satisfiable = signals_state.satisfiable_signals;

  base::AutoLock lock(lock_);
  if (!portal_) {
    return false;
  }

  IpczPortalStatus status = {.size = sizeof(status)};
  const IpczResult result = GetIpczAPI().QueryPortalStatus(
      portal_->handle(), IPCZ_NO_FLAGS, nullptr, &status);
  if (result != IPCZ_RESULT_OK) {
    return false;
  }

  if ((status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) != 0) {
    is_peer_closed_ = true;
  }

  satisfiable = MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  if (is_peer_closed_) {
    satisfied |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  } else {
    satisfiable |= MOJO_HANDLE_SIGNAL_PEER_REMOTE;
  }

  if (is_consumer()) {
    const bool new_data_available =
        has_new_data_ || status.num_local_parcels > 0;
    if (new_data_available) {
      has_new_data_ = true;
      satisfied |= MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
      satisfiable |= MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
    }

    const bool any_data_available = new_data_available || data_.data_size() > 0;
    if (any_data_available) {
      satisfiable |= MOJO_HANDLE_SIGNAL_READABLE;
      satisfied |= MOJO_HANDLE_SIGNAL_READABLE;
    }

    if (!is_peer_closed_) {
      satisfiable |=
          MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
    }

    return true;
  }

  DCHECK(is_producer());
  if (!is_peer_closed_) {
    satisfiable |= MOJO_HANDLE_SIGNAL_WRITABLE;
    if (data_.available_capacity() > 0 || status.num_local_parcels > 0) {
      satisfied |= MOJO_HANDLE_SIGNAL_WRITABLE;
    }
  }

  return true;
}

void DataPipe::FlushUpdatesFromPeer() {
  scoped_refptr<PortalWrapper> portal;
  {
    base::AutoLock lock(lock_);
    if (!portal_ || in_transit_) {
      // Once an endpoint has begun serialization we must not read its portal,
      // lest we potentially lose updates.
      return;
    }
    portal = portal_;
  }

  DrainResult result = DrainPeerUpdates(portal->handle());

  base::AutoLock lock(lock_);
  if (result.dead) {
    is_peer_closed_ = true;
  }

  if (result.num_bytes_changed == 0) {
    return;
  }

  if (is_producer()) {
    // The consumer has consumed these bytes.
    data_.DiscardAll(result.num_bytes_changed);
    return;
  }

  // The producer has produced these bytes.
  DCHECK(is_consumer());
  data_.ExtendDataRange(result.num_bytes_changed);
  has_new_data_ = true;
}

bool DataPipe::DeserializeRingBuffer(const RingBuffer::SerializedState& state) {
  base::AutoLock lock(lock_);
  if (!data_.Deserialize(state) || data_.data_size() % element_size_ != 0) {
    return false;
  }
  return true;
}

DataPipe::Pair::Pair() = default;

DataPipe::Pair::Pair(const Pair&) = default;

DataPipe::Pair& DataPipe::Pair::operator=(const Pair&) = default;

DataPipe::Pair::~Pair() = default;

}  // namespace mojo::core::ipcz_driver
