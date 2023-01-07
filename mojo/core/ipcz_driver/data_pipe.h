// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_DATA_PIPE_H_
#define MOJO_CORE_IPCZ_DRIVER_DATA_PIPE_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

class Transport;

// DataPipe emulates a Mojo data pipe producer or consumer endpoint by wrapping
// a portal and enforcing fixed limits on every Put transaction. This is to
// satisfy assumptions of fixed data pipe capacity in application code, since
// such assumptions may have performance implications.
//
// TODO(https://crbug.com/1299283): Once everything is transitioned to mojo-ipcz
// this object (and builtin data pipe bindings support in general) can
// deprecated in favor of a mojom-based library implementation of data pipes,
// built directly on ipcz portals.
class DataPipe : public Object<DataPipe> {
 public:
  struct Config {
    // The size of each "element" in bytes. Relevant for Mojo data pipe APIs
    // which read in write in terms of element counts.
    size_t element_size;

    // The total byte capacity of the data pipe. This is a best-effort limit on
    // the number of unread bytes allowed to accumulate at the consumer before
    // the producer waits to produce more data.
    size_t byte_capacity;
  };

  // A wrapper for the DataPipe's underlying portal, used for thread-safe portal
  // ownership and access.
  class PortalWrapper : public base::RefCountedThreadSafe<PortalWrapper> {
   public:
    explicit PortalWrapper(IpczHandle portal);
    PortalWrapper(const PortalWrapper&) = delete;
    void operator=(const PortalWrapper&) = delete;

    IpczHandle handle() const { return handle_; }
    void set_handle(IpczHandle handle) { handle_ = handle; }

    IpczHandle TakeHandle() {
      return std::exchange(handle_, IPCZ_INVALID_HANDLE);
    }

   private:
    friend class base::RefCountedThreadSafe<PortalWrapper>;

    ~PortalWrapper();

    IpczHandle handle_ = IPCZ_INVALID_HANDLE;
  };

  // Constructs a partial DataPipe endpoint configured according to `config`.
  // This DataPipe is not usable until it's given a portal via AdoptPortal().
  explicit DataPipe(const Config& config);

  static Type object_type() { return kDataPipe; }

  // Constructs a new pair of DataPipe endpoints, one for reading and one for
  // writing.
  struct Pair {
    Pair();
    Pair(const Pair&);
    Pair& operator=(const Pair&);
    ~Pair();

    scoped_refptr<DataPipe> consumer;
    scoped_refptr<DataPipe> producer;
  };
  static Pair CreatePair(const Config& config);

  size_t byte_capacity() const { return limits_.max_queued_bytes; }
  size_t element_size() const { return element_size_; }
  bool is_producer() const { return byte_capacity() > 0; }
  bool is_consumer() const { return byte_capacity() == 0; }

  // Provides this DataPipe instance with a portal to own and use for I/O. Must
  // only be called on a DataPipe that does not already have a portal.
  void AdoptPortal(IpczHandle portal);

  // Returns a reference to the underlying portal which can be safely used from
  // any thread. May return null if no portal has been adopted by this DataPipe
  // yet.
  scoped_refptr<PortalWrapper> GetPortal();

  // Indicates whether this DataPipe is currently flagged as having newly
  // arrived data since the last read attempt. Used to emulate Mojo data pipes'
  // MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE signal.
  void SetHasNewData();
  bool HasNewData();

  // Takes ownership of the DataPipe's portal (for serialization) and returns
  // the handle to it.
  IpczHandle TakePortal();

  // Implements Mojo's WriteData API.
  IpczResult WriteData(const void* elements,
                       uint32_t& num_bytes,
                       MojoWriteDataFlags flags);
  IpczResult BeginWriteData(void*& data,
                            uint32_t& num_bytes,
                            MojoBeginWriteDataFlags flags);
  IpczResult EndWriteData(size_t num_bytes_produced);
  IpczResult ReadData(void* elements,
                      uint32_t& num_bytes,
                      MojoReadDataFlags flags);
  IpczResult BeginReadData(const void*& buffer, uint32_t& buffer_num_bytes);
  IpczResult EndReadData(size_t num_bytes_consumed);

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
      base::span<const PlatformHandle> handles);

 private:
  ~DataPipe() override;

  // We need to emulate Mojo's MOJO_HANDLE_SIGNAL_NEW_DATA_READABALE signal on
  // data pipe consumer endpoints. This signal is raised any time new data
  // arrives, and it remains high until the next ReadData() or BeginReadData()
  // call on the same endpoint.
  //
  // ipcz does not implement such a trap condition or status flag for portals,
  // but it does support installation of edge-triggered traps for new data
  // arrival. We can therefore emulate Mojo behavior with a slight hack: any
  // MojoTrap trigger watching a DataPipe consumer for NEW_DATA_READABLE will
  // call directly into the DataPipe instance with SetHasNewData() *before*
  // invoking any corresponding event handler, so that the signal state is
  // correct by the time the handler is invoked.
  //
  // But some consumers also query this signal state separately, without
  // necessarily installing a trap; and trap installation itself queries the
  // status to block installation if conditions are already satisfied. DataPipe
  // therefore also repeatedly installs a trap on itself to invoke
  // SetHasNewData() any time a new parcel arrives. This ensures that the
  // signal's state is always accurate.
  void WatchForNewData();

  const size_t element_size_;
  const IpczPutLimits limits_;

  base::Lock lock_;

  // A portal used to transfer data to and from the other end of the DataPipe.
  // Ref-counted separately since this object needs to maintain thread-safe
  // access and ensure that Close() doesn't race with other operations on the
  // underlying portal.
  scoped_refptr<PortalWrapper> portal_ GUARDED_BY(lock_);

  // This loosely tracks whether new data has arrived since the last ReadData or
  // BeginReadData attempt.
  bool has_new_data_ GUARDED_BY(lock_) = false;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_DATA_PIPE_H_
