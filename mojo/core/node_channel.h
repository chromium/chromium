// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_NODE_CHANNEL_H_
#define MOJO_CORE_NODE_CHANNEL_H_

#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/embedder/process_error_callback.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/system_impl_export.h"

namespace mojo {
namespace core {

inline constexpr uint64_t kNodeCapabilityNone = 0;
inline constexpr uint64_t kNodeCapabilitySupportsUpgrade = 1;

inline constexpr uint32_t kNodeChannelHeaderSize = 8;

// Wraps a Channel to send and receive Node control messages.
class MOJO_SYSTEM_IMPL_EXPORT NodeChannel
    : public base::RefCountedDeleteOnSequence<NodeChannel>,
      public Channel::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
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
                                      PlatformHandle broker_channel,
                                      const uint64_t broker_capabilities) = 0;
    virtual void OnEventMessage(const ports::NodeName& from_node,
                                Channel::MessagePtr message) = 0;
    virtual void OnRequestPortMerge(const ports::NodeName& from_node,
                                    const ports::PortName& connector_port_name,
                                    const std::string& token) = 0;
    virtual void OnRequestIntroduction(const ports::NodeName& from_node,
                                       const ports::NodeName& name) = 0;
    virtual void OnIntroduce(const ports::NodeName& from_node,
                             const ports::NodeName& name,
                             PlatformHandle channel_handle,
                             const uint64_t remote_capabilities) = 0;
    virtual void OnBroadcast(const ports::NodeName& from_node,
                             Channel::MessagePtr message) = 0;
#if BUILDFLAG(IS_WIN)
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
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const ProcessErrorCallback& process_error_callback);

  NodeChannel(const NodeChannel&) = delete;
  NodeChannel& operator=(const NodeChannel&) = delete;

  static Channel::MessagePtr CreateEventMessage(size_t capacity,
                                                size_t payload_size,
                                                void** payload,
                                                size_t num_handles);

  // Retrieves address and size of an Event message's underlying message data.
  // Returns `false` if the message is not a valid Event message.
  static bool GetEventMessageData(Channel::Message& message,
                                  void** data,
                                  size_t* num_data_bytes);

  // Start receiving messages.
  void Start();

  // Permanently stop the channel from sending or receiving messages.
  void ShutDown();

  // Leaks the pipe handle instead of closing it on shutdown.
  void LeakHandleOnShutdown();

  // Invokes the bad message callback for this channel.  To avoid losing error
  // reports the caller should ensure that the channel |HasBadMessageHandler|
  // before calling |NotifyBadMessage|.
  void NotifyBadMessage(const std::string& error);

  // Returns whether the channel has a bad message handler.
  bool HasBadMessageHandler() { return !process_error_callback_.is_null(); }

  void SetRemoteProcessHandle(base::Process process_handle);
  bool HasRemoteProcessHandle();
  base::Process CloneRemoteProcessHandle();

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
                       base::Process process_handle);
  void BrokerClientAdded(const ports::NodeName& client_name,
                         PlatformHandle broker_channel);
  void AcceptBrokerClient(const ports::NodeName& broker_name,
                          PlatformHandle broker_channel,
                          const uint64_t broker_capabilities);
  void RequestPortMerge(const ports::PortName& connector_port_name,
                        const std::string& token);
  void RequestIntroduction(const ports::NodeName& name);
  void Introduce(const ports::NodeName& name,
                 PlatformHandle channel_handle,
                 uint64_t capabilities);
  void SendChannelMessage(Channel::MessagePtr message);
  void Broadcast(Channel::MessagePtr message);
  void BindBrokerHost(PlatformHandle broker_host_handle);

  uint64_t RemoteCapabilities() const;
  bool HasRemoteCapability(const uint64_t capability) const;
  void SetRemoteCapabilities(const uint64_t capability);

  uint64_t LocalCapabilities() const;
  bool HasLocalCapability(const uint64_t capability) const;
  void SetLocalCapabilities(const uint64_t capability);

#if BUILDFLAG(IS_WIN)
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

  void OfferChannelUpgrade();

 private:
  friend class base::RefCountedDeleteOnSequence<NodeChannel>;
  friend class base::DeleteHelper<NodeChannel>;

  using PendingMessageQueue = base::queue<Channel::MessagePtr>;
  using PendingRelayMessageQueue =
      base::queue<std::pair<ports::NodeName, Channel::MessagePtr>>;

  NodeChannel(Delegate* delegate,
              ConnectionParams connection_params,
              Channel::HandlePolicy channel_handle_policy,
              scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
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

  // This method is responsible for setting up the default set of capabilities
  // for this channel.
  void InitializeLocalCapabilities();

  // This dangling raw_ptr occurred in:
  // mojo_unittests: NodeChannelTest.MessagesCannotBeSmallerThanOldestVersion
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1425190/test-results?q=ExactID%3Aninja%3A%2F%2Fmojo%3Amojo_unittests%2FNodeChannelTest.MessagesCannotBeSmallerThanOldestVersion+VHash%3A589215eb23c7875a
  const raw_ptr<Delegate, FlakyDanglingUntriaged> delegate_;
  const ProcessErrorCallback process_error_callback_;

  base::Lock channel_lock_;
  scoped_refptr<Channel> channel_ GUARDED_BY(channel_lock_);

  // Must only be accessed from the owning task runner's thread.
  ports::NodeName remote_node_name_;

  uint64_t remote_capabilities_ = kNodeCapabilityNone;
  uint64_t local_capabilities_ = kNodeCapabilityNone;

  base::Lock remote_process_handle_lock_;
  base::Process remote_process_handle_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_NODE_CHANNEL_H_
