// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/transport.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/overloaded.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "mojo/core/ipcz_driver/invitation.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

#if BUILDFLAG(IS_WIN)
#include "mojo/public/cpp/platform/platform_handle_security_util_win.h"
#endif

namespace mojo::core::ipcz_driver {

namespace {

#if BUILDFLAG(IS_WIN)
// Objects encode their Windows handles as 64-bit HANDLE values within messages.
//
// Windows does not provide any out-of-band mechanism for transmitting objects
// between processes (such as sendmsg() ancillary data on POSIX). Instead there
// is only the DuplicateHandle() API which allows processes to clone object
// handles to or from each others' processes. Therefore every handle
// transmission requires at least one end of the Transport to have sufficient
// access rights and resources to use DuplicateHandle() in reference to handles
// in the other endpoint's process. Handles may be transmitted either as valid
// values from the sender's own process; or as valid values in the recipient's
// process, already duplicated there by the sender.
//
// In general senders always send handles owned by the recipient if capable of
// doing so. If sent handles are owned by the sender, they can only be decoded
// if the recipient itself is sufficiently privileged and capable to duplicate
// handles from the sending process.
//
// This enumeration indicates whether handle values encoded by a serialized
// object belong to the sender or the recipient.
enum HandleOwner : uint8_t {
  // Encoded HANDLEs belong to the sending process. For the recipient to use
  // these, they must have a handle to the sending process with access rights to
  // duplicate handles from there.
  kSender,

  // Encoded HANDLEs belong to the recipient's process. The recipient can use
  // these handles as-is. Only brokers should be trusted to send handles that
  // already belong to the recipient.
  kRecipient,
};

// HANDLE value size varies by architecture. We always encode them with 64 bits.
using HandleData = uint64_t;

HandleData HandleToData(HANDLE handle) {
  return static_cast<HandleData>(reinterpret_cast<uintptr_t>(handle));
}

HANDLE DataToHandle(HandleData data) {
  return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(data));
}
#endif

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

  // Indicates how the handles are encoded for this object.
  HandleOwner handle_owner;

  // Padding for 8-byte size alignment.
  uint8_t reserved[3];
#endif
};

// Header for a serialized Transport object.
struct IPCZ_ALIGN(8) TransportHeader {
  // Indicates what type of destination the other end of this serialized
  // transport is connected to.
  Transport::EndpointType destination_type;

  // Indicates whether the remote process on the other end of this transport
  // is the same process sending this object.
  bool is_same_remote_process;

  // See notes on equivalent fields defined on Transport. Note that serialized
  // transports endpoints with `is_peer_trusted` set to true can only be
  // accepted from transports which are themselves trusted.
  bool is_peer_trusted;
  bool is_trusted_by_peer;
};

#if BUILDFLAG(IS_WIN)
// Encodes a Windows HANDLE value for transmission within a serialized driver
// object payload. See documentation on HandleOwner above for general notes
// about how handles are communicated over IPC on Windows. Returns true on
// success, with the encoded handle value in `out_handle_data`. Returns false if
// handle duplication failed.
bool EncodeHandle(PlatformHandle& handle,
                  const base::Process& remote_process,
                  HandleOwner handle_owner,
                  HandleData& out_handle_data,
                  bool is_remote_process_untrusted) {
  // Duplicating INVALID_HANDLE_VALUE passes a process handle. If you intend to
  // do this, you must open a valid process handle, not pass the result of
  // GetCurrentProcess(). e.g. https://crbug.com/243339.
  CHECK(handle.is_valid());
  if (handle_owner == HandleOwner::kSender) {
    // Nothing to do when sending handles that belong to us. The recipient must
    // be sufficiently privileged and equipped to duplicate such handles to
    // itself.
    out_handle_data = HandleToData(handle.ReleaseHandle());
    return true;
  }

  // To encode a handle that already belongs to the recipient, we must first
  // duplicate the handle to the recipient's process. Note that it is invalid to
  // call EncodeHandle() to encode kRecipient handles without providing a valid
  // handle to the remote process.
  DCHECK_EQ(handle_owner, HandleOwner::kRecipient);
  DCHECK(remote_process.IsValid());
#if BUILDFLAG(IS_WIN)
  if (is_remote_process_untrusted) {
    DcheckIfFileHandleIsUnsafe(handle.GetHandle().get());
  }
#endif

  HANDLE new_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), handle.ReleaseHandle(),
                         remote_process.Handle(), &new_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
    return false;
  }

  out_handle_data = HandleToData(new_handle);
  return true;
}

// Decodes a Windows HANDLE value from a transmission containing a serialized
// driver object. See documentation on HandleOwner above for general notes about
// how handles are communicated over IPC on Windows.
PlatformHandle DecodeHandle(HandleData data,
                            const base::Process& remote_process,
                            HandleOwner handle_owner,
                            Transport& from_transport) {
  const HANDLE handle = DataToHandle(data);
  if (handle_owner == HandleOwner::kRecipient) {
    if (from_transport.destination_type() != Transport::kBroker &&
        !from_transport.is_peer_trusted() && !remote_process.is_current()) {
      // Do not trust non-broker endpoints to send handles which already belong
      // to us, unless the transport is explicitly marked as trustworthy (e.g.
      // is connected to a known elevated process.)
      return PlatformHandle();
    }
    return PlatformHandle(base::win::ScopedHandle(handle));
  }

  if (!remote_process.IsValid()) {
    return PlatformHandle();
  }

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

Transport::Transport(EndpointTypes endpoint_types,
                     PlatformChannelEndpoint endpoint,
                     base::Process remote_process,
                     bool is_remote_process_untrusted)
    : endpoint_types_(endpoint_types),
      remote_process_(std::move(remote_process)),
#if BUILDFLAG(IS_WIN)
      is_remote_process_untrusted_(is_remote_process_untrusted),
#endif
      inactive_endpoint_(std::move(endpoint)) {
}

// static
scoped_refptr<Transport> Transport::Create(EndpointTypes endpoint_types,
                                           PlatformChannelEndpoint endpoint,
                                           base::Process remote_process,
                                           bool is_remote_process_untrusted) {
  return base::MakeRefCounted<Transport>(endpoint_types, std::move(endpoint),
                                         std::move(remote_process),
                                         is_remote_process_untrusted);
}

// static
std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>>
Transport::CreatePair(EndpointType first_type, EndpointType second_type) {
  PlatformChannel channel;
  auto one = Create({.source = first_type, .destination = second_type},
                    channel.TakeLocalEndpoint());
  auto two = Create({.source = second_type, .destination = first_type},
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

void Transport::OverrideIOTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  io_task_runner_ = std::move(task_runner);
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
    if (channel_ || !inactive_endpoint_.is_valid()) {
      return false;
    }

    ipcz_transport_ = transport;
    activity_handler_ = activity_handler;
    self_reference_for_channel_ = base::WrapRefCounted(this);
    channel_ = Channel::CreateForIpczDriver(this, std::move(inactive_endpoint_),
                                            io_task_runner_);
    if (leak_channel_on_shutdown_) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<Channel> channel) { channel->LeakHandle(); },
              channel_));
    }

    if (!pending_transmissions_.empty()) {
      pending_transmissions_.swap(pending_transmissions);
    }

    channel = channel_;
  }

  // NOTE: Some Channel implementations could re-enter this Transport from
  // within Start(), so it's critical that we don't call it while holding our
  // lock.
  channel->Start();
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
    if (inactive_endpoint_.is_valid()) {
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
                                    sizeof(HandleData) * object_num_handles;
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
  header.reserved[0] = 0;
  header.reserved[1] = 0;
  header.reserved[2] = 0;

  const HandleOwner handle_owner =
      remote_process_.IsValid() &&
              (source_type() == kBroker || is_trusted_by_peer())
          ? HandleOwner::kRecipient
          : HandleOwner::kSender;
  header.handle_owner = handle_owner;

  auto handle_data = base::make_span(reinterpret_cast<HandleData*>(&header + 1),
                                     object_num_handles);
  auto object_data =
      base::make_span(reinterpret_cast<uint8_t*>(&header + 1) +
                          object_num_handles * sizeof(HandleData),
                      object_num_bytes);
#else
  auto object_data = base::make_span(reinterpret_cast<uint8_t*>(&header + 1),
                                     object_num_bytes);
#endif

  // A small amount of stack storage is reserved to avoid heap allocation in the
  // most common cases.
  absl::InlinedVector<PlatformHandle, 2> platform_handles;
  platform_handles.resize(object_num_handles);
  if (!object.Serialize(*this, object_data,
                        base::make_span(platform_handles))) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  bool ok = true;
  for (size_t i = 0; i < object_num_handles; ++i) {
#if BUILDFLAG(IS_WIN)
    ok &= EncodeHandle(platform_handles[i], remote_process_, handle_owner,
                       handle_data[i], is_remote_process_untrusted_);
#else
    handles[i] = TransmissiblePlatformHandle::ReleaseAsHandle(
        base::MakeRefCounted<TransmissiblePlatformHandle>(
            std::move(platform_handles[i])));
#endif
  }
  return ok ? IPCZ_RESULT_OK : IPCZ_RESULT_INVALID_ARGUMENT;
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
  const HandleOwner handle_owner = header.handle_owner;

  size_t available_bytes = bytes.size() - header_size;
  const size_t max_handles = available_bytes / sizeof(HandleData);
  if (num_handles > max_handles) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const size_t handle_data_size = num_handles * sizeof(HandleData);
  auto handle_data = base::make_span(
      reinterpret_cast<const HandleData*>(bytes.data() + header_size),
      num_handles);
  auto object_data = bytes.subspan(header_size + handle_data_size);
#else
  auto object_data = bytes.subspan(header_size);
  size_t num_handles = handles.size();
#endif

  // A small amount of stack storage is reserved to avoid heap allocation in the
  // most common cases.
  absl::InlinedVector<PlatformHandle, 2> platform_handles;
  platform_handles.resize(num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
#if BUILDFLAG(IS_WIN)
    platform_handles[i] =
        DecodeHandle(handle_data[i], remote_process_, handle_owner, *this);
#else
    platform_handles[i] =
        TransmissiblePlatformHandle::TakeFromHandle(handles[i])->TakeHandle();
#endif
    if (!platform_handles[i].is_valid()) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }
  }

  auto object_handles = base::make_span(platform_handles);
  switch (header.type) {
    case ObjectBase::kTransport: {
      object = Deserialize(*this, object_data, object_handles);
      break;
    }

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
  header.destination_type = destination_type();
  header.is_same_remote_process = remote_process_.is_current();
  header.is_peer_trusted = is_peer_trusted();
  header.is_trusted_by_peer = is_trusted_by_peer();

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

  CHECK(inactive_endpoint_.is_valid());
  handles[0] = inactive_endpoint_.TakePlatformHandle();
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
  if (handles.size() >= 2) {
    process = base::Process(handles[1].ReleaseHandle());
  }
#endif
  const bool is_source_trusted = from_transport.is_peer_trusted() ||
                                 from_transport.destination_type() == kBroker;
  const bool is_new_peer_trusted = header.is_peer_trusted;
  if (is_new_peer_trusted && !is_source_trusted) {
    // Untrusted transports cannot send us trusted transports.
    return nullptr;
  }
  if (header.is_same_remote_process &&
      from_transport.remote_process().IsValid()) {
    process = from_transport.remote_process().Duplicate();
  }
  auto transport = Create({.source = from_transport.source_type(),
                           .destination = header.destination_type},
                          PlatformChannelEndpoint(std::move(handles[0])),
                          std::move(process));
  transport->set_is_peer_trusted(is_new_peer_trusted);
  transport->set_is_trusted_by_peer(header.is_trusted_by_peer);

  // Inherit the IO task used by the receiving Transport. Deserialized
  // transports are always adopted by the receiving node, and we want any given
  // node to receive all of its transports' I/O on the same thread.
  transport->OverrideIOTaskRunner(from_transport.io_task_runner_);

  return transport;
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

bool Transport::CanTransmitHandles() const {
#if BUILDFLAG(IS_WIN)
  // On Windows, we can transmit handles only if at least one endpoint is a
  // broker, or if we have a handle to the remote process, or if the both ends
  // of the transport are held by the same process.
  return destination_type() == kBroker || source_type() == kBroker ||
         (remote_process_.IsValid() && is_trusted_by_peer()) ||
         remote_process_.is_current();
#else
  return true;
#endif
}

bool Transport::ShouldSerializeProcessHandle(Transport& transmitter) const {
#if BUILDFLAG(IS_WIN)
  return remote_process_.IsValid() && !remote_process_.is_current() &&
         (transmitter.destination_type() == kBroker ||
          transmitter.is_peer_trusted());
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
