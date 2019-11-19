// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_channel.h"  // nogncheck
#include "mojo/public/cpp/platform/platform_channel.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

using mojo::core::Channel;
using mojo::core::ConnectionParams;
using mojo::core::ports::NodeName;

// Implementation of NodeChannel::Delegate which does nothing. All of the
// interesting NodeChannel control message message parsing is done by
// NodeChannel by the time any of the delegate methods are invoked, so there's
// no need for this to do any work.
class FakeNodeChannelDelegate : public mojo::core::NodeChannel::Delegate {
 public:
  FakeNodeChannelDelegate() = default;
  ~FakeNodeChannelDelegate() override = default;

  void OnAcceptInvitee(const NodeName& from_node,
                       const NodeName& inviter_name,
                       const NodeName& token) override {}
  void OnAcceptInvitation(const NodeName& from_node,
                          const NodeName& token,
                          const NodeName& invitee_name) override {}
  void OnAddBrokerClient(const NodeName& from_node,
                         const NodeName& client_name,
                         base::ProcessHandle process_handle) override {}
  void OnBrokerClientAdded(const NodeName& from_node,
                           const NodeName& client_name,
                           mojo::PlatformHandle broker_channel) override {}
  void OnAcceptBrokerClient(const NodeName& from_node,
                            const NodeName& broker_name,
                            mojo::PlatformHandle broker_channel) override {}
  void OnEventMessage(const NodeName& from_node,
                      Channel::MessagePtr message) override {}
  void OnRequestPortMerge(
      const NodeName& from_node,
      const mojo::core::ports::PortName& connector_port_name,
      const std::string& token) override {}
  void OnRequestIntroduction(const NodeName& from_node,
                             const NodeName& name) override {}
  void OnIntroduce(const NodeName& from_node,
                   const NodeName& name,
                   mojo::PlatformHandle channel_handle) override {}
  void OnBroadcast(const NodeName& from_node,
                   Channel::MessagePtr message) override {}
#if defined(OS_WIN)
  void OnRelayEventMessage(const NodeName& from_node,
                           base::ProcessHandle from_process,
                           const NodeName& destination,
                           Channel::MessagePtr message) override {}
  void OnEventMessageFromRelay(const NodeName& from_node,
                               const NodeName& source_node,
                               Channel::MessagePtr message) override {}
#endif
  void OnAcceptPeer(const NodeName& from_node,
                    const NodeName& token,
                    const NodeName& peer_name,
                    const mojo::core::ports::PortName& port_name) override {}
  void OnChannelError(const NodeName& node,
                      mojo::core::NodeChannel* channel) override {}
};

// A fake delegate for the sending Channel endpoint. The sending Channel is not
// being fuzzed and won't receive any interesting messages, so this doesn't need
// to do anything.
class FakeChannelDelegate : public Channel::Delegate {
 public:
  FakeChannelDelegate() = default;
  ~FakeChannelDelegate() override = default;

  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<mojo::PlatformHandle> handles) override {}
  void OnChannelError(Channel::Error error) override {}
};

// Message deserialization may register handles in the global handle table. We
// need to initialize Core for that to be OK.
struct Environment {
  Environment() : main_thread_task_executor(base::MessagePumpType::IO) {
    mojo::core::InitializeCore();
  }

  base::SingleThreadTaskExecutor main_thread_task_executor;
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  static base::NoDestructor<Environment> environment;

  // Platform-specific implementation of an OS IPC primitive that is normally
  // used to carry messages between processes.
  mojo::PlatformChannel channel;

  FakeNodeChannelDelegate receiver_delegate;
  auto receiver = mojo::core::NodeChannel::Create(
      &receiver_delegate, ConnectionParams(channel.TakeLocalEndpoint()),
      Channel::HandlePolicy::kRejectHandles,
      environment->main_thread_task_executor.task_runner(), base::DoNothing());

#if defined(OS_WIN)
  // On Windows, it's important that the receiver behaves like a broker process
  // receiving messages from a non-broker process. This is because that case can
  // safely handle invalid HANDLE attachments without crashing. The same is not
  // true for messages going in the reverse direction (where the non-broker
  // receiver has to assume that the broker has already duplicated the HANDLE
  // into the non-broker's process), but fuzzing that direction is not
  // interesting since a compromised broker process has much bigger problems.
  //
  // Note that in order for this hack to work properly, the remote process
  // handle needs to be a "real" process handle rather than the pseudo-handle
  // returned by GetCurrentProcessHandle(). Hence the use of OpenProcess().
  receiver->SetRemoteProcessHandle(mojo::core::ScopedProcessHandle(
      ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, ::GetCurrentProcessId())));
#endif

  receiver->Start();

  // We only use a Channel for the sender side, since it allows us to easily
  // encode and transmit raw messages. For this fuzzer, we allocate a valid
  // Channel Message with a valid header, but fill its payload contents with
  // fuzz. Such messages will always reach the receiving NodeChannel to be
  // parsed further.
  FakeChannelDelegate sender_delegate;
  auto sender = Channel::Create(
      &sender_delegate, ConnectionParams(channel.TakeRemoteEndpoint()),
      Channel::HandlePolicy::kRejectHandles,
      environment->main_thread_task_executor.task_runner());
  sender->Start();
  auto message = std::make_unique<Channel::Message>(size, 0 /* num_handles */);
  std::copy(data, data + size,
            static_cast<unsigned char*>(message->mutable_payload()));
  sender->Write(std::move(message));

  // Make sure |receiver| does whatever work it's gonna do in response to our
  // message. By the time the loop goes idle, all parsing will be done.
  base::RunLoop().RunUntilIdle();

  // Clean up our channels so we don't leak the underlying OS primitives.
  sender->ShutDown();
  sender.reset();
  receiver->ShutDown();
  receiver.reset();
  base::RunLoop().RunUntilIdle();

  return 0;
}
