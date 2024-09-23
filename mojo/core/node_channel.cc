// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/node_channel.h"

#include <cstring>
#include <limits>
#include <sstream>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/broker_host.h"
#include "mojo/core/channel.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/request_context.h"

namespace mojo {
namespace core {

namespace {

// NOTE: Please ONLY append messages to the end of this enum.
enum class MessageType : uint32_t {
  ACCEPT_INVITEE,
  ACCEPT_INVITATION,
  ADD_BROKER_CLIENT,
  BROKER_CLIENT_ADDED,
  ACCEPT_BROKER_CLIENT,
  EVENT_MESSAGE,
  REQUEST_PORT_MERGE,
  REQUEST_INTRODUCTION,
  INTRODUCE,
#if BUILDFLAG(IS_WIN)
  RELAY_EVENT_MESSAGE,
#endif
  BROADCAST_EVENT,
#if BUILDFLAG(IS_WIN)
  EVENT_MESSAGE_FROM_RELAY,
#endif
  ACCEPT_PEER,
  BIND_BROKER_HOST,
};

#pragma pack(push, 1)

struct alignas(8) Header {
  MessageType type;
};

static_assert(sizeof(Header) == kNodeChannelHeaderSize);

static_assert(IsAlignedForChannelMessage(sizeof(Header)),
              "Invalid header size.");

struct alignas(8) AcceptInviteeDataV0 {
  ports::NodeName inviter_name;
  ports::NodeName token;
};

struct alignas(8) AcceptInviteeDataV1 : AcceptInviteeDataV0 {
  uint64_t capabilities = kNodeCapabilityNone;
};

using AcceptInviteeData = AcceptInviteeDataV1;

struct alignas(8) AcceptInvitationDataV0 {
  ports::NodeName token;
  ports::NodeName invitee_name;
};

struct alignas(8) AcceptInvitationDataV1 : AcceptInvitationDataV0 {
  uint64_t capabilities = kNodeCapabilityNone;
};

using AcceptInvitationData = AcceptInvitationDataV1;

struct alignas(8) AcceptPeerDataV0 {
  ports::NodeName token;
  ports::NodeName peer_name;
  ports::PortName port_name;
};

using AcceptPeerData = AcceptPeerDataV0;

// This message may include a process handle on platforms that require it.
struct alignas(8) AddBrokerClientDataV0 {
  ports::NodeName client_name;
#if !BUILDFLAG(IS_WIN)
  uint32_t process_handle;
#endif
};

using AddBrokerClientData = AddBrokerClientDataV0;

#if !BUILDFLAG(IS_WIN)
static_assert(sizeof(base::ProcessHandle) == sizeof(uint32_t),
              "Unexpected pid size");
static_assert(sizeof(AddBrokerClientData) % kChannelMessageAlignment == 0,
              "Invalid AddBrokerClientData size.");
#endif

// This data is followed by a platform channel handle to the broker.
struct alignas(8) BrokerClientAddedDataV0 {
  ports::NodeName client_name;
};

using BrokerClientAddedData = BrokerClientAddedDataV0;

// This data may be followed by a platform channel handle to the broker. If not,
// then the inviter is the broker and its channel should be used as such.
struct alignas(8) AcceptBrokerClientDataV0 {
  ports::NodeName broker_name;
};

struct alignas(8) AcceptBrokerClientDataV1 : AcceptBrokerClientDataV0 {
  uint64_t capabilities = kNodeCapabilityNone;
  uint64_t broker_capabilities = kNodeCapabilityNone;
};

using AcceptBrokerClientData = AcceptBrokerClientDataV1;

// This is followed by arbitrary payload data which is interpreted as a token
// string for port location.
// NOTE: Because this field is variable length it cannot be versioned.
struct alignas(8) RequestPortMergeData {
  ports::PortName connector_port_name;
};

// Used for both REQUEST_INTRODUCTION and INTRODUCE.
//
// For INTRODUCE the message also includes a valid platform handle for a
// channel the receiver may use to communicate with the named node directly,
// or an invalid platform handle if the node is unknown to the sender or
// otherwise cannot be introduced.
struct alignas(8) IntroductionDataV0 {
  ports::NodeName name;
};

struct alignas(8) IntroductionDataV1 : IntroductionDataV0 {
  uint64_t capabilities = kNodeCapabilityNone;
};

using IntroductionData = IntroductionDataV1;

// This message is just a PlatformHandle. The data struct alignas(8) here has
// only a padding field to ensure an aligned, non-zero-length payload.
struct alignas(8) BindBrokerHostDataV0 {};

using BindBrokerHostData = BindBrokerHostDataV0;

#if BUILDFLAG(IS_WIN)
// This struct alignas(8) is followed by the full payload of a message to be
// relayed.
// NOTE: Because this field is variable length it cannot be versioned.
struct alignas(8) RelayEventMessageData {
  ports::NodeName destination;
};

// This struct alignas(8) is followed by the full payload of a relayed
// message.
struct alignas(8) EventMessageFromRelayDataV0 {
  ports::NodeName source;
};

using EventMessageFromRelayData = EventMessageFromRelayDataV0;

#endif

#pragma pack(pop)

Channel::MessagePtr CreateMessage(MessageType type,
                                  size_t payload_size,
                                  size_t num_handles,
                                  void** out_data,
                                  size_t capacity = 0) {
  const size_t total_size = payload_size + sizeof(Header);
  if (capacity == 0)
    capacity = total_size;
  else
    capacity = std::max(total_size, capacity);
  auto message =
      Channel::Message::CreateMessage(capacity, total_size, num_handles);
  Header* header = reinterpret_cast<Header*>(message->mutable_payload());

  // Make sure any header padding gets zeroed.
  memset(header, 0, sizeof(Header));
  header->type = type;

  // The out_data starts beyond the header.
  *out_data = reinterpret_cast<void*>(header + 1);
  return message;
}

template <typename DataType>
Channel::MessagePtr CreateMessage(MessageType type,
                                  size_t payload_size,
                                  size_t num_handles,
                                  DataType** out_data,
                                  size_t capacity = 0) {
  auto msg_ptr = CreateMessage(type, payload_size, num_handles,
                               reinterpret_cast<void**>(out_data), capacity);

  // Since we know the type let's make sure any padding areas are zeroed.
  memset(*out_data, 0, sizeof(DataType));

  return msg_ptr;
}

// This method takes a second template argument which is another datatype
// which represents the smallest size this payload can be to be considered
// valid this MUST be used when there is more than one version of a message to
// specify the oldest version of the message.
template <typename DataType, typename MinSizedDataType>
bool GetMessagePayloadMinimumSized(const void* bytes,
                                   size_t num_bytes,
                                   DataType* out_data) {
  static_assert(sizeof(DataType) > 0, "DataType must have non-zero size.");
  if (num_bytes < sizeof(Header) + sizeof(MinSizedDataType)) {
    return false;
  }

  // Always make sure that the full object is zeored and default constructed
  // as we may not have the complete type. The default construction allows
  // fields to be default initialized to be resilient to older message
  // versions.
  memset(out_data, 0, sizeof(*out_data));
  new (out_data) DataType;

  // Overwrite any fields we received.
  memcpy(out_data, static_cast<const uint8_t*>(bytes) + sizeof(Header),
         std::min(sizeof(DataType), num_bytes - sizeof(Header)));
  return true;
}

template <typename DataType>
bool GetMessagePayload(const void* bytes,
                       size_t num_bytes,
                       DataType* out_data) {
  return GetMessagePayloadMinimumSized<DataType, DataType>(bytes, num_bytes,
                                                           out_data);
}

}  // namespace

// static
scoped_refptr<NodeChannel> NodeChannel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    Channel::HandlePolicy channel_handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const ProcessErrorCallback& process_error_callback) {
#if BUILDFLAG(IS_NACL)
  LOG(FATAL) << "Multi-process not yet supported on NaCl-SFI";
#else
  return new NodeChannel(delegate, std::move(connection_params),
                         channel_handle_policy, io_task_runner,
                         process_error_callback);
#endif
}

// static
Channel::MessagePtr NodeChannel::CreateEventMessage(size_t capacity,
                                                    size_t payload_size,
                                                    void** payload,
                                                    size_t num_handles) {
  return CreateMessage(MessageType::EVENT_MESSAGE, payload_size, num_handles,
                       payload, capacity);
}

// static
bool NodeChannel::GetEventMessageData(Channel::Message& message,
                                      void** data,
                                      size_t* num_data_bytes) {
  // NOTE: Callers must guarantee that the payload in `message` must be at least
  // large enough to hold a Header.
  if (message.payload_size() < sizeof(Header))
    return false;
  *data = reinterpret_cast<Header*>(message.mutable_payload()) + 1;
  *num_data_bytes = message.payload_size() - sizeof(Header);
  return true;
}

void NodeChannel::Start() {
  base::AutoLock lock(channel_lock_);
  // ShutDown() may have already been called, in which case |channel_| is null.
  if (channel_)
    channel_->Start();
}

void NodeChannel::ShutDown() {
  base::AutoLock lock(channel_lock_);
  if (channel_) {
    channel_->ShutDown();
    channel_ = nullptr;
  }
}

void NodeChannel::LeakHandleOnShutdown() {
  base::AutoLock lock(channel_lock_);
  if (channel_) {
    channel_->LeakHandle();
  }
}

void NodeChannel::NotifyBadMessage(const std::string& error) {
  DCHECK(HasBadMessageHandler());
  process_error_callback_.Run("Received bad user message: " + error);
}

void NodeChannel::SetRemoteProcessHandle(base::Process process_handle) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(channel_lock_);
    if (channel_)
      channel_->set_remote_process(process_handle.Duplicate());
  }
  base::AutoLock lock(remote_process_handle_lock_);
  DCHECK(!remote_process_handle_.IsValid());
  CHECK_NE(remote_process_handle_.Handle(), base::GetCurrentProcessHandle());
  remote_process_handle_ = std::move(process_handle);
}

bool NodeChannel::HasRemoteProcessHandle() {
  base::AutoLock lock(remote_process_handle_lock_);
  return remote_process_handle_.IsValid();
}

base::Process NodeChannel::CloneRemoteProcessHandle() {
  base::AutoLock lock(remote_process_handle_lock_);
  return remote_process_handle_.Duplicate();
}

void NodeChannel::SetRemoteNodeName(const ports::NodeName& name) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  remote_node_name_ = name;
}

void NodeChannel::AcceptInvitee(const ports::NodeName& inviter_name,
                                const ports::NodeName& token) {
  AcceptInviteeData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_INVITEE, sizeof(AcceptInviteeData), 0, &data);
  data->inviter_name = inviter_name;
  data->token = token;
  data->capabilities = local_capabilities_;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptInvitation(const ports::NodeName& token,
                                   const ports::NodeName& invitee_name) {
  AcceptInvitationData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_INVITATION, sizeof(AcceptInvitationData), 0, &data);
  data->token = token;
  data->invitee_name = invitee_name;
  data->capabilities = local_capabilities_;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptPeer(const ports::NodeName& sender_name,
                             const ports::NodeName& token,
                             const ports::PortName& port_name) {
  AcceptPeerData* data;
  Channel::MessagePtr message =
      CreateMessage(MessageType::ACCEPT_PEER, sizeof(AcceptPeerData), 0, &data);
  data->token = token;
  data->peer_name = sender_name;
  data->port_name = port_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AddBrokerClient(const ports::NodeName& client_name,
                                  base::Process process_handle) {
  AddBrokerClientData* data;
  std::vector<PlatformHandle> handles;
#if BUILDFLAG(IS_WIN)
  handles.emplace_back(base::win::ScopedHandle(process_handle.Release()));
#endif
  Channel::MessagePtr message =
      CreateMessage(MessageType::ADD_BROKER_CLIENT, sizeof(AddBrokerClientData),
                    handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
#if !BUILDFLAG(IS_WIN)
  // Older clients treat this as a real process handle, but don't actually need
  // it, so send a valid null handle.
  data->process_handle = base::kNullProcessHandle;
#endif
  WriteChannelMessage(std::move(message));
}

void NodeChannel::BrokerClientAdded(const ports::NodeName& client_name,
                                    PlatformHandle broker_channel) {
  BrokerClientAddedData* data;
  std::vector<PlatformHandle> handles;
  if (broker_channel.is_valid())
    handles.emplace_back(std::move(broker_channel));
  Channel::MessagePtr message =
      CreateMessage(MessageType::BROKER_CLIENT_ADDED,
                    sizeof(BrokerClientAddedData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptBrokerClient(const ports::NodeName& broker_name,
                                     PlatformHandle broker_channel,
                                     const uint64_t broker_capabilities) {
  AcceptBrokerClientData* data;
  std::vector<PlatformHandle> handles;
  if (broker_channel.is_valid())
    handles.emplace_back(std::move(broker_channel));
  Channel::MessagePtr message =
      CreateMessage(MessageType::ACCEPT_BROKER_CLIENT,
                    sizeof(AcceptBrokerClientData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->broker_name = broker_name;
  data->broker_capabilities = broker_capabilities;
  data->capabilities = local_capabilities_;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::RequestPortMerge(const ports::PortName& connector_port_name,
                                   const std::string& token) {
  RequestPortMergeData* data;
  Channel::MessagePtr message =
      CreateMessage(MessageType::REQUEST_PORT_MERGE,
                    sizeof(RequestPortMergeData) + token.size(), 0, &data);
  data->connector_port_name = connector_port_name;
  memcpy(data + 1, token.data(), token.size());
  WriteChannelMessage(std::move(message));
}

void NodeChannel::RequestIntroduction(const ports::NodeName& name) {
  IntroductionData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::REQUEST_INTRODUCTION, sizeof(IntroductionData), 0, &data);
  data->name = name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::Introduce(const ports::NodeName& name,
                            PlatformHandle channel_handle,
                            uint64_t capabilities) {
  IntroductionData* data;
  std::vector<PlatformHandle> handles;
  if (channel_handle.is_valid())
    handles.emplace_back(std::move(channel_handle));
  Channel::MessagePtr message = CreateMessage(
      MessageType::INTRODUCE, sizeof(IntroductionData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->name = name;
  // Note that these are not our capabilities, but the capabilities of the peer
  // we're introducing.
  data->capabilities = capabilities;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::SendChannelMessage(Channel::MessagePtr message) {
  WriteChannelMessage(std::move(message));
}

void NodeChannel::Broadcast(Channel::MessagePtr message) {
  DCHECK(!message->has_handles());
  void* data;
  Channel::MessagePtr broadcast_message = CreateMessage(
      MessageType::BROADCAST_EVENT, message->data_num_bytes(), 0, &data);
  memcpy(data, message->data(), message->data_num_bytes());
  WriteChannelMessage(std::move(broadcast_message));
}

void NodeChannel::BindBrokerHost(PlatformHandle broker_host_handle) {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
  DCHECK(broker_host_handle.is_valid());
  BindBrokerHostData* data;
  std::vector<PlatformHandle> handles;
  handles.push_back(std::move(broker_host_handle));
  Channel::MessagePtr message =
      CreateMessage(MessageType::BIND_BROKER_HOST, sizeof(BindBrokerHostData),
                    handles.size(), &data);
  message->SetHandles(std::move(handles));
  WriteChannelMessage(std::move(message));
#endif
}

#if BUILDFLAG(IS_WIN)
void NodeChannel::RelayEventMessage(const ports::NodeName& destination,
                                    Channel::MessagePtr message) {
  DCHECK(message->has_handles());

  // Note that this is only used on Windows, and on Windows all platform
  // handles are included in the message data. We blindly copy all the data
  // here and the relay node (the broker) will duplicate handles as needed.
  size_t num_bytes = sizeof(RelayEventMessageData) + message->data_num_bytes();
  RelayEventMessageData* data;
  Channel::MessagePtr relay_message =
      CreateMessage(MessageType::RELAY_EVENT_MESSAGE, num_bytes, 0, &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());

  // When the handles are duplicated in the broker, the source handles will
  // be closed. If the broker never receives this message then these handles
  // will leak, but that means something else has probably broken and the
  // sending process won't likely be around much longer.
  //
  // TODO(crbug.com/40563346): We would like to be able to violate the
  // above stated assumption. We should not leak handles in cases where we
  // outlive the broker, as we may continue existing and eventually accept a new
  // broker invitation.
  std::vector<PlatformHandleInTransit> handles = message->TakeHandles();
  for (auto& handle : handles)
    handle.TakeHandle().release();

  WriteChannelMessage(std::move(relay_message));
}

void NodeChannel::EventMessageFromRelay(const ports::NodeName& source,
                                        Channel::MessagePtr message) {
  size_t num_bytes =
      sizeof(EventMessageFromRelayData) + message->payload_size();
  EventMessageFromRelayData* data;
  Channel::MessagePtr relayed_message =
      CreateMessage(MessageType::EVENT_MESSAGE_FROM_RELAY, num_bytes,
                    message->num_handles(), &data);
  data->source = source;
  if (message->payload_size())
    memcpy(data + 1, message->payload(), message->payload_size());
  relayed_message->SetHandles(message->TakeHandles());
  WriteChannelMessage(std::move(relayed_message));
}
#endif  // BUILDFLAG(IS_WIN)

NodeChannel::NodeChannel(
    Delegate* delegate,
    ConnectionParams connection_params,
    Channel::HandlePolicy channel_handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const ProcessErrorCallback& process_error_callback)
    : base::RefCountedDeleteOnSequence<NodeChannel>(io_task_runner),
      delegate_(delegate),
      process_error_callback_(process_error_callback)
#if !BUILDFLAG(IS_NACL)
      ,
      channel_(Channel::Create(this,
                               std::move(connection_params),
                               channel_handle_policy,
                               std::move(io_task_runner)))
#endif
{
  InitializeLocalCapabilities();
}

NodeChannel::~NodeChannel() {
  ShutDown();
}

void NodeChannel::CreateAndBindLocalBrokerHost(
    PlatformHandle broker_host_handle) {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
  // Self-owned.
  ConnectionParams connection_params(
      PlatformChannelEndpoint(std::move(broker_host_handle)));
  new BrokerHost(remote_process_handle_.Duplicate(),
                 std::move(connection_params), process_error_callback_);
#endif
}

void NodeChannel::OnChannelMessage(const void* payload,
                                   size_t payload_size,
                                   std::vector<PlatformHandle> handles) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  if (payload_size <= sizeof(Header)) {
    delegate_->OnChannelError(remote_node_name_, this);
    return;
  }

  const Header* header = static_cast<const Header*>(payload);
  switch (header->type) {
    case MessageType::ACCEPT_INVITEE: {
      AcceptInviteeData data;
      if (GetMessagePayloadMinimumSized<AcceptInviteeData, AcceptInviteeDataV0>(
              payload, payload_size, &data)) {
        // Attach any capabilities that the other side advertised.
        SetRemoteCapabilities(data.capabilities);
        delegate_->OnAcceptInvitee(remote_node_name_, data.inviter_name,
                                   data.token);
        return;
      }
      break;
    }

    case MessageType::ACCEPT_INVITATION: {
      AcceptInvitationData data;
      if (GetMessagePayloadMinimumSized<AcceptInvitationData,
                                        AcceptInvitationDataV0>(
              payload, payload_size, &data)) {
        // Attach any capabilities that the other side advertised.
        SetRemoteCapabilities(data.capabilities);
        delegate_->OnAcceptInvitation(remote_node_name_, data.token,
                                      data.invitee_name);
        return;
      }
      break;
    }

    case MessageType::ADD_BROKER_CLIENT: {
      AddBrokerClientData data;
      if (GetMessagePayload(payload, payload_size, &data)) {
#if BUILDFLAG(IS_WIN)
        if (handles.size() != 1) {
          DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
          break;
        }
        delegate_->OnAddBrokerClient(remote_node_name_, data.client_name,
                                     handles[0].ReleaseHandle());
#else
        if (!handles.empty()) {
          DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
          break;
        }
        delegate_->OnAddBrokerClient(remote_node_name_, data.client_name,
                                     base::kNullProcessHandle);
#endif
        return;
      }
      break;
    }

    case MessageType::BROKER_CLIENT_ADDED: {
      BrokerClientAddedData data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        if (handles.size() != 1) {
          DLOG(ERROR) << "Dropping invalid BrokerClientAdded message.";
          break;
        }
        delegate_->OnBrokerClientAdded(remote_node_name_, data.client_name,
                                       std::move(handles[0]));
        return;
      }
      break;
    }

    case MessageType::ACCEPT_BROKER_CLIENT: {
      AcceptBrokerClientData data;
      if (GetMessagePayloadMinimumSized<AcceptBrokerClientData,
                                        AcceptBrokerClientDataV0>(
              payload, payload_size, &data)) {
        PlatformHandle broker_channel;
        if (handles.size() > 1) {
          DLOG(ERROR) << "Dropping invalid AcceptBrokerClient message.";
          break;
        }
        if (handles.size() == 1)
          broker_channel = std::move(handles[0]);

        // Attach any capabilities that the other side advertised.
        SetRemoteCapabilities(data.capabilities);
        delegate_->OnAcceptBrokerClient(remote_node_name_, data.broker_name,
                                        std::move(broker_channel),
                                        data.broker_capabilities);
        return;
      }
      break;
    }

    case MessageType::EVENT_MESSAGE: {
      Channel::MessagePtr message =
          Channel::Message::CreateMessage(payload_size, handles.size());
      message->SetHandles(std::move(handles));
      memcpy(message->mutable_payload(), payload, payload_size);
      delegate_->OnEventMessage(remote_node_name_, std::move(message));
      return;
    }

    case MessageType::REQUEST_PORT_MERGE: {
      RequestPortMergeData data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        // Don't accept an empty token.
        size_t token_size = payload_size - sizeof(data) - sizeof(Header);
        if (token_size == 0)
          break;
        std::string token(reinterpret_cast<const char*>(payload) +
                              sizeof(Header) + sizeof(data),
                          token_size);
        delegate_->OnRequestPortMerge(remote_node_name_,
                                      data.connector_port_name, token);
        return;
      }
      break;
    }

    case MessageType::REQUEST_INTRODUCTION: {
      IntroductionData data;
      if (GetMessagePayloadMinimumSized<IntroductionData, IntroductionDataV0>(
              payload, payload_size, &data)) {
        delegate_->OnRequestIntroduction(remote_node_name_, data.name);
        return;
      }
      break;
    }

    case MessageType::INTRODUCE: {
      IntroductionData data;
      if (GetMessagePayloadMinimumSized<IntroductionData, IntroductionDataV0>(
              payload, payload_size, &data)) {
        if (handles.size() > 1) {
          DLOG(ERROR) << "Dropping invalid introduction message.";
          break;
        }
        PlatformHandle channel_handle;
        if (handles.size() == 1)
          channel_handle = std::move(handles[0]);

        // The node channel for this introduction will be created later, so we
        // can only pass up the capabilities we received from the broker for
        // that remote.
        delegate_->OnIntroduce(remote_node_name_, data.name,
                               std::move(channel_handle), data.capabilities);
        return;
      }
      break;
    }

#if BUILDFLAG(IS_WIN)
    case MessageType::RELAY_EVENT_MESSAGE: {
      base::ProcessHandle from_process;
      {
        base::AutoLock lock(remote_process_handle_lock_);
        // NOTE: It's safe to retain a weak reference to this process handle
        // through the extent of this call because |this| is kept alive and
        // |remote_process_handle_| is never reset once set.
        from_process = remote_process_handle_.Handle();

        // If we don't have a handle to the remote process, we should not be
        // receiving relay requests from them because we're not the broker.
        if (from_process == base::kNullProcessHandle)
          break;
      }
      RelayEventMessageData data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        // Don't try to relay an empty message.
        if (payload_size <= sizeof(Header) + sizeof(data))
          break;

        const void* message_start = reinterpret_cast<const uint8_t*>(payload) +
                                    sizeof(Header) + sizeof(data);
        Channel::MessagePtr message = Channel::Message::Deserialize(
            message_start, payload_size - sizeof(Header) - sizeof(data),
            Channel::HandlePolicy::kAcceptHandles, from_process);
        if (!message) {
          DLOG(ERROR) << "Dropping invalid relay message.";
          break;
        }
        delegate_->OnRelayEventMessage(remote_node_name_, from_process,
                                       data.destination, std::move(message));
        return;
      }
      break;
    }
#endif

    case MessageType::BROADCAST_EVENT: {
      if (payload_size <= sizeof(Header))
        break;
      const void* data = static_cast<const void*>(
          reinterpret_cast<const Header*>(payload) + 1);
      Channel::MessagePtr message =
          Channel::Message::Deserialize(data, payload_size - sizeof(Header),
                                        Channel::HandlePolicy::kRejectHandles);
      if (!message) {
        DLOG(ERROR) << "Dropping invalid broadcast message.";
        break;
      }
      delegate_->OnBroadcast(remote_node_name_, std::move(message));
      return;
    }

#if BUILDFLAG(IS_WIN)
    case MessageType::EVENT_MESSAGE_FROM_RELAY: {
      EventMessageFromRelayData data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        if (payload_size < (sizeof(Header) + sizeof(data)))
          break;

        size_t num_bytes = payload_size - sizeof(data) - sizeof(Header);

        Channel::MessagePtr message =
            Channel::Message::CreateMessage(num_bytes, handles.size());
        message->SetHandles(std::move(handles));
        if (num_bytes)
          memcpy(message->mutable_payload(),
                 static_cast<const uint8_t*>(payload) + sizeof(Header) +
                     sizeof(data),
                 num_bytes);
        delegate_->OnEventMessageFromRelay(remote_node_name_, data.source,
                                           std::move(message));
        return;
      }
      break;
    }
#endif  // BUILDFLAG(IS_WIN)

    case MessageType::ACCEPT_PEER: {
      AcceptPeerData data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptPeer(remote_node_name_, data.token, data.peer_name,
                                data.port_name);
        return;
      }
      break;
    }

    case MessageType::BIND_BROKER_HOST:
      if (handles.size() == 1) {
        CreateAndBindLocalBrokerHost(std::move(handles[0]));
        return;
      }
      break;

    default:
      // Ignore unrecognized message types, allowing for future extensibility.
      return;
  }

  DLOG(ERROR) << "Received invalid message type: "
              << static_cast<int>(header->type) << " closing channel.";
  if (process_error_callback_)
    process_error_callback_.Run("NodeChannel received a malformed message");
  delegate_->OnChannelError(remote_node_name_, this);
}

void NodeChannel::OnChannelError(Channel::Error error) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  ShutDown();

  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Channel received a malformed message");
  }

  // |OnChannelError()| may cause |this| to be destroyed, but still need
  // access to the name after that destruction. So make a copy of
  // |remote_node_name_| so it can be used if |this| becomes destroyed.
  ports::NodeName node_name = remote_node_name_;
  delegate_->OnChannelError(node_name, this);
}

void NodeChannel::WriteChannelMessage(Channel::MessagePtr message) {
  base::AutoLock lock(channel_lock_);
  if (!channel_)
    DLOG(ERROR) << "Dropping message on closed channel.";
  else
    channel_->Write(std::move(message));
}

void NodeChannel::OfferChannelUpgrade() {
#if !BUILDFLAG(IS_NACL)
  base::AutoLock lock(channel_lock_);
  channel_->OfferChannelUpgrade();
#endif
}

uint64_t NodeChannel::RemoteCapabilities() const {
  return remote_capabilities_;
}

bool NodeChannel::HasRemoteCapability(const uint64_t capability) const {
  return (remote_capabilities_ & capability) == capability;
}

void NodeChannel::SetRemoteCapabilities(const uint64_t capabilities) {
  remote_capabilities_ |= capabilities;
}

uint64_t NodeChannel::LocalCapabilities() const {
  return local_capabilities_;
}

bool NodeChannel::HasLocalCapability(const uint64_t capability) const {
  return (local_capabilities_ & capability) == capability;
}

void NodeChannel::SetLocalCapabilities(const uint64_t capabilities) {
  if (GetConfiguration().dont_advertise_capabilities) {
    return;
  }

  local_capabilities_ |= capabilities;
}

void NodeChannel::InitializeLocalCapabilities() {
  if (GetConfiguration().dont_advertise_capabilities) {
    return;
  }

  if (core::Channel::SupportsChannelUpgrade()) {
    SetLocalCapabilities(kNodeCapabilitySupportsUpgrade);
  }
}

}  // namespace core
}  // namespace mojo
