// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/fuzzing_utils.h"
#include "mojo/public/cpp/platform/platform_channel.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  static base::NoDestructor<mojo::core::Environment> environment;
  mojo::PlatformChannel channel;
  mojo::core::FakeChannelDelegate receiver_delegate{/*is_ipcz_transport=*/true};
  using mojo::core::Channel;
  auto receiver =
      Channel::Create(&receiver_delegate,
                      mojo::core::ConnectionParams(channel.TakeLocalEndpoint()),
                      Channel::HandlePolicy::kRejectHandles,
                      environment->main_thread_task_executor.task_runner());
  receiver->Start();

  mojo::core::FakeChannelDelegate sender_delegate{/*is_ipcz_transport=*/true};
  auto sender = Channel::Create(
      &sender_delegate,
      mojo::core::ConnectionParams(channel.TakeRemoteEndpoint()),
      Channel::HandlePolicy::kRejectHandles,
      environment->main_thread_task_executor.task_runner());
  sender->Start();

  // SAFETY: required from fuzzer.
  auto payload = UNSAFE_BUFFERS(base::span(data, size));

  // Fuzz ipcz message parsing from raw data.
  sender->Write(Channel::Message::CreateRawForFuzzing(payload));
  base::RunLoop().RunUntilIdle();

  // Fuzz handling valid ipcz messages.
  auto message_type = Channel::Message::MessageType::NORMAL;
  constexpr size_t kMessageTypeSize = sizeof(message_type);
  if (payload.size() >= kMessageTypeSize) {
    message_type = static_cast<Channel::Message::MessageType>(
        base::U16FromLittleEndian(payload.first<kMessageTypeSize>()));
    payload = payload.subspan(kMessageTypeSize);
  }
  uint32_t sequence_number = 0;
  constexpr size_t kSequenceNumberTypeSize = sizeof(sequence_number);
  if (payload.size() >= kSequenceNumberTypeSize) {
    sequence_number =
        base::U32FromLittleEndian(payload.first<kSequenceNumberTypeSize>());
    payload = payload.subspan(kSequenceNumberTypeSize);
  }

  sender->Write(Channel::Message::CreateIpczMessage(payload, {}, message_type,
                                                    sequence_number));
  base::RunLoop().RunUntilIdle();

  sender->ShutDown();
  sender.reset();
  receiver->ShutDown();
  receiver.reset();
  base::RunLoop().RunUntilIdle();

  return 0;
}
