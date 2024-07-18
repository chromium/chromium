// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_channel.h"  // nogncheck
#include "mojo/core/test/mock_node_channel_delegate.h"
#include "mojo/public/cpp/platform/platform_channel.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

using mojo::core::Channel;
using mojo::core::ConnectionParams;
using mojo::core::ports::NodeName;

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

  mojo::core::MockNodeChannelDelegate receiver_delegate;
  auto receiver = mojo::core::NodeChannel::Create(
      &receiver_delegate, ConnectionParams(channel.TakeLocalEndpoint()),
      Channel::HandlePolicy::kRejectHandles,
      environment->main_thread_task_executor.task_runner(), base::DoNothing());

#if BUILDFLAG(IS_WIN)
  // On Windows, it's important that the receiver behaves like a broker process
  // receiving messages from a non-broker process. This is because that case can
  // safely handle invalid HANDLE attachments without crashing. The same is not
  // true for messages going in the reverse direction (where the non-broker
  // receiver has to assume that the broker has already duplicated the HANDLE
  // into the non-broker's process), but fuzzing that direction is not
  // interesting since a compromised broker process has much bigger problems.
  receiver->SetRemoteProcessHandle(base::Process::Current());
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
  auto message = Channel::Message::CreateMessage(size, 0 /* num_handles */);
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
