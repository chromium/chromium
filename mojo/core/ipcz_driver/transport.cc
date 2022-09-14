// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/transport.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/stack_container.h"
#include "base/functional/overloaded.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "mojo/core/core.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "mojo/core/ipcz_driver/invitation.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// Header serialized at the beginning of all mojo-ipcz driver objects.
struct IPCZ_ALIGN(8) ObjectHeader {
  // The size of this header in bytes. Used for versioning.
  uint32_t size;

  // Identifies the type of object serialized.
  ObjectBase::Type type;

#if BUILDFLAG(IS_WIN)
  // On Windows only, platform handles are serialized as part of object data.
  // This identifies how many packed HANDLE values immediately follow this
  // header and precede the actual object data.
  uint32_t num_handles;

  // Padding for 8-byte size alignment.
  uint32_t reserved;
#endif
};

// Header for a serialized Transport object.
struct IPCZ_ALIGN(8) TransportHeader {
  // Indicates what type of destination the other end of this serialized
  // transport is connected to.
  Transport::Destination destination;

  // Indicates whether the remote process on the other end of this transport
  // is the same process sending this object.
  bool is_same_remote_process;
};

#if BUILDFLAG(IS_WIN)
void EncodeHandle(PlatformHandle& handle,
                  const base::Process& remote_process,
                  Transport::Destination destination,
                  HANDLE& out_handle) {
  DCHECK(handle.is_valid());
  if (!remote_process.IsValid()) {
    // When sending to a broker, HANDLE values are encoded as-is. Handles are
    // never sent from a non-broker to another non-broker, by virtue of
    // Transport's Serialize() behavior forcing ipcz to relay through a broker.
    DCHECK_EQ(destination, Transport::kToBroker);
    out_handle = handle.ReleaseHandle();
    return;
  }

  // When sending from a broker to a non-broker, duplicate the handle to the
  // remote process first, then encode that duplicated value.
  BOOL result = ::DuplicateHandle(
      ::GetCurrentProcess(), handle.ReleaseHandle(), remote_process.Handle(),
      &out_handle, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  DCHECK(result);
}

PlatformHandle DecodeHandle(HANDLE handle,
                            const base::Process& remote_process,
                            Transport::Destination destination) {
  if (!remote_process.IsValid()) {
    // Handles coming from a broker are already ours.
    DCHECK(destination == Transport::kToBroker);
    return PlatformHandle(base::win::ScopedHandle(handle));
  }

  // Handles coming from a non-broker to a broker must be duplicated from the
  // source process first.
  HANDLE local_dupe = INVALID_HANDLE_VALUE;
  ::DuplicateHandle(remote_process.Handle(), handle, ::GetCurrentProcess(),
                    &local_dupe, 0, FALSE,
                    DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  return PlatformHandle(base::win::ScopedHandle(local_dupe));
}
#endif  // BUILDFLAG(IS_WIN)

scoped_refptr<base::SingleThreadTaskRunner>& GetIOTaskRunnerStorage() {
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>> runner;
  return *runner;
}
}  // namespace

Transport::Transport(Destination destination,
                     Channel::Endpoint endpoint,
                     base::Process remote_process)
    : destination_(destination),
      remote_process_(std::move(remote_process)),
      inactive_endpoint_(std::move(endpoint)) {}

// static
std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>>
Transport::CreatePair(Destination first_destination,
                      Destination second_destination) {
  PlatformChannel channel;
  auto one = base::MakeRefCounted<Transport>(first_destination,
                                             channel.TakeLocalEndpoint());
  auto two = base::MakeRefCounted<Transport>(second_destination,
                                             channel.TakeRemoteEndpoint());
  return {one, two};
}

Transport::~Transport() {
  if (error_handler_) {
    const MojoProcessErrorDetails details{
        .struct_size = sizeof(details),
        .error_message_length = 0,
        .error_message = nullptr,
        .flags = MOJO_PROCESS_ERROR_FLAG_DISCONNECTED,
    };
    error_handler_(error_handler_context_, &details);
  }
}

// static
void Transport::SetIOTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> runner) {
  GetIOTaskRunnerStorage() = std::move(runner);
}

// static
const scoped_refptr<base::SingleThreadTaskRunner>&
Transport::GetIOTaskRunner() {
  return GetIOTaskRunnerStorage();
}

void Transport::ReportBadActivity(const std::string& error_message) {
  if (!error_handler_) {
    Invitation::InvokeDefaultProcessErrorHandler(error_message);
    return;
  }

  const MojoProcessErrorDetails details{
      .struct_size = sizeof(details),
      .error_message_length =
          base::checked_cast<uint32_t>(error_message.size()),
      .error_message = error_message.c_str(),
      .flags = MOJO_PROCESS_ERROR_FLAG_NONE,
  };
  error_handler_(error_handler_context_, &details);
}

bool Transport::Activate(IpczHandle transport,
                         IpczTransportActivityHandler activity_handler) {
  scoped_refptr<Channel> channel;
  std::vector<PendingTransmission> pending_transmissions;
  {
    base::AutoLock lock(lock_);
    if (channel_ || !IsEndpointValid()) {
      return false;
    }

    ipcz_transport_ = transport;
    activity_handler_ = activity_handler;
    self_reference_for_channel_ = base::WrapRefCounted(this);
    channel_ = Channel::CreateForIpczDriver(this, std::move(inactive_endpoint_),
                                            GetIOTaskRunner());
    channel_->Start();
    if (leak_channel_on_shutdown_) {
      GetIOTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<Channel> channel) { channel->LeakHandle(); },
              channel_));
    }

    if (!pending_transmissions_.empty()) {
      pending_transmissions_.swap(pending_transmissions);
      channel = channel_;
    }
  }

  for (auto& transmission : pending_transmissions) {
    channel->Write(Channel::Message::CreateIpczMessage(
        base::make_span(transmission.bytes), std::move(transmission.handles)));
  }

  return true;
}

bool Transport::Deactivate() {
  scoped_refptr<Channel> channel;
  {
    base::AutoLock lock(lock_);
    if (!channel_) {
      return false;
    }

    channel = std::move(channel_);
  }

  // This will post a task to the Channel's IO thread to complete shutdown. Once
  // the last Channel reference is dropped, it will invoke OnChannelDestroyed()
  // on this Transport. The Transport is kept alive in the meantime by its own
  // retained `self_reference_for_channel_`.
  channel->ShutDown();
  return true;
}

bool Transport::Transmit(base::span<const uint8_t> data,
                         base::span<const IpczDriverHandle> handles) {
#if BUILDFLAG(IS_WIN)
  // All Windows handles must be inlined as message data as part of object
  // serialization, so the driver should never attempt to transmit handles
  // out-of-band there.
  DCHECK(handles.empty());
#endif

  std::vector<PlatformHandle> platform_handles;
  platform_handles.reserve(handles.size());
  for (IpczDriverHandle handle : handles) {
    auto transmissible_handle =
        TransmissiblePlatformHandle::TakeFromHandle(handle);
    DCHECK(transmissible_handle);
    platform_handles.push_back(transmissible_handle->TakeHandle());
  }

  scoped_refptr<Channel> channel;
  {
    base::AutoLock lock(lock_);
    if (IsEndpointValid()) {
      PendingTransmission transmission;
      transmission.bytes = std::vector<uint8_t>(data.begin(), data.end());
      transmission.handles = std::move(platform_handles);
      pending_transmissions_.push_back(std::move(transmission));
      return true;
    }

    if (!channel_) {
      return false;
    }
    channel = channel_;
  }

  channel->Write(
      Channel::Message::CreateIpczMessage(data, std::move(platform_handles)));
  return true;
}

IpczResult Transport::SerializeObject(ObjectBase& object,
                                      void* data,
                                      size_t* num_bytes,
                                      IpczDriverHandle* handles,
                                      size_t* num_handles) {
  size_t object_num_bytes;
  size_t object_num_handles;
  if (!object.GetSerializedDimensions(*this, object_num_bytes,
                                      object_num_handles)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (object_num_handles > 0 && !CanTransmitHandles()) {
    // Let ipcz know that it must relay this object through a broker instead of
    // transmitting it over this transport.
    return IPCZ_RESULT_PERMISSION_DENIED;
  }

#if BUILDFLAG(IS_WIN)
  const size_t required_num_bytes = sizeof(ObjectHeader) + object_num_bytes +
                                    sizeof(HANDLE) * object_num_handles;
  const size_t required_num_handles = 0;
#else
  const size_t required_num_bytes = sizeof(ObjectHeader) + object_num_bytes;
  const size_t required_num_handles = object_num_handles;
#endif
  const size_t data_capacity = num_bytes ? *num_bytes : 0;
  const size_t handle_capacity = num_handles ? *num_handles : 0;
  if (num_bytes) {
    *num_bytes = required_num_bytes;
  }
  if (num_handles) {
    *num_handles = required_num_handles;
  }
  if (data_capacity < required_num_bytes ||
      handle_capacity < required_num_handles) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  auto& header = *static_cast<ObjectHeader*>(data);
  header.size = sizeof(header);
  header.type = object.type();
#if BUILDFLAG(IS_WIN)
  header.num_handles = object_num_handles;
  header.reserved = 0;
  auto handle_data = base::make_span(reinterpret_cast<HANDLE*>(&header + 1),
                                     object_num_handles);
  auto object_data = base::make_span(reinterpret_cast<uint8_t*>(&header + 1) +
                                         object_num_handles * sizeof(HANDLE),
                                     object_num_bytes);
#else
  auto object_data = base::make_span(reinterpret_cast<uint8_t*>(&header + 1),
                                     object_num_bytes);
#endif

  // A small amount of stack storage is reserved to avoid heap allocation in the
  // most common cases.
  base::StackVector<PlatformHandle, 2> platform_handles;
  platform_handles->resize(object_num_handles);
  if (!object.Serialize(*this, object_data,
                        base::make_span(platform_handles.container()))) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  for (size_t i = 0; i < object_num_handles; ++i) {
#if BUILDFLAG(IS_WIN)
    EncodeHandle(platform_handles[i], remote_process_, destination_,
                 handle_data[i]);
#else
    handles[i] = TransmissiblePlatformHandle::ReleaseAsHandle(
        base::MakeRefCounted<TransmissiblePlatformHandle>(
            std::move(platform_handles[i])));
#endif
  }
  return IPCZ_RESULT_OK;
}

IpczResult Transport::DeserializeObject(
    base::span<const uint8_t> bytes,
    base::span<const IpczDriverHandle> handles,
    scoped_refptr<ObjectBase>& object) {
  if (bytes.size() < sizeof(ObjectHeader)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const auto& header = *reinterpret_cast<const ObjectHeader*>(bytes.data());
  const uint32_t header_size = header.size;
  if (header_size < sizeof(ObjectHeader) || header_size > bytes.size()) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

#if BUILDFLAG(IS_WIN)
  DCHECK(handles.empty());
  size_t num_handles = header.num_handles;

  size_t available_bytes = bytes.size() - header_size;
  const size_t max_handles = available_bytes / sizeof(HANDLE);
  if (num_handles > max_handles) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const size_t handle_data_size = num_handles * sizeof(HANDLE);
  auto handle_data = base::make_span(
      reinterpret_cast<const HANDLE*>(bytes.data() + header_size), num_handles);
  auto object_data = bytes.subspan(header_size + handle_data_size);
#else
  auto object_data = bytes.subspan(header_size);
  size_t num_handles = handles.size();
#endif

  // A small amount of stack storage is reserved to avoid heap allocation in the
  // most common cases.
  base::StackVector<PlatformHandle, 2> platform_handles;
  platform_handles->resize(num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
#if BUILDFLAG(IS_WIN)
    platform_handles[i] =
        DecodeHandle(handle_data[i], remote_process_, destination_);
#else
    platform_handles[i] =
        TransmissiblePlatformHandle::TakeFromHandle(handles[i])->TakeHandle();
#endif
    if (!platform_handles[i].is_valid()) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }
  }

  auto object_handles = base::make_span(platform_handles.container());
  switch (header.type) {
    case ObjectBase::kTransport:
      object = Deserialize(*this, object_data, object_handles);
      break;

    case ObjectBase::kSharedBuffer:
      object = SharedBuffer::Deserialize(object_data, object_handles);
      break;

    case ObjectBase::kTransmissiblePlatformHandle:
      object =
          TransmissiblePlatformHandle::Deserialize(object_data, object_handles);
      break;

    case ObjectBase::kWrappedPlatformHandle:
      object = WrappedPlatformHandle::Deserialize(object_data, object_handles);
      break;

    case ObjectBase::kDataPipe:
      object = DataPipe::Deserialize(object_data, object_handles);
      break;

    default:
      return IPCZ_RESULT_UNIMPLEMENTED;
  }

  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  return IPCZ_RESULT_OK;
}

void Transport::Close() {
  Deactivate();
}

bool Transport::IsSerializable() const {
  return true;
}

bool Transport::GetSerializedDimensions(Transport& transmitter,
                                        size_t& num_bytes,
                                        size_t& num_handles) {
  num_bytes = sizeof(TransportHeader);
#if BUILDFLAG(IS_WIN)
  num_handles = ShouldSerializeProcessHandle(transmitter) ? 2 : 1;
#else
  num_handles = 1;
#endif
  return true;
}

bool Transport::Serialize(Transport& transmitter,
                          base::span<uint8_t> data,
                          base::span<PlatformHandle> handles) {
  DCHECK_EQ(sizeof(TransportHeader), data.size());
  auto& header = *reinterpret_cast<TransportHeader*>(data.data());
  header.destination = destination_;
  header.is_same_remote_process = remote_process_.is_current();

#if BUILDFLAG(IS_WIN)
  if (ShouldSerializeProcessHandle(transmitter)) {
    DCHECK_EQ(handles.size(), 2u);
    DCHECK(remote_process_.IsValid());
    DCHECK(!remote_process_.is_current());
    handles[1] = PlatformHandle(
        base::win::ScopedHandle(remote_process_.Duplicate().Release()));
  } else {
    DCHECK_EQ(handles.size(), 1u);
  }
#else
  DCHECK_EQ(handles.size(), 1u);
#endif

  DCHECK(IsEndpointValid());
  DCHECK(absl::holds_alternative<PlatformChannelEndpoint>(inactive_endpoint_));
  handles[0] = absl::get<PlatformChannelEndpoint>(inactive_endpoint_)
                   .TakePlatformHandle();
  return true;
}

// static
scoped_refptr<Transport> Transport::Deserialize(
    Transport& from_transport,
    base::span<const uint8_t> data,
    base::span<PlatformHandle> handles) {
  if (data.size() < sizeof(TransportHeader) || handles.size() < 1) {
    return nullptr;
  }

  base::Process process;
  const auto& header = *reinterpret_cast<const TransportHeader*>(data.data());
#if BUILDFLAG(IS_WIN)
  if (handles.size() >= 2 && from_transport.remote_process().IsValid()) {
    process = base::Process(handles[1].ReleaseHandle());
  }
#endif
  if (header.is_same_remote_process &&
      from_transport.remote_process().IsValid()) {
    process = from_transport.remote_process().Duplicate();
  }
  return base::MakeRefCounted<Transport>(
      header.destination, PlatformChannelEndpoint(std::move(handles[0])),
      std::move(process));
}

bool Transport::IsIpczTransport() const {
  return true;
}

void Transport::OnChannelMessage(const void* payload,
                                 size_t payload_size,
                                 std::vector<PlatformHandle> handles) {
  std::vector<IpczDriverHandle> driver_handles(handles.size());
  for (size_t i = 0; i < handles.size(); ++i) {
    driver_handles[i] = TransmissiblePlatformHandle::ReleaseAsHandle(
        base::MakeRefCounted<TransmissiblePlatformHandle>(
            std::move(handles[i])));
  }

  const IpczResult result = activity_handler_(
      ipcz_transport_, static_cast<const uint8_t*>(payload), payload_size,
      driver_handles.data(), driver_handles.size(), IPCZ_NO_FLAGS, nullptr);
  if (result != IPCZ_RESULT_OK && result != IPCZ_RESULT_UNIMPLEMENTED) {
    OnChannelError(Channel::Error::kReceivedMalformedData);
  }
}

void Transport::OnChannelError(Channel::Error error) {
  activity_handler_(ipcz_transport_, nullptr, 0, nullptr, 0,
                    IPCZ_TRANSPORT_ACTIVITY_ERROR, nullptr);
}

void Transport::OnChannelDestroyed() {
  activity_handler_(ipcz_transport_, nullptr, 0, nullptr, 0,
                    IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr);

  // Drop our self-reference now that the Channel is definitely done calling us.
  // May delete `this` as the stack unwinds.
  scoped_refptr<Transport> self;
  base::AutoLock lock(lock_);
  self = std::move(self_reference_for_channel_);
}

bool Transport::IsEndpointValid() const {
  return absl::visit(base::Overloaded{
                         [](const PlatformChannelEndpoint& endpoint) {
                           return endpoint.is_valid();
                         },
                         [](const PlatformChannelServerEndpoint& endpoint) {
                           return endpoint.is_valid();
                         },
                     },
                     inactive_endpoint_);
}

bool Transport::CanTransmitHandles() const {
#if BUILDFLAG(IS_WIN)
  // On Windows, only transports with a broker on one end may transmit handles.
  return remote_process_.IsValid() || destination_ == kToBroker;
#else
  return true;
#endif
}

bool Transport::ShouldSerializeProcessHandle(Transport& transmitter) const {
#if BUILDFLAG(IS_WIN)
  return remote_process_.IsValid() && !remote_process_.is_current() &&
         transmitter.destination() == kToBroker;
#else
  // We have no need for the process handle on other platforms.
  return false;
#endif
}

Transport::PendingTransmission::PendingTransmission() = default;

Transport::PendingTransmission::PendingTransmission(PendingTransmission&&) =
    default;

Transport::PendingTransmission& Transport::PendingTransmission::operator=(
    PendingTransmission&&) = default;

Transport::PendingTransmission::~PendingTransmission() = default;

}  // namespace mojo::core::ipcz_driver
