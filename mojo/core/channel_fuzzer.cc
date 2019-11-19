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
#include "mojo/public/cpp/platform/platform_channel.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

// A fake delegate for each Channel endpoint. By the time an incoming message
// reaches a Delegate, all interesting message parsing at the lowest protocol
// layer has already been done by the receiving Channel implementation, so this
// doesn't need to do any work.
class FakeChannelDelegate : public mojo::core::Channel::Delegate {
 public:
  FakeChannelDelegate() = default;
  ~FakeChannelDelegate() override = default;

  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<mojo::PlatformHandle> handles) override {}
  void OnChannelError(mojo::core::Channel::Error error) override {}
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

  FakeChannelDelegate receiver_delegate;
  auto receiver = mojo::core::Channel::Create(
      &receiver_delegate,
      mojo::core::ConnectionParams(channel.TakeLocalEndpoint()),
      mojo::core::Channel::HandlePolicy::kRejectHandles,
      environment->main_thread_task_executor.task_runner());

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
  receiver->set_remote_process(mojo::core::ScopedProcessHandle(
      ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, ::GetCurrentProcessId())));
#endif

  receiver->Start();

  FakeChannelDelegate sender_delegate;
  auto sender = mojo::core::Channel::Create(
      &sender_delegate,
      mojo::core::ConnectionParams(channel.TakeRemoteEndpoint()),
      mojo::core::Channel::HandlePolicy::kRejectHandles,
      environment->main_thread_task_executor.task_runner());
  sender->Start();

  sender->Write(mojo::core::Channel::Message::CreateRawForFuzzing(
      base::make_span(data, size)));

  // Make sure |receiver| does whatever work it's gonna do in response to our
  // message. By the time the loop goes idle, all parsing will be done.
  base::RunLoop().RunUntilIdle();

  // Clean up our channels so we don't leak their underlying OS primitives.
  sender->ShutDown();
  sender.reset();
  receiver->ShutDown();
  receiver.reset();
  base::RunLoop().RunUntilIdle();

  return 0;
}
