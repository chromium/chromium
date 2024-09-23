// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_TRANSPORT_H_
#define MOJO_CORE_IPCZ_DRIVER_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// An ipcz driver transport implementation backed by a Channel object.
class MOJO_SYSTEM_IMPL_EXPORT Transport : public Object<Transport>,
                                          public Channel::Delegate {
 public:
  // Enumerates the type of node at local endpoint of a Transport object.
  enum EndpointType : uint32_t {
    kBroker,
    kNonBroker,
  };

  struct EndpointTypes {
    EndpointType source;
    EndpointType destination;
  };
  Transport(EndpointTypes endpoint_types,
            PlatformChannelEndpoint endpoint,
            base::Process remote_process,
            bool is_remote_process_untrusted = false);

  // Static helper that is slightly more readable due to better type deduction
  // than MakeRefCounted<T>.
  static scoped_refptr<Transport> Create(
      EndpointTypes endpoint_types,
      PlatformChannelEndpoint endpoint,
      base::Process remote_process = base::Process(),
      bool is_remote_process_untrusted = false);

  static std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>>
  CreatePair(EndpointType first_type, EndpointType second_type);

  // Accessors for a global TaskRunner to use for Transport I/O.
  static void SetIOTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> runner);
  static const scoped_refptr<base::SingleThreadTaskRunner>& GetIOTaskRunner();

  static constexpr Type object_type() { return kTransport; }

  EndpointType source_type() const { return endpoint_types_.source; }
  EndpointType destination_type() const { return endpoint_types_.destination; }
  const base::Process& remote_process() const { return remote_process_; }

  // Provides a handle to the remote process on the other end of this transport.
  // If this is called, it must be before the Transport is activated.
  void set_remote_process(base::Process process) {
    DCHECK(!remote_process_.IsValid());
    remote_process_ = std::move(process);
  }

  void set_leak_channel_on_shutdown(bool leak) {
    leak_channel_on_shutdown_ = leak;
  }

  void set_is_peer_trusted(bool trusted) { is_peer_trusted_ = trusted; }
  bool is_peer_trusted() const { return is_peer_trusted_; }

  void set_is_trusted_by_peer(bool trusted) { is_trusted_by_peer_ = trusted; }
  bool is_trusted_by_peer() const { return is_trusted_by_peer_; }

  void SetErrorHandler(MojoProcessErrorHandler handler, uintptr_t context) {
    error_handler_ = handler;
    error_handler_context_ = context;
  }

  // Overrides the IO task runner used to monitor this transport for IO. Unless
  // this is called, all Transports use the global IO task runner by default.
  void OverrideIOTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Takes ownership of the Transport's underlying channel endpoint, effectively
  // invalidating the transport. May only be called on a Transport which has not
  // yet been activated, and only when the channel endpoint is not a server.
  PlatformChannelEndpoint TakeEndpoint() {
    return std::move(inactive_endpoint_);
  }

  // Handles reports of bad activity from ipcz, resulting from parcel rejection
  // by the application.
  void ReportBadActivity(const std::string& error_message);

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
      Transport& from_transport,
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

  // Indicates whether this transport should serialize its remote process handle
  // along with its endpoint handle being serialized for transmission over
  // `transmitter`. This must only be true if we have a valid remote process
  // handle and `transmitter` goes to a broker. Always false on non-Windows
  // platforms.
  bool ShouldSerializeProcessHandle(Transport& transmitter) const;

  const EndpointTypes endpoint_types_;
  base::Process remote_process_;
  MojoProcessErrorHandler error_handler_ = nullptr;
  uintptr_t error_handler_context_ = 0;
  bool leak_channel_on_shutdown_ = false;

  // Indicates whether the remote transport endpoint is "trusted" by this
  // endpoint. In practice this means we will accept pre-duplicated handles from
  // the remote process on Windows. This bit is ignored if the remote endpoint
  // is a broker, since brokers are implicitly trusted; and it's currently
  // meaningless on platforms other than Windows.
  bool is_peer_trusted_ = false;

  // Indicates whether this endpoint is "trusted" by the remote endpoint.
  // In practice this means the remote endpoint will accept pre-duplicated
  // handles from us on Windows. This bit is ignored if the local endpoint is a
  // broker, since brokers are implicitly trusted; and it's currently
  // meaningless on platforms other than Windows.
  bool is_trusted_by_peer_ = false;

#if BUILDFLAG(IS_WIN)
  // Indicates whether the remote process is "untrusted" in Mojo parlance,
  // meaning this Transport restricts what kinds of objects can be transferred
  // from this end (Windows only.)
  bool is_remote_process_untrusted_;
#endif

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
  // TODO(crbug.com/40058840): Refactor Channel so that this is
  // unnecessary, once the non-ipcz Mojo implementation is phased out.
  scoped_refptr<Transport> self_reference_for_channel_ GUARDED_BY(lock_);

  // The IO task runner used by this Transport to watch for incoming I/O events.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_{
      GetIOTaskRunner()};

  // These fields are not guarded by locks, since they're only set prior to
  // activation and remain constant throughout the remainder of this object's
  // lifetime.
  IpczHandle ipcz_transport_ = IPCZ_INVALID_HANDLE;
  IpczTransportActivityHandler activity_handler_ = nullptr;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_TRANSPORT_H_
