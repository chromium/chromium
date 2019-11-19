// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_NODE_CHANNEL_H_
#define MOJO_CORE_NODE_CHANNEL_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/embedder/process_error_callback.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/scoped_process_handle.h"

namespace mojo {
namespace core {

// Wraps a Channel to send and receive Node control messages.
class NodeChannel : public base::RefCountedThreadSafe<NodeChannel>,
                    public Channel::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnAcceptInvitee(const ports::NodeName& from_node,
                                 const ports::NodeName& inviter_name,
                                 const ports::NodeName& token) = 0;
    virtual void OnAcceptInvitation(const ports::NodeName& from_node,
                                    const ports::NodeName& token,
                                    const ports::NodeName& invitee_name) = 0;
    virtual void OnAddBrokerClient(const ports::NodeName& from_node,
                                   const ports::NodeName& client_name,
                                   base::ProcessHandle process_handle) = 0;
    virtual void OnBrokerClientAdded(const ports::NodeName& from_node,
                                     const ports::NodeName& client_name,
                                     PlatformHandle broker_channel) = 0;
    virtual void OnAcceptBrokerClient(const ports::NodeName& from_node,
                                      const ports::NodeName& broker_name,
                                      PlatformHandle broker_channel) = 0;
    virtual void OnEventMessage(const ports::NodeName& from_node,
                                Channel::MessagePtr message) = 0;
    virtual void OnRequestPortMerge(const ports::NodeName& from_node,
                                    const ports::PortName& connector_port_name,
                                    const std::string& token) = 0;
    virtual void OnRequestIntroduction(const ports::NodeName& from_node,
                                       const ports::NodeName& name) = 0;
    virtual void OnIntroduce(const ports::NodeName& from_node,
                             const ports::NodeName& name,
                             PlatformHandle channel_handle) = 0;
    virtual void OnBroadcast(const ports::NodeName& from_node,
                             Channel::MessagePtr message) = 0;
#if defined(OS_WIN)
    virtual void OnRelayEventMessage(const ports::NodeName& from_node,
                                     base::ProcessHandle from_process,
                                     const ports::NodeName& destination,
                                     Channel::MessagePtr message) = 0;
    virtual void OnEventMessageFromRelay(const ports::NodeName& from_node,
                                         const ports::NodeName& source_node,
                                         Channel::MessagePtr message) = 0;
#endif
    virtual void OnAcceptPeer(const ports::NodeName& from_node,
                              const ports::NodeName& token,
                              const ports::NodeName& peer_name,
                              const ports::PortName& port_name) = 0;
    virtual void OnChannelError(const ports::NodeName& node,
                                NodeChannel* channel) = 0;
  };

  static scoped_refptr<NodeChannel> Create(
      Delegate* delegate,
      ConnectionParams connection_params,
      Channel::HandlePolicy channel_handle_policy,
      scoped_refptr<base::TaskRunner> io_task_runner,
      const ProcessErrorCallback& process_error_callback);

  static Channel::MessagePtr CreateEventMessage(size_t capacity,
                                                size_t payload_size,
                                                void** payload,
                                                size_t num_handles);

  static void GetEventMessageData(Channel::Message* message,
                                  void** data,
                                  size_t* num_data_bytes);

  Channel* channel() const { return channel_.get(); }

  // Start receiving messages.
  void Start();

  // Permanently stop the channel from sending or receiving messages.
  void ShutDown();

  // Leaks the pipe handle instead of closing it on shutdown.
  void LeakHandleOnShutdown();

  // Invokes the bad message callback for this channel, if any.
  void NotifyBadMessage(const std::string& error);

  void SetRemoteProcessHandle(ScopedProcessHandle process_handle);
  bool HasRemoteProcessHandle();
  ScopedProcessHandle CloneRemoteProcessHandle();

  // Used for context in Delegate calls (via |from_node| arguments.)
  void SetRemoteNodeName(const ports::NodeName& name);

  void AcceptInvitee(const ports::NodeName& inviter_name,
                     const ports::NodeName& token);
  void AcceptInvitation(const ports::NodeName& token,
                        const ports::NodeName& invitee_name);
  void AcceptPeer(const ports::NodeName& sender_name,
                  const ports::NodeName& token,
                  const ports::PortName& port_name);
  void AddBrokerClient(const ports::NodeName& client_name,
                       ScopedProcessHandle process_handle);
  void BrokerClientAdded(const ports::NodeName& client_name,
                         PlatformHandle broker_channel);
  void AcceptBrokerClient(const ports::NodeName& broker_name,
                          PlatformHandle broker_channel);
  void RequestPortMerge(const ports::PortName& connector_port_name,
                        const std::string& token);
  void RequestIntroduction(const ports::NodeName& name);
  void Introduce(const ports::NodeName& name, PlatformHandle channel_handle);
  void SendChannelMessage(Channel::MessagePtr message);
  void Broadcast(Channel::MessagePtr message);
  void BindBrokerHost(PlatformHandle broker_host_handle);

#if defined(OS_WIN)
  // Relay the message to the specified node via this channel.  This is used to
  // pass windows handles between two processes that do not have permission to
  // duplicate handles into the other's address space. The relay process is
  // assumed to have that permission.
  void RelayEventMessage(const ports::NodeName& destination,
                         Channel::MessagePtr message);

  // Sends a message to its destination from a relay. This is interpreted by the
  // receiver similarly to EventMessage, but the original source node is
  // provided as additional message metadata from the (trusted) relay node.
  void EventMessageFromRelay(const ports::NodeName& source,
                             Channel::MessagePtr message);
#endif

 private:
  friend class base::RefCountedThreadSafe<NodeChannel>;

  using PendingMessageQueue = base::queue<Channel::MessagePtr>;
  using PendingRelayMessageQueue =
      base::queue<std::pair<ports::NodeName, Channel::MessagePtr>>;

  NodeChannel(Delegate* delegate,
              ConnectionParams connection_params,
              Channel::HandlePolicy channel_handle_policy,
              scoped_refptr<base::TaskRunner> io_task_runner,
              const ProcessErrorCallback& process_error_callback);
  ~NodeChannel() override;

  // Creates a BrokerHost to satisfy a |BindBrokerHost()| request from the other
  // end of the channel.
  void CreateAndBindLocalBrokerHost(PlatformHandle broker_host_handle);

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;
  void OnChannelError(Channel::Error error) override;

  void WriteChannelMessage(Channel::MessagePtr message);

  Delegate* const delegate_;
  const scoped_refptr<base::TaskRunner> io_task_runner_;
  const ProcessErrorCallback process_error_callback_;

  base::Lock channel_lock_;
  scoped_refptr<Channel> channel_;

  // Must only be accessed from |io_task_runner_|'s thread.
  ports::NodeName remote_node_name_;

  base::Lock remote_process_handle_lock_;
  ScopedProcessHandle remote_process_handle_;

  DISALLOW_COPY_AND_ASSIGN(NodeChannel);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_NODE_CHANNEL_H_
