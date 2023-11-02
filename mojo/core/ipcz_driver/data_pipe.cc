// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/data_pipe.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_math.h"
#include "base/synchronization/lock.h"
#include "mojo/core/ipcz_api.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// The wire representation of a serialized DataPipe endpoint.
struct IPCZ_ALIGN(8) DataPipeHeader {
  // The size of this structure, in bytes. Used for versioning.
  uint32_t size;

  // The size in bytes of an element of this pipe.
  uint32_t element_size;

  // The write capacity of the pipe endpoint in bytes. Always zero for consumer
  // endpoints, always non-zero for producers.
  uint32_t byte_capacity;
};

IpczResult EndReadDataImpl(IpczHandle portal,
                           size_t element_size,
                           size_t num_bytes_consumed) {
  if (num_bytes_consumed == 0) {
    return GetIpczAPI().EndGet(portal, 0, 0, IPCZ_NO_FLAGS, nullptr, nullptr,
                               nullptr);
  }

  IpczResult result;
  if (num_bytes_consumed % element_size != 0) {
    result = IPCZ_RESULT_INVALID_ARGUMENT;
  } else {
    result = GetIpczAPI().EndGet(portal, num_bytes_consumed, 0, IPCZ_NO_FLAGS,
                                 nullptr, nullptr, nullptr);
    if (result == IPCZ_RESULT_OUT_OF_RANGE) {
      // Mojo expects a different result in this case.
      result = IPCZ_RESULT_INVALID_ARGUMENT;
    }
  }

  if (result != IPCZ_RESULT_OK) {
    // Unlike with ipcz, Mojo's two-phase operations are expected to terminate
    // in all failure modes.
    GetIpczAPI().EndGet(portal, 0, 0, IPCZ_END_GET_ABORT, nullptr, nullptr,
                        nullptr);
  }
  return result;
}

}  // namespace

DataPipe::PortalWrapper::PortalWrapper(IpczHandle handle) : handle_(handle) {}

DataPipe::PortalWrapper::~PortalWrapper() {
  if (handle_ != IPCZ_INVALID_HANDLE) {
    GetIpczAPI().Close(handle_, IPCZ_NO_FLAGS, nullptr);
  }
}

DataPipe::DataPipe(const Config& config)
    : element_size_(config.element_size),
      limits_({
          .size = sizeof(IpczPutLimits),
          .max_queued_parcels = std::numeric_limits<size_t>::max(),
          .max_queued_bytes = config.byte_capacity,
      }) {
  DCHECK_GT(element_size_, 0u);
  DCHECK_LE(element_size_, std::numeric_limits<uint32_t>::max());
  DCHECK_LE(byte_capacity(), std::numeric_limits<uint32_t>::max());
}

DataPipe::~DataPipe() {
  Close();
}

// static
DataPipe::Pair DataPipe::CreatePair(const Config& config) {
  IpczHandle producer;
  IpczHandle consumer;
  const IpczResult result = GetIpczAPI().OpenPortals(
      GetIpczNode(), IPCZ_NO_FLAGS, nullptr, &producer, &consumer);
  DCHECK_EQ(result, IPCZ_RESULT_OK);

  Pair pair;
  pair.consumer = base::MakeRefCounted<DataPipe>(
      Config{.element_size = config.element_size, .byte_capacity = 0});
  pair.consumer->AdoptPortal(consumer);

  pair.producer = base::MakeRefCounted<DataPipe>(config);
  pair.producer->AdoptPortal(producer);
  return pair;
}

void DataPipe::AdoptPortal(IpczHandle portal) {
  auto wrapper = base::MakeRefCounted<PortalWrapper>(portal);
  {
    base::AutoLock lock(lock_);
    DCHECK(!portal_);
    portal_ = wrapper;
  }

  if (byte_capacity() == 0) {
    // Immediately start watching for new parcels so we can maintain the
    // new-data signal.
    WatchForNewData();

    // If there are any parcels ready to read now, treat them as if they're new
    // data. If we're wrong the only side effect is that an observer may attempt
    // a single redundant read of the pipe.
    IpczPortalStatus status = {.size = sizeof(status)};
    GetIpczAPI().QueryPortalStatus(wrapper->handle(), IPCZ_NO_FLAGS, nullptr,
                                   &status);
    if (status.num_local_bytes > 0) {
      SetHasNewData();
    }
  }
}

scoped_refptr<DataPipe::PortalWrapper> DataPipe::GetPortal() {
  base::AutoLock lock(lock_);
  return portal_;
}

void DataPipe::SetHasNewData() {
  base::AutoLock lock(lock_);
  has_new_data_ = true;
}

bool DataPipe::HasNewData() {
  base::AutoLock lock(lock_);
  return has_new_data_;
}

IpczHandle DataPipe::TakePortal() {
  scoped_refptr<PortalWrapper> portal;

  base::AutoLock lock(lock_);
  portal.swap(portal_);
  return portal->TakeHandle();
}

IpczResult DataPipe::WriteData(const void* elements,
                               uint32_t& num_bytes,
                               MojoWriteDataFlags flags) {
  if (num_bytes % element_size_ != 0) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const MojoBeginWriteDataFlags begin_write_flags =
      (flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE)
          ? MOJO_BEGIN_WRITE_DATA_FLAG_ALL_OR_NONE
          : MOJO_BEGIN_WRITE_DATA_FLAG_NONE;

  const uint32_t max_num_bytes = num_bytes;
  void* data;
  const IpczResult begin_result =
      BeginWriteData(data, num_bytes, begin_write_flags);
  if (begin_result != IPCZ_RESULT_OK) {
    return begin_result;
  }

  num_bytes = std::min(num_bytes, max_num_bytes);
  memcpy(data, elements, num_bytes);

  const IpczResult end_result = EndWriteData(num_bytes);
  DCHECK_EQ(end_result, IPCZ_RESULT_OK);
  return IPCZ_RESULT_OK;
}

IpczResult DataPipe::BeginWriteData(void*& data,
                                    uint32_t& num_bytes,
                                    MojoBeginWriteDataFlags flags) {
  scoped_refptr<PortalWrapper> portal = GetPortal();
  if (!portal) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const bool allow_partial = !(flags & MOJO_BEGIN_WRITE_DATA_FLAG_ALL_OR_NONE);
  const IpczBeginPutFlags begin_put_flags =
      allow_partial ? IPCZ_BEGIN_PUT_ALLOW_PARTIAL : IPCZ_NO_FLAGS;

  const IpczBeginPutOptions begin_put_options = {
      .size = sizeof(begin_put_options),
      .limits = &limits_,
  };

  // Several MojoBeginWriteData() callers supply an input size of zero and
  // expect to get some buffer capacity based on the pipe's total configured
  // capacity. mojo-ipcz emulates a commonly chosen capacity of 64k.
  constexpr size_t kDefaultBufferSize = 64 * 1024;
  size_t put_num_bytes =
      (num_bytes || !allow_partial) ? num_bytes : kDefaultBufferSize;
  if (allow_partial) {
    put_num_bytes = std::min(put_num_bytes, limits_.max_queued_bytes);
  }
  const IpczResult begin_put_result =
      GetIpczAPI().BeginPut(portal->handle(), begin_put_flags,
                            &begin_put_options, &put_num_bytes, &data);
  if (begin_put_result == IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    if (allow_partial) {
      return MOJO_RESULT_SHOULD_WAIT;
    }
    return MOJO_RESULT_OUT_OF_RANGE;
  }
  if (begin_put_result != IPCZ_RESULT_OK) {
    return begin_put_result;
  }
  num_bytes = base::checked_cast<uint32_t>(put_num_bytes);
  return IPCZ_RESULT_OK;
}

IpczResult DataPipe::EndWriteData(size_t num_bytes_produced) {
  scoped_refptr<PortalWrapper> portal = GetPortal();
  if (!portal) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  IpczResult result;
  IpczEndPutFlags flags = IPCZ_NO_FLAGS;
  if (num_bytes_produced == 0) {
    // We want to abort this write if 0 bytes were produced, rather than
    // committing an empty parcel.
    flags = IPCZ_END_PUT_ABORT;
  }

  if (num_bytes_produced % element_size_ != 0) {
    result = IPCZ_RESULT_INVALID_ARGUMENT;
  } else {
    result = GetIpczAPI().EndPut(portal->handle(), num_bytes_produced, nullptr,
                                 0, flags, nullptr);
  }

  if (result != IPCZ_RESULT_OK) {
    // Unlike with ipcz, Mojo's two-phase operations are expected to terminate
    // in all failure modes.
    GetIpczAPI().EndPut(portal->handle(), 0, nullptr, 0, IPCZ_END_PUT_ABORT,
                        nullptr);
  }

  if (result == IPCZ_RESULT_NOT_FOUND) {
    // MojoWriteData returns success when ending a two-phase write to a pipe
    // whose consumer is already gone.
    return IPCZ_RESULT_OK;
  }
  return result;
}

IpczResult DataPipe::ReadData(void* elements,
                              uint32_t& num_bytes,
                              MojoReadDataFlags flags) {
  const bool query = flags & MOJO_READ_DATA_FLAG_QUERY;
  const bool peek = flags & MOJO_READ_DATA_FLAG_PEEK;
  const bool discard = flags & MOJO_READ_DATA_FLAG_DISCARD;
  const bool allow_partial = !(flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE);

  // Filter for assorted configurations that aren't used in practice and which
  // therefore do not require support here.
  if (!query && !discard && !elements) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  if ((peek && discard) || (query && (peek || discard))) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  IpczHandle portal;
  IpczPortalStatus status = {.size = sizeof(status)};
  {
    base::AutoLock lock(lock_);
    if (!portal_) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }

    portal = portal_->handle();

    const IpczResult query_result =
        GetIpczAPI().QueryPortalStatus(portal, IPCZ_NO_FLAGS, nullptr, &status);
    if (query_result != IPCZ_RESULT_OK) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }

    has_new_data_ = false;
  }

  const size_t num_bytes_available = status.num_local_bytes;
  if (query) {
    num_bytes = base::checked_cast<uint32_t>(num_bytes_available);
    return IPCZ_RESULT_OK;
  }

  if (num_bytes % element_size_ != 0) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const bool is_peer_closed = status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED;

  // For all-or-none reads, we loop getting parcel data until we've satisfied
  // the request. Note that this is not thread-safe, so all-or-none data pipe
  // reads will not work properly if a pipe consumer has multiple threads
  // reading from it. This is not an issue in practice: all-or-none reads are
  // very rarely used, and no known data pipe consumers have multiple reader
  // threads.
  if (!allow_partial) {
    if (num_bytes_available < num_bytes) {
      return is_peer_closed ? IPCZ_RESULT_FAILED_PRECONDITION
                            : IPCZ_RESULT_OUT_OF_RANGE;
    }

    auto out_data =
        base::span<uint8_t>(static_cast<uint8_t*>(elements), num_bytes);
    while (!out_data.empty()) {
      const void* data;
      size_t get_num_bytes;
      const IpczResult begin_result = GetIpczAPI().BeginGet(
          portal, IPCZ_NO_FLAGS, nullptr, &data, &get_num_bytes, nullptr);

      // Hitting this DCHECK implies that another thread is reading our portal.
      DCHECK_EQ(begin_result, IPCZ_RESULT_OK);
      const size_t num_bytes_to_consume =
          std::min(get_num_bytes, out_data.size());
      if (!discard) {
        memcpy(out_data.data(), data, num_bytes_to_consume);
      }
      out_data = out_data.subspan(num_bytes_to_consume);

      if (peek) {
        EndReadDataImpl(portal, element_size_, 0);
      } else {
        EndReadDataImpl(portal, element_size_, num_bytes_to_consume);
      }
    }
    return IPCZ_RESULT_OK;
  }

  // Potentially partial reads use a two-phase read.
  const void* data;
  uint32_t num_bytes_available_for_get;
  const IpczResult begin_result =
      BeginReadData(data, num_bytes_available_for_get);
  if (begin_result != IPCZ_RESULT_OK) {
    return begin_result;
  }

  const size_t num_bytes_to_consume =
      std::min(num_bytes_available_for_get, num_bytes);
  if (!discard) {
    memcpy(elements, data, num_bytes_to_consume);
  }
  if (peek) {
    EndReadDataImpl(portal, element_size_, 0);
  } else {
    EndReadDataImpl(portal, element_size_, num_bytes_to_consume);
  }

  num_bytes = base::checked_cast<uint32_t>(num_bytes_to_consume);
  return IPCZ_RESULT_OK;
}

IpczResult DataPipe::BeginReadData(const void*& buffer,
                                   uint32_t& buffer_num_bytes) {
  base::AutoLock lock(lock_);
  if (!portal_) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  size_t num_bytes;
  const IpczResult begin_result = GetIpczAPI().BeginGet(
      portal_->handle(), IPCZ_NO_FLAGS, nullptr, &buffer, &num_bytes, nullptr);
  if (begin_result != IPCZ_RESULT_OK) {
    return begin_result;
  }

  has_new_data_ = false;
  buffer_num_bytes = num_bytes;
  return IPCZ_RESULT_OK;
}

IpczResult DataPipe::EndReadData(size_t num_bytes_consumed) {
  scoped_refptr<PortalWrapper> portal = GetPortal();
  if (!portal) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  return EndReadDataImpl(portal->handle(), element_size_, num_bytes_consumed);
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
  return true;
}

bool DataPipe::GetSerializedDimensions(Transport& transmitter,
                                       size_t& num_bytes,
                                       size_t& num_handles) {
  num_bytes = sizeof(DataPipeHeader);
  num_handles = 0;
  return true;
}

bool DataPipe::Serialize(Transport& transmitter,
                         base::span<uint8_t> data,
                         base::span<PlatformHandle> handles) {
  // NOTE: Drivers cannot serialize their objects to other ipcz objects (such as
  // portals) through the driver API. Instead, mojo-ipcz serializes and
  // deserializes a DataPipe's portal within WriteMessage() and ReadMessage()
  // in core_ipcz.cc. Here we only serialize a header to convey pipe capacity.
  DCHECK_EQ(data.size(), sizeof(DataPipeHeader));
  auto& header = *reinterpret_cast<DataPipeHeader*>(data.data());
  header.size = sizeof(header);
  header.element_size = static_cast<uint32_t>(element_size_);
  header.byte_capacity = static_cast<uint32_t>(limits_.max_queued_bytes);
  return true;
}

// static
scoped_refptr<DataPipe> DataPipe::Deserialize(
    base::span<const uint8_t> data,
    base::span<const PlatformHandle> handles) {
  if (data.size() < sizeof(DataPipeHeader)) {
    return nullptr;
  }

  const auto& header = *reinterpret_cast<const DataPipeHeader*>(data.data());
  if (header.size < sizeof(header)) {
    return nullptr;
  }

  return base::MakeRefCounted<DataPipe>(
      Config{.element_size = header.element_size,
             .byte_capacity = header.byte_capacity});
}

void DataPipe::WatchForNewData() {
  DCHECK(is_consumer());

  auto handler = [](const IpczTrapEvent* event) {
    auto& pipe = *reinterpret_cast<DataPipe*>(event->context);
    if (event->condition_flags & IPCZ_TRAP_NEW_LOCAL_PARCEL) {
      pipe.WatchForNewData();
      pipe.SetHasNewData();
    }

    // Balanced by the leaked reference just before trap installation below.
    pipe.Release();
  };

  scoped_refptr<PortalWrapper> portal = GetPortal();
  if (!portal) {
    return;
  }

  // Leak a self reference as long as the trap below is installed. Balanced in
  // the lambda above.
  AddRef();

  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_NEW_LOCAL_PARCEL,
  };
  const uintptr_t context = reinterpret_cast<uintptr_t>(this);
  const IpczResult result =
      GetIpczAPI().Trap(portal->handle(), &conditions, handler, context,
                        IPCZ_NO_FLAGS, nullptr, nullptr, nullptr);
  DCHECK_EQ(result, IPCZ_RESULT_OK);
}

DataPipe::Pair::Pair() = default;

DataPipe::Pair::Pair(const Pair&) = default;

DataPipe::Pair& DataPipe::Pair::operator=(const Pair&) = default;

DataPipe::Pair::~Pair() = default;

}  // namespace mojo::core::ipcz_driver
