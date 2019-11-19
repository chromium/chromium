// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/node_channel.h"

#include <cstring>
#include <limits>
#include <sstream>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
#if defined(OS_WIN)
  RELAY_EVENT_MESSAGE,
#endif
  BROADCAST_EVENT,
#if defined(OS_WIN)
  EVENT_MESSAGE_FROM_RELAY,
#endif
  ACCEPT_PEER,
  BIND_BROKER_HOST,
};

struct Header {
  MessageType type;
  uint32_t padding;
};

static_assert(IsAlignedForChannelMessage(sizeof(Header)),
              "Invalid header size.");

struct AcceptInviteeData {
  ports::NodeName inviter_name;
  ports::NodeName token;
};

struct AcceptInvitationData {
  ports::NodeName token;
  ports::NodeName invitee_name;
};

struct AcceptPeerData {
  ports::NodeName token;
  ports::NodeName peer_name;
  ports::PortName port_name;
};

// This message may include a process handle on plaforms that require it.
struct AddBrokerClientData {
  ports::NodeName client_name;
#if !defined(OS_WIN)
  uint32_t process_handle;
  uint32_t padding;
#endif
};

#if !defined(OS_WIN)
static_assert(sizeof(base::ProcessHandle) == sizeof(uint32_t),
              "Unexpected pid size");
static_assert(sizeof(AddBrokerClientData) % kChannelMessageAlignment == 0,
              "Invalid AddBrokerClientData size.");
#endif

// This data is followed by a platform channel handle to the broker.
struct BrokerClientAddedData {
  ports::NodeName client_name;
};

// This data may be followed by a platform channel handle to the broker. If not,
// then the inviter is the broker and its channel should be used as such.
struct AcceptBrokerClientData {
  ports::NodeName broker_name;
};

// This is followed by arbitrary payload data which is interpreted as a token
// string for port location.
struct RequestPortMergeData {
  ports::PortName connector_port_name;
};

// Used for both REQUEST_INTRODUCTION and INTRODUCE.
//
// For INTRODUCE the message also includes a valid platform handle for a channel
// the receiver may use to communicate with the named node directly, or an
// invalid platform handle if the node is unknown to the sender or otherwise
// cannot be introduced.
struct IntroductionData {
  ports::NodeName name;
};

// This message is just a PlatformHandle. The data struct here has only a
// padding field to ensure an aligned, non-zero-length payload.
struct BindBrokerHostData {
  uint64_t padding;
};

#if defined(OS_WIN)
// This struct is followed by the full payload of a message to be relayed.
struct RelayEventMessageData {
  ports::NodeName destination;
};

// This struct is followed by the full payload of a relayed message.
struct EventMessageFromRelayData {
  ports::NodeName source;
};
#endif

template <typename DataType>
Channel::MessagePtr CreateMessage(MessageType type,
                                  size_t payload_size,
                                  size_t num_handles,
                                  DataType** out_data,
                                  size_t capacity = 0) {
  const size_t total_size = payload_size + sizeof(Header);
  if (capacity == 0)
    capacity = total_size;
  else
    capacity = std::max(total_size, capacity);
  auto message =
      std::make_unique<Channel::Message>(capacity, total_size, num_handles);
  Header* header = reinterpret_cast<Header*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_data = reinterpret_cast<DataType*>(&header[1]);
  return message;
}

template <typename DataType>
bool GetMessagePayload(const void* bytes,
                       size_t num_bytes,
                       DataType** out_data) {
  static_assert(sizeof(DataType) > 0, "DataType must have non-zero size.");
  if (num_bytes < sizeof(Header) + sizeof(DataType))
    return false;
  *out_data = reinterpret_cast<const DataType*>(
      static_cast<const char*>(bytes) + sizeof(Header));
  return true;
}

}  // namespace

// static
scoped_refptr<NodeChannel> NodeChannel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    Channel::HandlePolicy channel_handle_policy,
    scoped_refptr<base::TaskRunner> io_task_runner,
    const ProcessErrorCallback& process_error_callback) {
#if defined(OS_NACL_SFI)
  LOG(FATAL) << "Multi-process not yet supported on NaCl-SFI";
  return nullptr;
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
void NodeChannel::GetEventMessageData(Channel::Message* message,
                                      void** data,
                                      size_t* num_data_bytes) {
  // NOTE: OnChannelMessage guarantees that we never accept a Channel::Message
  // with a payload of fewer than |sizeof(Header)| bytes.
  *data = reinterpret_cast<Header*>(message->mutable_payload()) + 1;
  *num_data_bytes = message->payload_size() - sizeof(Header);
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
  if (!process_error_callback_.is_null())
    process_error_callback_.Run("Received bad user message: " + error);
}

void NodeChannel::SetRemoteProcessHandle(ScopedProcessHandle process_handle) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(channel_lock_);
    if (channel_)
      channel_->set_remote_process(process_handle.Clone());
  }
  base::AutoLock lock(remote_process_handle_lock_);
  DCHECK(!remote_process_handle_.is_valid());
  CHECK_NE(remote_process_handle_.get(), base::GetCurrentProcessHandle());
  remote_process_handle_ = std::move(process_handle);
}

bool NodeChannel::HasRemoteProcessHandle() {
  base::AutoLock lock(remote_process_handle_lock_);
  return remote_process_handle_.is_valid();
}

ScopedProcessHandle NodeChannel::CloneRemoteProcessHandle() {
  base::AutoLock lock(remote_process_handle_lock_);
  if (!remote_process_handle_.is_valid())
    return ScopedProcessHandle();
  return remote_process_handle_.Clone();
}

void NodeChannel::SetRemoteNodeName(const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  remote_node_name_ = name;
}

void NodeChannel::AcceptInvitee(const ports::NodeName& inviter_name,
                                const ports::NodeName& token) {
  AcceptInviteeData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_INVITEE, sizeof(AcceptInviteeData), 0, &data);
  data->inviter_name = inviter_name;
  data->token = token;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptInvitation(const ports::NodeName& token,
                                   const ports::NodeName& invitee_name) {
  AcceptInvitationData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_INVITATION, sizeof(AcceptInvitationData), 0, &data);
  data->token = token;
  data->invitee_name = invitee_name;
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
                                  ScopedProcessHandle process_handle) {
  AddBrokerClientData* data;
  std::vector<PlatformHandle> handles;
#if defined(OS_WIN)
  handles.emplace_back(base::win::ScopedHandle(process_handle.release()));
#endif
  Channel::MessagePtr message =
      CreateMessage(MessageType::ADD_BROKER_CLIENT, sizeof(AddBrokerClientData),
                    handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
#if !defined(OS_WIN)
  data->process_handle = process_handle.get();
  data->padding = 0;
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
                                     PlatformHandle broker_channel) {
  AcceptBrokerClientData* data;
  std::vector<PlatformHandle> handles;
  if (broker_channel.is_valid())
    handles.emplace_back(std::move(broker_channel));
  Channel::MessagePtr message =
      CreateMessage(MessageType::ACCEPT_BROKER_CLIENT,
                    sizeof(AcceptBrokerClientData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->broker_name = broker_name;
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
                            PlatformHandle channel_handle) {
  IntroductionData* data;
  std::vector<PlatformHandle> handles;
  if (channel_handle.is_valid())
    handles.emplace_back(std::move(channel_handle));
  Channel::MessagePtr message = CreateMessage(
      MessageType::INTRODUCE, sizeof(IntroductionData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->name = name;
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
#if !defined(OS_MACOSX) && !defined(OS_NACL) && !defined(OS_FUCHSIA)
  DCHECK(broker_host_handle.is_valid());
  BindBrokerHostData* data;
  std::vector<PlatformHandle> handles;
  handles.push_back(std::move(broker_host_handle));
  Channel::MessagePtr message =
      CreateMessage(MessageType::BIND_BROKER_HOST, sizeof(BindBrokerHostData),
                    handles.size(), &data);
  data->padding = 0;
  message->SetHandles(std::move(handles));
  WriteChannelMessage(std::move(message));
#endif
}

#if defined(OS_WIN)
void NodeChannel::RelayEventMessage(const ports::NodeName& destination,
                                    Channel::MessagePtr message) {
#if defined(OS_WIN)
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
  // TODO(https://crbug.com/813112): We would like to be able to violate the
  // above stated assumption. We should not leak handles in cases where we
  // outlive the broker, as we may continue existing and eventually accept a new
  // broker invitation.
  std::vector<PlatformHandleInTransit> handles = message->TakeHandles();
  for (auto& handle : handles)
    handle.TakeHandle().release();

#else
  DCHECK(message->has_mach_ports());

  // On OSX, the handles are extracted from the relayed message and attached to
  // the wrapper. The broker then takes the handles attached to the wrapper and
  // moves them back to the relayed message. This is necessary because the
  // message may contain fds which need to be attached to the outer message so
  // that they can be transferred to the broker.
  std::vector<PlatformHandleInTransit> handles = message->TakeHandles();
  size_t num_bytes = sizeof(RelayEventMessageData) + message->data_num_bytes();
  RelayEventMessageData* data;
  Channel::MessagePtr relay_message = CreateMessage(
      MessageType::RELAY_EVENT_MESSAGE, num_bytes, handles.size(), &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());
  relay_message->SetHandles(std::move(handles));
#endif  // defined(OS_WIN)

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
#endif  // defined(OS_WIN)

NodeChannel::NodeChannel(Delegate* delegate,
                         ConnectionParams connection_params,
                         Channel::HandlePolicy channel_handle_policy,
                         scoped_refptr<base::TaskRunner> io_task_runner,
                         const ProcessErrorCallback& process_error_callback)
    : delegate_(delegate),
      io_task_runner_(io_task_runner),
      process_error_callback_(process_error_callback)
#if !defined(OS_NACL_SFI)
      ,
      channel_(Channel::Create(this,
                               std::move(connection_params),
                               channel_handle_policy,
                               io_task_runner_))
#endif
{
}

NodeChannel::~NodeChannel() {
  ShutDown();
}

void NodeChannel::CreateAndBindLocalBrokerHost(
    PlatformHandle broker_host_handle) {
#if !defined(OS_MACOSX) && !defined(OS_NACL) && !defined(OS_FUCHSIA)
  // Self-owned.
  ConnectionParams connection_params(
      PlatformChannelEndpoint(std::move(broker_host_handle)));
  new BrokerHost(remote_process_handle_.get(), std::move(connection_params),
                 process_error_callback_);
#endif
}

void NodeChannel::OnChannelMessage(const void* payload,
                                   size_t payload_size,
                                   std::vector<PlatformHandle> handles) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  // Ensure this NodeChannel stays alive through the extent of this method. The
  // delegate may have the only other reference to this object and it may choose
  // to drop it here in response to, e.g., a malformed message.
  scoped_refptr<NodeChannel> keepalive = this;

  if (payload_size <= sizeof(Header)) {
    delegate_->OnChannelError(remote_node_name_, this);
    return;
  }

  const Header* header = static_cast<const Header*>(payload);
  switch (header->type) {
    case MessageType::ACCEPT_INVITEE: {
      const AcceptInviteeData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptInvitee(remote_node_name_, data->inviter_name,
                                   data->token);
        return;
      }
      break;
    }

    case MessageType::ACCEPT_INVITATION: {
      const AcceptInvitationData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptInvitation(remote_node_name_, data->token,
                                      data->invitee_name);
        return;
      }
      break;
    }

    case MessageType::ADD_BROKER_CLIENT: {
      const AddBrokerClientData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
#if defined(OS_WIN)
        if (handles.size() != 1) {
          DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
          break;
        }
        delegate_->OnAddBrokerClient(remote_node_name_, data->client_name,
                                     handles[0].ReleaseHandle());
#else
        if (!handles.empty()) {
          DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
          break;
        }
        delegate_->OnAddBrokerClient(remote_node_name_, data->client_name,
                                     data->process_handle);
#endif
        return;
      }
      break;
    }

    case MessageType::BROKER_CLIENT_ADDED: {
      const BrokerClientAddedData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        if (handles.size() != 1) {
          DLOG(ERROR) << "Dropping invalid BrokerClientAdded message.";
          break;
        }
        delegate_->OnBrokerClientAdded(remote_node_name_, data->client_name,
                                       std::move(handles[0]));
        return;
      }
      break;
    }

    case MessageType::ACCEPT_BROKER_CLIENT: {
      const AcceptBrokerClientData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        PlatformHandle broker_channel;
        if (handles.size() > 1) {
          DLOG(ERROR) << "Dropping invalid AcceptBrokerClient message.";
          break;
        }
        if (handles.size() == 1)
          broker_channel = std::move(handles[0]);

        delegate_->OnAcceptBrokerClient(remote_node_name_, data->broker_name,
                                        std::move(broker_channel));
        return;
      }
      break;
    }

    case MessageType::EVENT_MESSAGE: {
      Channel::MessagePtr message(
          new Channel::Message(payload_size, handles.size()));
      message->SetHandles(std::move(handles));
      memcpy(message->mutable_payload(), payload, payload_size);
      delegate_->OnEventMessage(remote_node_name_, std::move(message));
      return;
    }

    case MessageType::REQUEST_PORT_MERGE: {
      const RequestPortMergeData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        // Don't accept an empty token.
        size_t token_size = payload_size - sizeof(*data) - sizeof(Header);
        if (token_size == 0)
          break;
        std::string token(reinterpret_cast<const char*>(data + 1), token_size);
        delegate_->OnRequestPortMerge(remote_node_name_,
                                      data->connector_port_name, token);
        return;
      }
      break;
    }

    case MessageType::REQUEST_INTRODUCTION: {
      const IntroductionData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnRequestIntroduction(remote_node_name_, data->name);
        return;
      }
      break;
    }

    case MessageType::INTRODUCE: {
      const IntroductionData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        if (handles.size() > 1) {
          DLOG(ERROR) << "Dropping invalid introduction message.";
          break;
        }
        PlatformHandle channel_handle;
        if (handles.size() == 1)
          channel_handle = std::move(handles[0]);

        delegate_->OnIntroduce(remote_node_name_, data->name,
                               std::move(channel_handle));
        return;
      }
      break;
    }

#if defined(OS_WIN)
    case MessageType::RELAY_EVENT_MESSAGE: {
      base::ProcessHandle from_process;
      {
        base::AutoLock lock(remote_process_handle_lock_);
        // NOTE: It's safe to retain a weak reference to this process handle
        // through the extent of this call because |this| is kept alive and
        // |remote_process_handle_| is never reset once set.
        from_process = remote_process_handle_.get();
      }
      const RelayEventMessageData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        // Don't try to relay an empty message.
        if (payload_size <= sizeof(Header) + sizeof(RelayEventMessageData))
          break;

        const void* message_start = data + 1;
        Channel::MessagePtr message = Channel::Message::Deserialize(
            message_start, payload_size - sizeof(Header) - sizeof(*data),
            from_process);
        if (!message) {
          DLOG(ERROR) << "Dropping invalid relay message.";
          break;
        }
        delegate_->OnRelayEventMessage(remote_node_name_, from_process,
                                       data->destination, std::move(message));
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
          Channel::Message::Deserialize(data, payload_size - sizeof(Header));
      if (!message || message->has_handles()) {
        DLOG(ERROR) << "Dropping invalid broadcast message.";
        break;
      }
      delegate_->OnBroadcast(remote_node_name_, std::move(message));
      return;
    }

#if defined(OS_WIN)
    case MessageType::EVENT_MESSAGE_FROM_RELAY: {
      const EventMessageFromRelayData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        size_t num_bytes = payload_size - sizeof(*data);
        if (num_bytes < sizeof(Header))
          break;
        num_bytes -= sizeof(Header);

        Channel::MessagePtr message(
            new Channel::Message(num_bytes, handles.size()));
        message->SetHandles(std::move(handles));
        if (num_bytes)
          memcpy(message->mutable_payload(), data + 1, num_bytes);
        delegate_->OnEventMessageFromRelay(remote_node_name_, data->source,
                                           std::move(message));
        return;
      }
      break;
    }
#endif  // defined(OS_WIN)

    case MessageType::ACCEPT_PEER: {
      const AcceptPeerData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptPeer(remote_node_name_, data->token, data->peer_name,
                                data->port_name);
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

  DLOG(ERROR) << "Received invalid message. Closing channel.";
  if (process_error_callback_)
    process_error_callback_.Run("NodeChannel received a malformed message");
  delegate_->OnChannelError(remote_node_name_, this);
}

void NodeChannel::OnChannelError(Channel::Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  ShutDown();

  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Channel received a malformed message");
  }

  // |OnChannelError()| may cause |this| to be destroyed, but still need access
  // to the name after that destruction. So make a copy of
  // |remote_node_name_| so it can be used if |this| becomes destroyed.
  ports::NodeName node_name = remote_node_name_;
  delegate_->OnChannelError(node_name, this);
}

void NodeChannel::WriteChannelMessage(Channel::MessagePtr message) {
  // Force a crash if this process attempts to send a message larger than the
  // maximum allowed size. This is more useful than killing a Channel when we
  // *receive* an oversized message, as we should consider oversized message
  // transmission to be a bug and this helps easily identify offending code.
  CHECK_LT(message->data_num_bytes(), GetConfiguration().max_message_num_bytes);

  base::AutoLock lock(channel_lock_);
  if (!channel_)
    DLOG(ERROR) << "Dropping message on closed channel.";
  else
    channel_->Write(std::move(message));
}

}  // namespace core
}  // namespace mojo
