// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_TRANSPORT_H_
#define MOJO_CORE_IPCZ_DRIVER_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/core/channel.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// An ipcz driver transport implementation backed by a Channel object.
class MOJO_SYSTEM_IMPL_EXPORT Transport : public Object<Transport>,
                                          public Channel::Delegate {
 public:
  // Tracks what type of remote process is on the other end of this transport.
  // This is used for handle brokering decisions on Windows.
  enum Destination : uint32_t {
    kToNonBroker,
    kToBroker,
  };

  Transport(Destination destination,
            PlatformChannelEndpoint endpoint,
            base::Process remote_process = base::Process());

  static std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>>
  CreatePair(Destination first_destination, Destination second_destination);

  static constexpr Type object_type() { return kTransport; }

  Destination destination() const { return destination_; }
  const base::Process& remote_process() const { return remote_process_; }

  // Activates this transport by creating and starting the underlying Channel
  // instance.
  bool Activate(IpczHandle transport,
                IpczTransportActivityHandler activity_handler);

  // Deactives this transport, release and calling ShutDown() on the underlying
  // Channel. Channel shutdown is asynchronous and will conclude with an
  // OnChannelDestroyed() invocation on this Transport.
  bool Deactivate();

  // Transmits `data` and `handles` over the underlying Channel. All handles in
  // `handles` must reference TransmissibleHandle instances with an underlying
  // handle the Channel can transmit out-of-band from `data`.
  bool Transmit(base::span<const uint8_t> data,
                base::span<const IpczDriverHandle> handles);

  // Attempts to serialize `object` for eventual transmission over this
  // Transport. This essentially implements the mojo-ipcz driver's Serialize()
  // API and behaves according to its specification. Upon success, `object` may
  // be invalidated.
  IpczResult SerializeObject(ObjectBase& object,
                             void* data,
                             size_t* num_bytes,
                             IpczDriverHandle* handles,
                             size_t* num_handles);

  // Deserializes a new driver object from `bytes` and `handles` received over
  // this Transport.
  IpczResult DeserializeObject(base::span<const uint8_t> bytes,
                               base::span<const IpczDriverHandle> handles,
                               scoped_refptr<ObjectBase>& object);

  // Object:
  void Close() override;
  bool IsSerializable() const override;
  bool GetSerializedDimensions(Transport& transmitter,
                               size_t& num_bytes,
                               size_t& num_handles) override;
  bool Serialize(Transport& transmitter,
                 base::span<uint8_t> data,
                 base::span<PlatformHandle> handles) override;

  static scoped_refptr<Transport> Deserialize(
      base::span<const uint8_t> data,
      base::span<PlatformHandle> handles);

  // Channel::Delegate:
  bool IsIpczTransport() const override;
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;
  void OnChannelError(Channel::Error error) override;
  void OnChannelDestroyed() override;

 private:
  struct PendingTransmission {
    PendingTransmission();
    PendingTransmission(PendingTransmission&&);
    PendingTransmission& operator=(PendingTransmission&&);
    ~PendingTransmission();

    std::vector<uint8_t> bytes;
    std::vector<PlatformHandle> handles;
  };

  ~Transport() override;

  bool CanTransmitHandles() const;

  const Destination destination_;
  const base::Process remote_process_;

  // The channel endpoint which will be used by this Transport to construct and
  // start its underlying Channel instance once activated. Not guarded by a lock
  // since it must not accessed beyond activation, where thread safety becomes a
  // factor.
  PlatformChannelEndpoint inactive_endpoint_;

  base::Lock lock_;
  scoped_refptr<Channel> channel_ GUARDED_BY(lock_);

  // Transmissions prior to activation must be queued, as the Channel is not
  // created until then. Queued messages are stored here. Once the Transport has
  // been activated, this is no longer used.
  std::vector<PendingTransmission> pending_transmissions_ GUARDED_BY(lock_);

  // NOTE: Channel does not retain a reference to its Delegate (this Transport,
  // in our case) and it may call back into us from any thread as long as it's
  // still alive. So we retain a self-reference on behalf of the Channel and
  // release it only once notified of the Channel's destruction.
  //
  // TODO(https://crbug.com/1299283): Refactor Channel so that this is
  // unnecessary, once the non-ipcz Mojo implementation is phased out.
  scoped_refptr<Transport> self_reference_for_channel_ GUARDED_BY(lock_);

  // These fields are not guarded by locks, since they're only set prior to
  // activation and remain constant throughout the remainder of this object's
  // lifetime.
  IpczHandle ipcz_transport_ = IPCZ_INVALID_HANDLE;
  IpczTransportActivityHandler activity_handler_ = nullptr;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_TRANSPORT_H_
