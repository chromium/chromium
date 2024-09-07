// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/node_controller.h"

#include <limits>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/broker.h"
#include "mojo/core/broker_host.h"
#include "mojo/core/configuration.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/ports/port_locker.h"
#include "mojo/core/request_context.h"
#include "mojo/core/user_message_impl.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace core {

namespace {

template <typename T>
void GenerateRandomName(T* out) {
  base::RandBytes(base::byte_span_from_ref(*out));
}

ports::NodeName GetRandomNodeName() {
  ports::NodeName name;
  GenerateRandomName(&name);
  return name;
}

Channel::MessagePtr SerializeEventMessage(ports::ScopedEvent event) {
  if (event->type() == ports::Event::Type::kUserMessage) {
    // User message events must already be partially serialized.
    return UserMessageImpl::FinalizeEventMessage(
        ports::Event::Cast<ports::UserMessageEvent>(&event));
  }

  void* data;
  size_t size = event->GetSerializedSize();
  auto message = NodeChannel::CreateEventMessage(size, size, &data, 0);
  event->Serialize(data);
  return message;
}

ports::ScopedEvent DeserializeEventMessage(
    const ports::NodeName& from_node,
    Channel::MessagePtr channel_message) {
  void* data;
  size_t size;
  bool valid = NodeChannel::GetEventMessageData(*channel_message, &data, &size);
  if (!valid)
    return nullptr;
  auto event = ports::Event::Deserialize(data, size);
  if (!event)
    return nullptr;

  if (event->type() != ports::Event::Type::kUserMessage)
    return event;

  // User messages require extra parsing.
  const size_t event_size = event->GetSerializedSize();

  // Note that if this weren't true, the event couldn't have been deserialized
  // in the first place.
  DCHECK_LE(event_size, size);

  auto message_event = ports::Event::Cast<ports::UserMessageEvent>(&event);
  auto message = UserMessageImpl::CreateFromChannelMessage(
      message_event.get(), std::move(channel_message),
      static_cast<uint8_t*>(data) + event_size, size - event_size);
  if (!message)
    return nullptr;

  message->set_source_node(from_node);
  message_event->AttachMessage(std::move(message));
  return std::move(message_event);
}

// Used by NodeController to watch for shutdown. Since no IO can happen once
// the IO thread is killed, the NodeController can cleanly drop all its peers
// at that time.
class ThreadDestructionObserver
    : public base::CurrentThread::DestructionObserver {
 public:
  static void Create(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::OnceClosure callback) {
    if (task_runner->RunsTasksInCurrentSequence()) {
      // Owns itself.
      new ThreadDestructionObserver(std::move(callback));
    } else {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&Create, task_runner, std::move(callback)));
    }
  }

  ThreadDestructionObserver(const ThreadDestructionObserver&) = delete;
  ThreadDestructionObserver& operator=(const ThreadDestructionObserver&) =
      delete;

 private:
  explicit ThreadDestructionObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {
    base::CurrentThread::Get()->AddDestructionObserver(this);
  }

  ~ThreadDestructionObserver() override {
    base::CurrentThread::Get()->RemoveDestructionObserver(this);
  }

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    std::move(callback_).Run();
    delete this;
  }

  base::OnceClosure callback_;
};

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
std::optional<ConnectionParams> CreateSyncNodeConnectionParams(
    const base::Process& target_process,
    ConnectionParams connection_params,
    const ProcessErrorCallback& process_error_callback,
    Channel::HandlePolicy& handle_policy) {
  ConnectionParams node_connection_params;
  const bool is_untrusted_process = connection_params.is_untrusted_process();

  // BrokerHost owns itself.
  BrokerHost* broker_host = new BrokerHost(
      target_process.IsValid() ? target_process.Duplicate() : base::Process(),
      std::move(connection_params), process_error_callback);

  // Sync connections usurp the passed endpoint and use it for the sync broker
  // channel. A new channel is created here for the NodeChannel and sent over
  // a sync broker message to the client.
  PlatformChannel node_channel;
  node_connection_params = ConnectionParams(node_channel.TakeLocalEndpoint());
  node_connection_params.set_is_untrusted_process(is_untrusted_process);
  if (!broker_host->SendChannel(
          node_channel.TakeRemoteEndpoint().TakePlatformHandle())) {
    return std::nullopt;
  }

  return node_connection_params;
}
#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace

NodeController::~NodeController() = default;

NodeController::NodeController()
    : name_(GetRandomNodeName()), node_(new ports::Node(name_, this)) {
  DVLOG(1) << "Initializing node " << name_;
}

void NodeController::SetIOTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  io_task_runner_ = task_runner;
  ThreadDestructionObserver::Create(
      io_task_runner_,
      base::BindOnce(&NodeController::DropAllPeers, base::Unretained(this)));
}

void NodeController::SendBrokerClientInvitation(
    base::Process target_process,
    ConnectionParams connection_params,
    const std::vector<std::pair<std::string, ports::PortRef>>& attached_ports,
    const ProcessErrorCallback& process_error_callback) {
  // Generate the temporary remote node name here so that it can be associated
  // with the ports "attached" to this invitation.
  ports::NodeName temporary_node_name;
  GenerateRandomName(&temporary_node_name);

  {
    base::AutoLock lock(reserved_ports_lock_);
    PortMap& port_map = reserved_ports_[temporary_node_name];
    for (auto& entry : attached_ports) {
      auto result = port_map.emplace(entry.first, entry.second);
      DCHECK(result.second) << "Duplicate attachment: " << entry.first;
    }
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NodeController::SendBrokerClientInvitationOnIOThread,
                     base::Unretained(this), std::move(target_process),
                     std::move(connection_params), temporary_node_name,
                     process_error_callback));
}

void NodeController::AcceptBrokerClientInvitation(
    ConnectionParams connection_params) {
  std::optional<PlatformHandle> broker_host_handle;
  DCHECK(!GetConfiguration().is_broker_process);
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
  if (!connection_params.is_async()) {
    // Use the bootstrap channel for the broker and receive the node's channel
    // synchronously as the first message from the broker.
    DCHECK(connection_params.endpoint().is_valid());
    broker_ = std::make_unique<Broker>(
        connection_params.TakeEndpoint().TakePlatformHandle(),
        /*wait_for_channel_handle=*/true);
    PlatformChannelEndpoint endpoint = broker_->GetInviterEndpoint();

    if (!endpoint.is_valid()) {
      // Most likely the inviter's side of the channel has already been closed
      // and the broker was unable to negotiate a NodeChannel pipe. In this case
      // we can cancel our connection to our inviter.
      DVLOG(1) << "Cannot connect to invalid inviter channel.";
      CancelPendingPortMerges();
      return;
    }

    const bool leak_endpoint = connection_params.leak_endpoint();
    connection_params = ConnectionParams(std::move(endpoint));
    connection_params.set_leak_endpoint(leak_endpoint);
  } else {
    // For async connections, we instead create a new channel for the broker and
    // send a request for the inviting process to bind to it. This avoids doing
    // blocking I/O to accept the invitation. Does not work in some sandboxed
    // environments, where the PlatformChannel constructor will CHECK fail.
    PlatformChannel channel;
    broker_ = std::make_unique<Broker>(
        channel.TakeLocalEndpoint().TakePlatformHandle(),
        /*wait_for_channel_handle=*/false);
    broker_host_handle = channel.TakeRemoteEndpoint().TakePlatformHandle();
  }
#endif
  // Re-enable port merge operations, which may have been disabled if this isn't
  // the first invitation accepted by this process.
  base::AutoLock lock(pending_port_merges_lock_);
  reject_pending_merges_ = false;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NodeController::AcceptBrokerClientInvitationOnIOThread,
                     base::Unretained(this), std::move(connection_params),
                     std::move(broker_host_handle)));
}

void NodeController::ConnectIsolated(ConnectionParams connection_params,
                                     const ports::PortRef& port,
                                     std::string_view connection_name) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NodeController::ConnectIsolatedOnIOThread,
                     base::Unretained(this), std::move(connection_params), port,
                     std::string(connection_name)));
}

void NodeController::SetPortObserver(const ports::PortRef& port,
                                     scoped_refptr<PortObserver> observer) {
  node_->SetUserData(port, std::move(observer));
}

void NodeController::ClosePort(const ports::PortRef& port) {
  SetPortObserver(port, nullptr);
  int rv = node_->ClosePort(port);
  DCHECK_EQ(rv, ports::OK) << " Failed to close port: " << port.name();
}

int NodeController::SendUserMessage(
    const ports::PortRef& port,
    std::unique_ptr<ports::UserMessageEvent> message) {
  return node_->SendUserMessage(port, std::move(message));
}

void NodeController::MergePortIntoInviter(const std::string& name,
                                          const ports::PortRef& port) {
  scoped_refptr<NodeChannel> inviter;
  bool reject_merge = false;
  {
    // Hold |pending_port_merges_lock_| while getting |inviter|. Otherwise,
    // there is a race where the inviter can be set, and |pending_port_merges_|
    // be processed between retrieving |inviter| and adding the merge to
    // |pending_port_merges_|.
    base::AutoLock lock(pending_port_merges_lock_);
    inviter = GetInviterChannel();
    if (reject_pending_merges_) {
      reject_merge = true;
    } else if (!inviter) {
      pending_port_merges_.push_back(std::make_pair(name, port));
      return;
    }
  }
  if (reject_merge) {
    node_->ClosePort(port);
    DVLOG(2) << "Rejecting port merge for name " << name
             << " due to closed inviter channel.";
    return;
  }

  RecordPendingPortMerge(port);

  inviter->RequestPortMerge(port.name(), name);
}

int NodeController::MergeLocalPorts(const ports::PortRef& port0,
                                    const ports::PortRef& port1) {
  return node_->MergeLocalPorts(port0, port1);
}

base::WritableSharedMemoryRegion NodeController::CreateSharedBuffer(
    size_t num_bytes) {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA) && \
    !BUILDFLAG(IS_ANDROID)
  // Shared buffer creation failure is fatal, so always use the broker when we
  // have one; unless of course the embedder forces us not to.
  if (!GetConfiguration().force_direct_shared_memory_allocation && broker_)
    return broker_->GetWritableSharedMemoryRegion(num_bytes);
#endif
  return base::WritableSharedMemoryRegion::Create(num_bytes);
}

void NodeController::RequestShutdown(base::OnceClosure callback) {
  {
    base::AutoLock lock(shutdown_lock_);
    shutdown_callback_ = std::move(callback);
    shutdown_callback_flag_.Set(true);
  }

  AttemptShutdownIfRequested();
}

void NodeController::NotifyBadMessageFrom(const ports::NodeName& source_node,
                                          const std::string& error) {
  scoped_refptr<NodeChannel> peer = GetPeerChannel(source_node);
  DCHECK(peer);
  DCHECK(peer->HasBadMessageHandler());
  peer->NotifyBadMessage(error);
}

bool NodeController::HasBadMessageHandler(const ports::NodeName& source_node) {
  scoped_refptr<NodeChannel> peer = GetPeerChannel(source_node);
  return peer ? peer->HasBadMessageHandler() : false;
}

void NodeController::ForceDisconnectProcessForTesting(
    base::ProcessId process_id) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NodeController::ForceDisconnectProcessForTestingOnIOThread,
          base::Unretained(this), process_id));
}

void NodeController::RecordPendingPortMerge(const ports::PortRef& port_ref) {
  // TODO(sroettger): this should also keep track of the node that is allowed
  //                  to trigger the merge.
  ports::SinglePortLocker locker(&port_ref);
  locker.port()->pending_merge_peer = true;
}

// static
void NodeController::DeserializeRawBytesAsEventForFuzzer(
    base::span<const unsigned char> data) {
  void* payload;
  auto message = NodeChannel::CreateEventMessage(0, data.size(), &payload, 0);
  DCHECK(message);
  base::ranges::copy(data, static_cast<unsigned char*>(payload));
  DeserializeEventMessage(ports::NodeName(), std::move(message));
}

// static
void NodeController::DeserializeMessageAsEventForFuzzer(
    Channel::MessagePtr message) {
  DeserializeEventMessage(ports::NodeName(), std::move(message));
}

void NodeController::SendBrokerClientInvitationOnIOThread(
    base::Process target_process,
    ConnectionParams connection_params,
    ports::NodeName temporary_node_name,
    const ProcessErrorCallback& process_error_callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
  Channel::HandlePolicy handle_policy = Channel::HandlePolicy::kAcceptHandles;
  ConnectionParams node_connection_params;

  if (connection_params.is_async()) {
    // For async connections, the passed endpoint really is the NodeChannel
    // endpoint. The broker channel will be established asynchronously by a
    // |BIND_SYNC_BROKER| message from the invited client.
    node_connection_params = std::move(connection_params);
  } else {
#if BUILDFLAG(IS_WIN)
    // On Windows, if `target_process` is invalid we can't duplicate a pipe
    // handle to the remote client. In that case we instead open a new named
    // pipe and send the client its name via the broker. Once connected, the new
    // named pipe will be used for the client Channel.
    if (!target_process.IsValid()) {
      NamedPlatformChannel::Options options;
      NamedPlatformChannel named_channel(options);

      const bool is_untrusted_process =
          connection_params.is_untrusted_process();
      BrokerHost* broker_host =
          new BrokerHost(base::Process(), std::move(connection_params),
                         process_error_callback);
      broker_host->SendNamedChannel(named_channel.GetServerName());

      // NOTE: The callback given here binds to `this` unretained. This is safe
      // because in production NodeController lives forever. In tests which do
      // tear it down, the IO thread is always destroyed first so this callback
      // will never run after NodeController destruction.
      PlatformChannelServer::WaitForConnection(
          named_channel.TakeServerEndpoint(),
          base::BindOnce(
              [](base::Process target_process,
                 const ports::NodeName& temporary_node_name,
                 const ProcessErrorCallback& process_error_callback,
                 bool is_untrusted_process, NodeController* node_controller,
                 PlatformChannelEndpoint endpoint) {
                if (!endpoint.is_valid()) {
                  return;
                }

                ConnectionParams params(std::move(endpoint));
                params.set_is_untrusted_process(is_untrusted_process);
                node_controller->FinishSendBrokerClientInvitationOnIOThread(
                    std::move(target_process), std::move(params),
                    temporary_node_name, Channel::HandlePolicy::kRejectHandles,
                    process_error_callback);
              },
              std::move(target_process), temporary_node_name,
              process_error_callback, is_untrusted_process, this));
      return;
    }
#endif

    std::optional<ConnectionParams> params = CreateSyncNodeConnectionParams(
        target_process, std::move(connection_params), process_error_callback,
        handle_policy);
    if (!params) {
      if (process_error_callback) {
        process_error_callback.Run("Unable to establish Mojo channel");
      }
      return;
    }

    node_connection_params = std::move(*params);
  }

  FinishSendBrokerClientInvitationOnIOThread(
      std::move(target_process), std::move(node_connection_params),
      temporary_node_name, handle_policy, process_error_callback);
#else   // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
  FinishSendBrokerClientInvitationOnIOThread(
      std::move(target_process), std::move(connection_params),
      temporary_node_name, Channel::HandlePolicy::kAcceptHandles,
      process_error_callback);
#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
}

void NodeController::FinishSendBrokerClientInvitationOnIOThread(
    base::Process target_process,
    ConnectionParams connection_params,
    ports::NodeName temporary_node_name,
    Channel::HandlePolicy handle_policy,
    const ProcessErrorCallback& process_error_callback) {
  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(connection_params), handle_policy,
                          io_task_runner_, process_error_callback);

  // We set up the invitee channel with a temporary name so it can be identified
  // as a pending invitee if it writes any messages to the channel. We may start
  // receiving messages from it (though we shouldn't) as soon as Start() is
  // called below.

  pending_invitations_.insert(std::make_pair(temporary_node_name, channel));

  channel->SetRemoteNodeName(temporary_node_name);
  channel->SetRemoteProcessHandle(std::move(target_process));
  channel->Start();

  channel->AcceptInvitee(name_, temporary_node_name);
}

void NodeController::AcceptBrokerClientInvitationOnIOThread(
    ConnectionParams connection_params,
    std::optional<PlatformHandle> broker_host_handle) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  {
    base::AutoLock lock(inviter_lock_);
    if (inviter_name_ != ports::kInvalidNodeName) {
      // We've already accepted an invitation before and are already part of
      // a different Mojo process network. In order to accept this new one and
      // remain in a consistent state, we have to purge all peer connections and
      // start from scratch.
      {
        base::AutoUnlock unlock(inviter_lock_);
        DropAllPeers();
      }
      inviter_name_ = ports::kInvalidNodeName;
    }

    const bool leak_endpoint = connection_params.leak_endpoint();
    // At this point we don't know the inviter's name, so we can't yet insert it
    // into our |peers_| map. That will happen as soon as we receive an
    // AcceptInvitee message from them.
    bootstrap_inviter_channel_ =
        NodeChannel::Create(this, std::move(connection_params),
                            Channel::HandlePolicy::kAcceptHandles,
                            io_task_runner_, ProcessErrorCallback());

    if (leak_endpoint) {
      // Prevent the inviter pipe handle from being closed on shutdown. Pipe
      // closure may be used by the inviter to detect that the invited process
      // has terminated. In such cases, the invited process must not be invited
      // more than once in its lifetime; otherwise this leak matters.
      //
      // Note that this behavior is supported primarily to help adapt legacy
      // Chrome IPC to Mojo, since channel disconnection is used there as a
      // signal for normal child process termination.
      bootstrap_inviter_channel_->LeakHandleOnShutdown();
    }
  }
  bootstrap_inviter_channel_->Start();
  if (broker_host_handle)
    bootstrap_inviter_channel_->BindBrokerHost(std::move(*broker_host_handle));
}

void NodeController::ConnectIsolatedOnIOThread(
    ConnectionParams connection_params,
    ports::PortRef port,
    const std::string& connection_name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Processes using isolated connections to communicate have no ability to lean
  // on a broker for handle relaying, so we allow them to send handles to each
  // other at their own peril.
  scoped_refptr<NodeChannel> channel = NodeChannel::Create(
      this, std::move(connection_params), Channel::HandlePolicy::kAcceptHandles,
      io_task_runner_, {});

  RequestContext request_context;
  ports::NodeName token;
  GenerateRandomName(&token);
  pending_isolated_connections_.emplace(
      token, IsolatedConnection{channel, port, connection_name});
  if (!connection_name.empty()) {
    // If a connection already exists with this name, drop it.
    auto it = named_isolated_connections_.find(connection_name);
    if (it != named_isolated_connections_.end()) {
      ports::NodeName connection_node = it->second;
      if (connection_node != name_) {
        DropPeer(connection_node, nullptr);
      } else {
        auto pending_it = pending_isolated_connections_.find(connection_node);
        if (pending_it != pending_isolated_connections_.end()) {
          node_->ClosePort(pending_it->second.local_port);
          pending_isolated_connections_.erase(pending_it);
        }
        named_isolated_connections_.erase(it);
      }
    }
    named_isolated_connections_.emplace(connection_name, token);
  }

  channel->SetRemoteNodeName(token);
  channel->Start();

  RecordPendingPortMerge(port);

  channel->AcceptPeer(name_, token, port.name());
}

scoped_refptr<NodeChannel> NodeController::GetPeerChannel(
    const ports::NodeName& name) {
  base::AutoLock lock(peers_lock_);
  auto it = peers_.find(name);
  if (it == peers_.end())
    return nullptr;
  return it->second;
}

scoped_refptr<NodeChannel> NodeController::GetInviterChannel() {
  ports::NodeName inviter_name;
  {
    base::AutoLock lock(inviter_lock_);
    inviter_name = inviter_name_;
  }
  return GetPeerChannel(inviter_name);
}

scoped_refptr<NodeChannel> NodeController::GetBrokerChannel() {
  if (GetConfiguration().is_broker_process)
    return nullptr;

  ports::NodeName broker_name;
  {
    base::AutoLock lock(broker_lock_);
    broker_name = broker_name_;
  }
  return GetPeerChannel(broker_name);
}

void NodeController::AddPeer(const ports::NodeName& name,
                             scoped_refptr<NodeChannel> channel,
                             bool start_channel,
                             bool allow_name_reuse) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(name != ports::kInvalidNodeName);
  DCHECK(channel);

  channel->SetRemoteNodeName(name);

  OutgoingMessageQueue pending_messages;
  {
    base::AutoLock lock(peers_lock_);
    if (base::Contains(peers_, name)) {
      // This can happen normally if two nodes race to be introduced to each
      // other. The losing pipe will be silently closed and introduction should
      // not be affected.
      DVLOG(1) << "Ignoring duplicate peer name " << name;
      return;
    }

    if (dropped_peers_.Contains(name) && !allow_name_reuse) {
      DVLOG(1) << "Trying to re-add dropped peer " << name;
      return;
    }

    auto result = peers_.insert(std::make_pair(name, channel));
    DCHECK(result.second);

    DVLOG(2) << "Accepting new peer " << name << " on node " << name_;

    auto it = pending_peer_messages_.find(name);
    if (it != pending_peer_messages_.end()) {
      std::swap(pending_messages, it->second);
      pending_peer_messages_.erase(it);
    }
  }

  if (start_channel)
    channel->Start();

  // Flush any queued message we need to deliver to this node.
  while (!pending_messages.empty()) {
    channel->SendChannelMessage(std::move(pending_messages.front()));
    pending_messages.pop();
  }
}

void NodeController::DropPeer(const ports::NodeName& node_name,
                              NodeChannel* channel) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // NOTE: Either the `peers_` erasure or the `pending_invitations_` erasure
  // below, if executed, may drop the last reference to the named NodeChannel
  // and thus result in its deletion. The passed `node_name` argument may be
  // owned by that same NodeChannel, so we make a copy of it here to avoid
  // potentially unsafe references further below.
  ports::NodeName name = node_name;

  {
    base::AutoLock lock(peers_lock_);
    auto it = peers_.find(name);

    if (it != peers_.end()) {
      ports::NodeName peer = it->first;
      peers_.erase(it);
      dropped_peers_.Insert(peer);
      DVLOG(1) << "Dropped peer " << peer;
    }

    pending_peer_messages_.erase(name);
    pending_invitations_.erase(name);
  }

  std::vector<ports::PortRef> ports_to_close;
  {
    // Clean up any reserved ports.
    base::AutoLock lock(reserved_ports_lock_);
    auto it = reserved_ports_.find(name);
    if (it != reserved_ports_.end()) {
      for (auto& entry : it->second)
        ports_to_close.emplace_back(entry.second);
      reserved_ports_.erase(it);
    }
  }

  bool is_inviter;
  {
    base::AutoLock lock(inviter_lock_);
    is_inviter = (name == inviter_name_ ||
                  (channel && channel == bootstrap_inviter_channel_));
  }

  // If the error comes from the inviter channel, we also need to cancel any
  // port merge requests, so that errors can be propagated to the message
  // pipes.
  if (is_inviter)
    CancelPendingPortMerges();

  auto connection_it = pending_isolated_connections_.find(name);
  if (connection_it != pending_isolated_connections_.end()) {
    IsolatedConnection& connection = connection_it->second;
    ports_to_close.push_back(connection.local_port);
    if (!connection.name.empty())
      named_isolated_connections_.erase(connection.name);
    pending_isolated_connections_.erase(connection_it);
  }

  for (const auto& port : ports_to_close)
    node_->ClosePort(port);

  node_->LostConnectionToNode(name);
  AttemptShutdownIfRequested();
}

void NodeController::SendPeerEvent(const ports::NodeName& name,
                                   ports::ScopedEvent event) {
  Channel::MessagePtr event_message = SerializeEventMessage(std::move(event));
  if (!event_message)
    return;
  scoped_refptr<NodeChannel> peer = GetPeerChannel(name);
#if BUILDFLAG(IS_WIN)
  if (event_message->has_handles()) {
    // If we're sending a message with handles we aren't the destination
    // node's inviter or broker (i.e. we don't know its process handle), ask
    // the broker to relay for us.
    scoped_refptr<NodeChannel> broker = GetBrokerChannel();
    if (!peer || !peer->HasRemoteProcessHandle()) {
      if (!GetConfiguration().is_broker_process && broker) {
        broker->RelayEventMessage(name, std::move(event_message));
      } else {
        base::AutoLock lock(broker_lock_);
        pending_relay_messages_[name].emplace(std::move(event_message));
      }
      return;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  if (peer) {
    peer->SendChannelMessage(std::move(event_message));
    return;
  }

  // If we don't know who the peer is and we are the broker, we can only assume
  // the peer is invalid, i.e., it's either a junk name or has already been
  // disconnected.
  scoped_refptr<NodeChannel> broker = GetBrokerChannel();
  if (!broker) {
    DVLOG(1) << "Dropping message for unknown peer: " << name;
    return;
  }

  // If we aren't the broker, assume we just need to be introduced and queue
  // until that can be either confirmed or denied by the broker.
  bool needs_introduction = false;
  {
    base::AutoLock lock(peers_lock_);
    // We may have been introduced on another thread by the time we get here.
    // Double-check to be safe.
    auto it = peers_.find(name);
    if (it == peers_.end()) {
      auto& queue = pending_peer_messages_[name];
      needs_introduction = queue.empty();
      queue.emplace(std::move(event_message));
    } else {
      peer = it->second;
    }
  }
  if (needs_introduction)
    broker->RequestIntroduction(name);
  else if (peer)
    peer->SendChannelMessage(std::move(event_message));
}

void NodeController::DropAllPeers() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  std::vector<scoped_refptr<NodeChannel>> all_peers;
  {
    base::AutoLock lock(inviter_lock_);
    if (bootstrap_inviter_channel_) {
      // |bootstrap_inviter_channel_| isn't null'd here because we rely on its
      // existence to determine whether or not this is the root node. Once
      // bootstrap_inviter_channel_->ShutDown() has been called,
      // |bootstrap_inviter_channel_| is essentially a dead object and it
      // doesn't matter if it's deleted now or when |this| is deleted. Note:
      // |bootstrap_inviter_channel_| is only modified on the IO thread.
      all_peers.push_back(bootstrap_inviter_channel_);
    }
  }

  {
    base::AutoLock lock(peers_lock_);
    for (const auto& peer : peers_)
      all_peers.push_back(peer.second);
    for (const auto& peer : pending_invitations_)
      all_peers.push_back(peer.second);
    peers_.clear();
    pending_invitations_.clear();
    pending_peer_messages_.clear();
    pending_isolated_connections_.clear();
    named_isolated_connections_.clear();
  }

  for (const auto& peer : all_peers)
    peer->ShutDown();

  if (destroy_on_io_thread_shutdown_)
    delete this;
}

void NodeController::ForwardEvent(const ports::NodeName& node,
                                  ports::ScopedEvent event) {
  DCHECK(event);
  if (node == name_)
    node_->AcceptEvent(name_, std::move(event));
  else
    SendPeerEvent(node, std::move(event));

  AttemptShutdownIfRequested();
}

void NodeController::BroadcastEvent(ports::ScopedEvent event) {
  Channel::MessagePtr channel_message = SerializeEventMessage(std::move(event));
  DCHECK(channel_message && !channel_message->has_handles());

  scoped_refptr<NodeChannel> broker = GetBrokerChannel();
  if (broker) {
    broker->Broadcast(std::move(channel_message));
  } else if (broker_name_ == ports::kInvalidNodeName) {
    // Do an additional check if broker_name_ is not set. It's possible that we
    // don't have a broker channel even though we're not the broker ourselves,
    // e.g. if this code path is called from the channel error path..
    OnBroadcast(name_, std::move(channel_message));
  }
}

void NodeController::PortStatusChanged(const ports::PortRef& port) {
  scoped_refptr<ports::UserData> user_data;
  node_->GetUserData(port, &user_data);

  PortObserver* observer = static_cast<PortObserver*>(user_data.get());
  if (observer) {
    observer->OnPortStatusChanged();
  } else {
    DVLOG(2) << "Ignoring status change for " << port.name() << " because it "
             << "doesn't have an observer.";
  }
}

void NodeController::OnAcceptInvitee(const ports::NodeName& from_node,
                                     const ports::NodeName& inviter_name,
                                     const ports::NodeName& token) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  scoped_refptr<NodeChannel> inviter;
  {
    base::AutoLock lock(inviter_lock_);
    if (bootstrap_inviter_channel_ &&
        inviter_name_ == ports::kInvalidNodeName) {
      inviter_name_ = inviter_name;
      inviter = bootstrap_inviter_channel_;
    }
  }

  if (!inviter) {
    DLOG(ERROR) << "Unexpected AcceptInvitee message from " << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  inviter->SetRemoteNodeName(inviter_name);
  inviter->AcceptInvitation(token, name_);

  // NOTE: The invitee does not actually add its inviter as a peer until
  // receiving an AcceptBrokerClient message from the broker. The inviter will
  // request that said message be sent upon receiving AcceptInvitation.

  DVLOG(1) << "Broker client " << name_ << " accepting invitation from "
           << inviter_name;
}

void NodeController::OnAcceptInvitation(const ports::NodeName& from_node,
                                        const ports::NodeName& token,
                                        const ports::NodeName& invitee_name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  auto it = pending_invitations_.find(from_node);
  if (it == pending_invitations_.end() || token != from_node) {
    DLOG(ERROR) << "Received unexpected AcceptInvitation message from "
                << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  {
    base::AutoLock lock(reserved_ports_lock_);
    auto reserved_ports_it = reserved_ports_.find(from_node);
    if (reserved_ports_it != reserved_ports_.end()) {
      // Swap the temporary node name's reserved ports into an entry keyed by
      // the real node name.
      auto result = reserved_ports_.emplace(
          invitee_name, std::move(reserved_ports_it->second));
      DCHECK(result.second);
      reserved_ports_.erase(reserved_ports_it);
    }
  }

  scoped_refptr<NodeChannel> channel = it->second;
  pending_invitations_.erase(it);

  DCHECK(channel);

  DVLOG(1) << "Node " << name_ << " accepted invitee " << invitee_name;

  AddPeer(invitee_name, channel, false /* start_channel */);

  // TODO(rockot): We could simplify invitee initialization if we could
  // synchronously get a new async broker channel from the broker. For now we do
  // it asynchronously since it's only used to facilitate handle passing, not
  // handle creation.
  scoped_refptr<NodeChannel> broker = GetBrokerChannel();
  if (broker) {
    // Inform the broker of this new client.
    broker->AddBrokerClient(invitee_name, channel->CloneRemoteProcessHandle());
  } else {
    // If we have no broker, either we need to wait for one, or we *are* the
    // broker.
    scoped_refptr<NodeChannel> inviter = GetInviterChannel();
    if (!inviter) {
      base::AutoLock lock(inviter_lock_);
      inviter = bootstrap_inviter_channel_;
    }

    if (!inviter) {
      // Yes, we're the broker. We can initialize the client directly.
      channel->AcceptBrokerClient(name_, PlatformHandle(),
                                  channel->LocalCapabilities());
    } else {
      // We aren't the broker, so wait for a broker connection.
      base::AutoLock lock(broker_lock_);
      pending_broker_clients_.push(invitee_name);
    }
  }
}

void NodeController::OnAddBrokerClient(const ports::NodeName& from_node,
                                       const ports::NodeName& client_name,
                                       base::ProcessHandle process_handle) {
  base::Process scoped_process_handle(process_handle);

  scoped_refptr<NodeChannel> sender = GetPeerChannel(from_node);
  if (!sender) {
    DLOG(ERROR) << "Ignoring AddBrokerClient from unknown sender.";
    return;
  }

  if (!GetConfiguration().is_broker_process) {
    DLOG(ERROR) << "Ignoring AddBrokerClient on non-broker node.";
    return;
  }

  if (GetPeerChannel(client_name)) {
    LOG(ERROR) << "Ignoring AddBrokerClient for known client.";
    DropPeer(from_node, nullptr);
    return;
  }

  PlatformChannel broker_channel;
  ConnectionParams connection_params(broker_channel.TakeLocalEndpoint());
  scoped_refptr<NodeChannel> client = NodeChannel::Create(
      this, std::move(connection_params), Channel::HandlePolicy::kAcceptHandles,
      io_task_runner_, ProcessErrorCallback());

#if BUILDFLAG(IS_WIN)
  // The broker must have a working handle to the client process in order to
  // properly copy other handles to and from the client.
  if (!scoped_process_handle.IsValid()) {
    DLOG(ERROR) << "Broker rejecting client with invalid process handle.";
    return;
  }
#endif
  client->SetRemoteProcessHandle(std::move(scoped_process_handle));

  AddPeer(client_name, client, true /* start_channel */);

  DVLOG(1) << "Broker " << name_ << " accepting client " << client_name
           << " from peer " << from_node;

  sender->BrokerClientAdded(
      client_name, broker_channel.TakeRemoteEndpoint().TakePlatformHandle());
}

void NodeController::OnBrokerClientAdded(const ports::NodeName& from_node,
                                         const ports::NodeName& client_name,
                                         PlatformHandle broker_channel) {
  scoped_refptr<NodeChannel> client = GetPeerChannel(client_name);
  if (!client) {
    DLOG(ERROR) << "BrokerClientAdded for unknown client " << client_name;
    return;
  }

  // This should have come from our own broker.
  if (GetBrokerChannel() != GetPeerChannel(from_node)) {
    DLOG(ERROR) << "BrokerClientAdded from non-broker node " << from_node;
    return;
  }

  DVLOG(1) << "Client " << client_name << " accepted by broker " << from_node;

  client->AcceptBrokerClient(from_node, std::move(broker_channel),
                             GetBrokerChannel()->RemoteCapabilities());
}

void NodeController::OnAcceptBrokerClient(const ports::NodeName& from_node,
                                          const ports::NodeName& broker_name,
                                          PlatformHandle broker_channel,
                                          const uint64_t broker_capabilities) {
  if (GetConfiguration().is_broker_process) {
    // The broker should never receive this message from anyone.
    DropPeer(from_node, nullptr);
    return;
  }

  // This node should already have an inviter in bootstrap mode.
  ports::NodeName inviter_name;
  scoped_refptr<NodeChannel> inviter;
  {
    base::AutoLock lock(inviter_lock_);
    inviter_name = inviter_name_;
    inviter = bootstrap_inviter_channel_;
    bootstrap_inviter_channel_ = nullptr;
  }

  if (inviter_name != from_node || !inviter ||
      broker_name == ports::kInvalidNodeName) {
    // We are not expecting this message. Assume the source is hostile.
    DropPeer(from_node, nullptr);
    return;
  }

  base::queue<ports::NodeName> pending_broker_clients;
  std::unordered_map<ports::NodeName, OutgoingMessageQueue>
      pending_relay_messages;
  {
    base::AutoLock lock(broker_lock_);
    broker_name_ = broker_name;
    std::swap(pending_broker_clients, pending_broker_clients_);
    std::swap(pending_relay_messages, pending_relay_messages_);
  }

  // It's now possible to add both the broker and the inviter as peers.
  // Note that the broker and inviter may be the same node.
  scoped_refptr<NodeChannel> broker;
  if (broker_name == inviter_name) {
    broker = inviter;
  } else if (broker_channel.is_valid()) {
    broker = NodeChannel::Create(
        this,
        ConnectionParams(PlatformChannelEndpoint(std::move(broker_channel))),
        Channel::HandlePolicy::kAcceptHandles, io_task_runner_,
        ProcessErrorCallback());
    broker->SetRemoteCapabilities(broker_capabilities);
    AddPeer(broker_name, broker, true /* start_channel */);
  } else {
    DropPeer(from_node, nullptr);
    return;
  }

  AddPeer(inviter_name, inviter, false /* start_channel */);

  {
    // Complete any port merge requests we have waiting for the inviter.
    std::vector<std::pair<std::string, ports::PortRef>> pending_port_merges;
    {
      base::AutoLock lock(pending_port_merges_lock_);
      std::swap(pending_port_merges_, pending_port_merges);
    }
    std::vector<ports::PortName> pending_port_names;
    for (auto& request : pending_port_merges) {
      RecordPendingPortMerge(request.second);
      inviter->RequestPortMerge(request.second.name(), request.first);
    }
  }

  // Feed the broker any pending invitees of our own.
  while (!pending_broker_clients.empty()) {
    const ports::NodeName& invitee_name = pending_broker_clients.front();
    auto it = peers_.find(invitee_name);
    if (it != peers_.end()) {
      broker->AddBrokerClient(invitee_name,
                              it->second->CloneRemoteProcessHandle());
    }
    pending_broker_clients.pop();
  }

#if BUILDFLAG(IS_WIN)
  // Have the broker relay any messages we have waiting.
  for (auto& entry : pending_relay_messages) {
    const ports::NodeName& destination = entry.first;
    auto& message_queue = entry.second;
    while (!message_queue.empty()) {
      broker->RelayEventMessage(destination, std::move(message_queue.front()));
      message_queue.pop();
    }
  }
#endif
  if (inviter->HasLocalCapability(kNodeCapabilitySupportsUpgrade) &&
      inviter->HasRemoteCapability(kNodeCapabilitySupportsUpgrade)) {
    inviter->OfferChannelUpgrade();
  }

  DVLOG(1) << "Client " << name_ << " accepted by broker " << broker_name;
}

void NodeController::OnEventMessage(const ports::NodeName& from_node,
                                    Channel::MessagePtr channel_message) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  auto event = DeserializeEventMessage(from_node, std::move(channel_message));
  if (!event) {
    // We silently ignore unparseable events, as they may come from a process
    // running a newer version of Mojo.
    DVLOG(1) << "Ignoring invalid or unknown event from " << from_node;
    return;
  }

  node_->AcceptEvent(from_node, std::move(event));

  AttemptShutdownIfRequested();
}

void NodeController::OnRequestPortMerge(
    const ports::NodeName& from_node,
    const ports::PortName& connector_port_name,
    const std::string& name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  DVLOG(2) << "Node " << name_ << " received RequestPortMerge for name " << name
           << " and port " << connector_port_name << "@" << from_node;

  ports::PortRef local_port;
  {
    base::AutoLock lock(reserved_ports_lock_);
    auto it = reserved_ports_.find(from_node);
    // TODO(crbug.com/40567118): We should send a notification back to the
    // requestor so they can clean up their dangling port in this failure case.
    // This requires changes to the internal protocol, which can't be made yet.
    // Until this is done, pipes from |MojoExtractMessagePipeFromInvitation()|
    // will never break if the given name was invalid.
    if (it == reserved_ports_.end()) {
      DVLOG(1) << "Ignoring port merge request from node " << from_node << ". "
               << "No ports reserved for that node.";
      return;
    }

    PortMap& port_map = it->second;
    auto port_it = port_map.find(name);
    if (port_it == port_map.end()) {
      DVLOG(1) << "Ignoring request to connect to port for unknown name "
               << name << " from node " << from_node;
      return;
    }
    local_port = port_it->second;
    port_map.erase(port_it);
    if (port_map.empty())
      reserved_ports_.erase(it);
  }

  int rv = node_->MergePorts(local_port, from_node, connector_port_name);
  if (rv != ports::OK)
    DLOG(ERROR) << "MergePorts failed: " << rv;
}

void NodeController::OnRequestIntroduction(const ports::NodeName& from_node,
                                           const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (broker_name_ != ports::kInvalidNodeName) {
    DLOG(ERROR) << "Ignoring OnRequestIntroduction on non-broker node.";
    DropPeer(from_node, nullptr);
    return;
  }

  scoped_refptr<NodeChannel> requestor = GetPeerChannel(from_node);
  if (from_node == name || name == ports::kInvalidNodeName || !requestor) {
    DLOG(ERROR) << "Rejecting invalid OnRequestIntroduction message from "
                << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  scoped_refptr<NodeChannel> new_friend = GetPeerChannel(name);
  if (!new_friend) {
    // We don't know who they're talking about!
    requestor->Introduce(name, PlatformHandle(), kNodeCapabilityNone);
  } else {
    PlatformChannel new_channel;
    requestor->Introduce(name,
                         new_channel.TakeLocalEndpoint().TakePlatformHandle(),
                         new_friend->RemoteCapabilities());
    new_friend->Introduce(from_node,
                          new_channel.TakeRemoteEndpoint().TakePlatformHandle(),
                          requestor->RemoteCapabilities());
  }
}

void NodeController::OnIntroduce(const ports::NodeName& from_node,
                                 const ports::NodeName& name,
                                 PlatformHandle channel_handle,
                                 const uint64_t remote_capabilities) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (broker_name_ == ports::kInvalidNodeName || from_node != broker_name_) {
    DVLOG(1) << "Ignoring introduction from non-broker process.";
    DropPeer(from_node, nullptr);
    return;
  }

  if (!channel_handle.is_valid()) {
    node_->LostConnectionToNode(name);

    DVLOG(1) << "Could not be introduced to peer " << name;
    base::AutoLock lock(peers_lock_);
    pending_peer_messages_.erase(name);
    return;
  }

#if BUILDFLAG(IS_WIN)
  // Introduced peers are never our broker nor our inviter, so we never accept
  // handles from them directly.
  constexpr auto kPeerHandlePolicy = Channel::HandlePolicy::kRejectHandles;
#else
  constexpr auto kPeerHandlePolicy = Channel::HandlePolicy::kAcceptHandles;
#endif

  scoped_refptr<NodeChannel> channel = NodeChannel::Create(
      this,
      ConnectionParams(PlatformChannelEndpoint(std::move(channel_handle))),
      kPeerHandlePolicy, io_task_runner_, ProcessErrorCallback());

  DVLOG(1) << "Adding new peer " << name << " via broker introduction.";
  AddPeer(name, channel, true /* start_channel */);

  channel->SetRemoteCapabilities(remote_capabilities);

  if (channel->HasLocalCapability(kNodeCapabilitySupportsUpgrade) &&
      channel->HasRemoteCapability(kNodeCapabilitySupportsUpgrade)) {
    channel->OfferChannelUpgrade();
  }
}

void NodeController::OnBroadcast(const ports::NodeName& from_node,
                                 Channel::MessagePtr message) {
  DCHECK(!message->has_handles());

  if (broker_name_ != ports::kInvalidNodeName) {
    DLOG(ERROR) << "Ignoring OnBroadcast on non-broker node.";
    DropPeer(from_node, nullptr);
    return;
  }

  auto event = DeserializeEventMessage(from_node, std::move(message));
  if (!event) {
    // We silently ignore unparseable events, as they may come from a process
    // running a newer version of Mojo.
    DVLOG(1) << "Ignoring request to broadcast invalid or unknown event from "
             << from_node;
    return;
  }

  base::AutoLock lock(peers_lock_);
  for (auto& iter : peers_) {
    // Clone and send the event to each known peer. Events which cannot be
    // cloned cannot be broadcast.
    ports::ScopedEvent clone = event->CloneForBroadcast();
    if (!clone) {
      DVLOG(1) << "Ignoring request to broadcast invalid event from "
               << from_node << " [type=" << static_cast<uint32_t>(event->type())
               << "]";
      return;
    }

    iter.second->SendChannelMessage(SerializeEventMessage(std::move(clone)));
  }
}

#if BUILDFLAG(IS_WIN)
void NodeController::OnRelayEventMessage(const ports::NodeName& from_node,
                                         base::ProcessHandle from_process,
                                         const ports::NodeName& destination,
                                         Channel::MessagePtr message) {
  // The broker should always know which process this came from.
  DCHECK(from_process != base::kNullProcessHandle);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (GetBrokerChannel()) {
    // Only the broker should be asked to relay a message.
    LOG(ERROR) << "Non-broker refusing to relay message.";
    DropPeer(from_node, nullptr);
    return;
  }

  if (destination == name_) {
    // Great, we can deliver this message locally.
    OnEventMessage(from_node, std::move(message));
    return;
  }

  scoped_refptr<NodeChannel> peer = GetPeerChannel(destination);
  if (peer)
    peer->EventMessageFromRelay(from_node, std::move(message));
  else
    DLOG(ERROR) << "Dropping relay message for unknown node " << destination;
}

void NodeController::OnEventMessageFromRelay(const ports::NodeName& from_node,
                                             const ports::NodeName& source_node,
                                             Channel::MessagePtr message) {
  if (GetPeerChannel(from_node) != GetBrokerChannel()) {
    LOG(ERROR) << "Refusing relayed message from non-broker node.";
    DropPeer(from_node, nullptr);
    return;
  }

  OnEventMessage(source_node, std::move(message));
}
#endif

void NodeController::OnAcceptPeer(const ports::NodeName& from_node,
                                  const ports::NodeName& token,
                                  const ports::NodeName& peer_name,
                                  const ports::PortName& port_name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  auto it = pending_isolated_connections_.find(from_node);
  if (it == pending_isolated_connections_.end()) {
    DLOG(ERROR) << "Received unexpected AcceptPeer message from " << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  IsolatedConnection& connection = it->second;
  scoped_refptr<NodeChannel> channel = std::move(connection.channel);
  ports::PortRef local_port = connection.local_port;
  if (!connection.name.empty())
    named_isolated_connections_[connection.name] = peer_name;
  pending_isolated_connections_.erase(it);
  DCHECK(channel);

  if (name_ != peer_name) {
    // It's possible (e.g. in tests) that we may "connect" to ourself, in which
    // case we skip this |AddPeer()| call and go straight to merging ports.
    // Note that we explicitly drop any prior connection to the same peer so
    // that new isolated connections can replace old ones.
    DropPeer(peer_name, nullptr);
    AddPeer(peer_name, channel, false /* start_channel */,
            true /* allow_name_reuse */);
    DVLOG(1) << "Node " << name_ << " accepted peer " << peer_name;
  }

  // We need to choose one side to initiate the port merge. It doesn't matter
  // who does it as long as they don't both try. Simple solution: pick the one
  // with the "smaller" port name.
  if (local_port.name() < port_name)
    node()->MergePorts(local_port, peer_name, port_name);
}

void NodeController::OnChannelError(const ports::NodeName& from_node,
                                    NodeChannel* channel) {
  if (io_task_runner_->RunsTasksInCurrentSequence()) {
    RequestContext request_context(RequestContext::Source::SYSTEM);
    DropPeer(from_node, channel);
  } else {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NodeController::OnChannelError, base::Unretained(this),
                       from_node, base::RetainedRef(channel)));
  }
}

void NodeController::CancelPendingPortMerges() {
  std::vector<ports::PortRef> ports_to_close;

  {
    base::AutoLock lock(pending_port_merges_lock_);
    reject_pending_merges_ = true;
    for (const auto& port : pending_port_merges_)
      ports_to_close.push_back(port.second);
    pending_port_merges_.clear();
  }

  for (const auto& port : ports_to_close)
    node_->ClosePort(port);
}

void NodeController::DestroyOnIOThreadShutdown() {
  destroy_on_io_thread_shutdown_ = true;
}

void NodeController::AttemptShutdownIfRequested() {
  if (!shutdown_callback_flag_)
    return;

  base::OnceClosure callback;
  {
    base::AutoLock lock(shutdown_lock_);
    if (shutdown_callback_.is_null())
      return;
    if (!node_->CanShutdownCleanly(
            ports::Node::ShutdownPolicy::ALLOW_LOCAL_PORTS)) {
      DVLOG(2) << "Unable to cleanly shut down node " << name_;
      return;
    }

    callback = std::move(shutdown_callback_);
    shutdown_callback_flag_.Set(false);
  }

  DCHECK(!callback.is_null());

  std::move(callback).Run();
}

void NodeController::ForceDisconnectProcessForTestingOnIOThread(
    base::ProcessId process_id) {
#if BUILDFLAG(IS_NACL) || BUILDFLAG(IS_IOS)
  NOTREACHED();
#else
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  RequestContext request_context;

  // A channel may have multiple aliases since we generate one for any we
  // invite and then only later refer to it by its own chosen name.
  NodeMap peers_to_drop;
  for (auto& peer : peers_) {
    NodeChannel* channel = peer.second.get();
    if (channel->HasRemoteProcessHandle()) {
      base::Process process(channel->CloneRemoteProcessHandle());
      if (process.Pid() == process_id)
        peers_to_drop.emplace(peer.first, peer.second);
    }
  }

  for (auto& peer : peers_to_drop)
    DropPeer(peer.first, peer.second.get());
#endif
}

NodeController::IsolatedConnection::IsolatedConnection() = default;

NodeController::IsolatedConnection::IsolatedConnection(
    const IsolatedConnection& other) = default;

NodeController::IsolatedConnection::IsolatedConnection(
    IsolatedConnection&& other) = default;

NodeController::IsolatedConnection::IsolatedConnection(
    scoped_refptr<NodeChannel> channel,
    const ports::PortRef& local_port,
    std::string_view name)
    : channel(std::move(channel)), local_port(local_port), name(name) {}

NodeController::IsolatedConnection::~IsolatedConnection() = default;

NodeController::IsolatedConnection&
NodeController::IsolatedConnection::operator=(const IsolatedConnection& other) =
    default;

NodeController::IsolatedConnection&
NodeController::IsolatedConnection::operator=(IsolatedConnection&& other) =
    default;

BoundedPeerSet::BoundedPeerSet() = default;
BoundedPeerSet::~BoundedPeerSet() = default;

void BoundedPeerSet::Insert(const ports::NodeName& name) {
  if (new_set_.size() == kHalfSize) {
    old_set_.clear();
    std::swap(old_set_, new_set_);
  }
  new_set_.insert(name);
}

bool BoundedPeerSet::Contains(const ports::NodeName& name) {
  if (base::Contains(old_set_, name)) {
    return true;
  }
  if (base::Contains(new_set_, name)) {
    return true;
  }
  return false;
}

}  // namespace core
}  // namespace mojo
