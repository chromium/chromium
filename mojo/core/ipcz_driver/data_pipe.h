// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_DATA_PIPE_H_
#define MOJO_CORE_IPCZ_DRIVER_DATA_PIPE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/ipcz_driver/ring_buffer.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

class Transport;

// DataPipe implements a Mojo data pipe producer or consumer endpoint by
// wrapping a shared memory ring buffer and using ipcz portals to communicate
// read and write quantities end-to-end.
//
// TODO(crbug.com/40058840): Once everything is transitioned to mojo-ipcz
// this object (and builtin data pipe bindings support in general) can be
// deprecated in favor of a mojom-based library implementation of data pipes,
// built directly on ipcz portals. For now they're implemented as ipcz driver
// objects so they can continue to be represented on the wire as a single Mojo
// handle.
class DataPipe : public Object<DataPipe> {
 public:
  enum class EndpointType : uint32_t {
    kProducer,
    kConsumer,
  };

  struct Config {
    // The size of each "element" in bytes. Relevant for Mojo data pipe APIs
    // which read in write in terms of element counts.
    size_t element_size;

    // The total byte capacity of the data pipe. This is a best-effort limit on
    // the number of unread bytes allowed to accumulate at the consumer before
    // the producer waits to produce more data.
    size_t byte_capacity;

    // Indicates whether the peer is known to be closed.
    bool is_peer_closed;
  };

  // A wrapper for the DataPipe's underlying portal, used for thread-safe portal
  // ownership and access.
  class PortalWrapper : public base::RefCountedThreadSafe<PortalWrapper> {
   public:
    explicit PortalWrapper(ScopedIpczHandle portal);
    PortalWrapper(const PortalWrapper&) = delete;
    void operator=(const PortalWrapper&) = delete;

    IpczHandle handle() const { return handle_.get(); }
    void set_handle(ScopedIpczHandle handle) { handle_ = std::move(handle); }

    ScopedIpczHandle TakeHandle() { return std::move(handle_); }

   private:
    friend class base::RefCountedThreadSafe<PortalWrapper>;

    ~PortalWrapper();

    ScopedIpczHandle handle_;
  };

  // Constructs a partial DataPipe endpoint of type `endpoint_type`, configured
  // according to `config`, and using `buffer` for the underlying transfer
  // buffer. `mapping` must be a valid mapping of all the memory referenced by
  // `buffer`.
  //
  // This DataPipe is not usable until it's given a portal via AdoptPortal().
  DataPipe(EndpointType endpoint_type,
           const Config& config,
           scoped_refptr<SharedBuffer> buffer,
           scoped_refptr<SharedBufferMapping> mapping);

  static Type object_type() { return kDataPipe; }

  // Constructs a new pair of DataPipe endpoints, one for reading and one for
  // writing. May fail and return null if the data pipe's shared memory backing
  // could not be allocated.
  struct Pair {
    Pair();
    Pair(const Pair&);
    Pair& operator=(const Pair&);
    ~Pair();

    scoped_refptr<DataPipe> consumer;
    scoped_refptr<DataPipe> producer;
  };
  static std::optional<Pair> CreatePair(const Config& config);

  bool is_producer() const { return endpoint_type_ == EndpointType::kProducer; }
  bool is_consumer() const { return endpoint_type_ == EndpointType::kConsumer; }

  // Provides this DataPipe instance with a portal to own and use for I/O. Must
  // only be called on a DataPipe that does not already have a portal. Returns
  // true if successful or false if `portal` is not a valid portal handle.
  bool AdoptPortal(ScopedIpczHandle portal);

  // Returns a reference to the underlying portal which can be safely used from
  // any thread. May return null if no portal has been adopted by this DataPipe
  // yet.
  scoped_refptr<PortalWrapper> GetPortal();

  // Takes ownership of the DataPipe's portal (for serialization) and returns
  // the handle to it.
  ScopedIpczHandle TakePortal();

  // Implements Mojo's WriteData API.
  MojoResult WriteData(const void* elements,
                       uint32_t& num_bytes,
                       MojoWriteDataFlags flags);
  MojoResult BeginWriteData(void*& data,
                            uint32_t& num_bytes,
                            MojoBeginWriteDataFlags flags);
  MojoResult EndWriteData(size_t num_bytes_produced);
  MojoResult ReadData(void* elements,
                      uint32_t& num_bytes,
                      MojoReadDataFlags flags);
  MojoResult BeginReadData(const void*& buffer, uint32_t& buffer_num_bytes);
  MojoResult EndReadData(size_t num_bytes_consumed);

  // ObjectBase:
  void Close() override;
  bool IsSerializable() const override;
  bool GetSerializedDimensions(Transport& transmitter,
                               size_t& num_bytes,
                               size_t& num_handles) override;
  bool Serialize(Transport& transmitter,
                 base::span<uint8_t> data,
                 base::span<PlatformHandle> handles) override;

  static scoped_refptr<DataPipe> Deserialize(
      base::span<const uint8_t> data,
      base::span<PlatformHandle> handles);

  // Returns Mojo signals to reflect the effective state of this DataPipe and
  // its control portal within `signals_state`. Returns true on success or false
  // if the DataPipe's signal state is unspecified due to impending closure. In
  // the latter case `signals_state` is zeroed out.
  bool GetSignals(MojoHandleSignalsState& signals_state);

  // Flushes any incoming status updates from the peer. Note that this may
  // trigger trap events before returning, since it can modify the state of the
  // control portal.
  void FlushUpdatesFromPeer() LOCKS_EXCLUDED(lock_);

 private:
  ~DataPipe() override;

  bool DeserializeRingBuffer(const RingBuffer::SerializedState& state);

  const EndpointType endpoint_type_;
  const size_t element_size_;

  mutable base::Lock lock_;

  // A portal used to transfer control messages between producer and consumer.
  // Ref-counted separately since this object needs to maintain thread-safe
  // access and ensure that Close() doesn't race with other operations on the
  // underlying portal.
  scoped_refptr<PortalWrapper> portal_ GUARDED_BY(lock_);

  // Owns a reference to the underlying shared memory region, and manages this
  // data pipe endpoint's local cache of the buffer state.
  scoped_refptr<SharedBuffer> buffer_ GUARDED_BY(lock_);
  RingBuffer data_ GUARDED_BY(lock_);
  std::optional<RingBuffer::DirectWriter> two_phase_writer_;
  std::optional<RingBuffer::DirectReader> two_phase_reader_;

  // Indicates whether this endpoint is in the process of being serialized and
  // transmitted elsewhere.
  bool in_transit_ GUARDED_BY(lock_) = false;

  // Indicates whether the peer endpoint is known to be closed.
  bool is_peer_closed_ GUARDED_BY(lock_) = false;

  // This loosely tracks whether new data has arrived since the last ReadData or
  // BeginReadData attempt.
  bool has_new_data_ GUARDED_BY(lock_) = false;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_DATA_PIPE_H_
