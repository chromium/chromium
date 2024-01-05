// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/apple/mach_logging.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/core/channel.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/test/data/channel_mac/channel_mac.pb.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "testing/libfuzzer/fuzzers/mach/mach_message_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace {

class ChannelMacFuzzer {
 public:
  ChannelMacFuzzer() {
    mojo::core::InitializeCore();

    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() {
    return io_task_executor_.task_runner();
  }

 private:
  base::SingleThreadTaskExecutor io_task_executor_{base::MessagePumpType::IO};
};

class FakeChannelDelegate : public mojo::core::Channel::Delegate {
 public:
  FakeChannelDelegate() = default;
  ~FakeChannelDelegate() override = default;

  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<mojo::PlatformHandle> handles) override {}
  void OnChannelError(mojo::core::Channel::Error error) override {}
};

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(mojo_fuzzer::ChannelMac proto) {
  static ChannelMacFuzzer environment;

  mojo::PlatformChannel platform_channel;
  mojo::PlatformChannelEndpoint fuzzed_endpoint;
  mojo::PlatformChannelEndpoint other_endpoint;

  mach_port_t send_port = platform_channel.local_endpoint()
                              .platform_handle()
                              .GetMachSendRight()
                              .get();

  if (proto.endpoint_type() == mojo_fuzzer::ChannelMac_EndpointType_LOCAL) {
    fuzzed_endpoint = platform_channel.TakeLocalEndpoint();
    other_endpoint = platform_channel.TakeRemoteEndpoint();
  } else if (proto.endpoint_type() ==
             mojo_fuzzer::ChannelMac_EndpointType_REMOTE) {
    fuzzed_endpoint = platform_channel.TakeRemoteEndpoint();
    other_endpoint = platform_channel.TakeLocalEndpoint();
  }

  FakeChannelDelegate delegate;

  auto channel = mojo::core::Channel::Create(
      &delegate, mojo::core::ConnectionParams(std::move(fuzzed_endpoint)),
      mojo::core::Channel::HandlePolicy::kAcceptHandles,
      environment.io_task_runner());
  channel->Start();

  // If the handshake is not being fuzzed, establish a peer Channel that will
  // put |channel| into a good state by performing the handshake.
  scoped_refptr<mojo::core::Channel> peer_channel;
  if (!proto.fuzz_handshake()) {
    peer_channel = mojo::core::Channel::Create(
        &delegate, mojo::core::ConnectionParams(std::move(other_endpoint)),
        mojo::core::Channel::HandlePolicy::kAcceptHandles,
        environment.io_task_runner());
    peer_channel->Start();
  }

  base::RunLoop().RunUntilIdle();

  // Save off any ports that were sent as part of a message until after the
  // channel has been shut down.
  std::list<mach_fuzzer::SendablePort> ports_to_destroy;

  for (auto& message : *proto.mutable_messages()) {
    if (message.HasExtension(mojo_fuzzer::MojoMessage::mojo_message)) {
      // Mojo message data for inline Mach messages is
      // [data_length_uint64][data_bytes].
      const auto& mojo_message =
          message.GetExtension(mojo_fuzzer::MojoMessage::mojo_message);

      // If the fuzz data do not specify an explicit length, just use the byte
      // length.
      uint64_t data_length = mojo_message.has_data_length()
                                 ? mojo_message.data_length()
                                 : mojo_message.data().size();
      std::string data;
      data.append(reinterpret_cast<const char*>(&data_length),
                  sizeof(data_length));
      data.append(mojo_message.data().begin(), mojo_message.data().end());
      message.set_data(data);
    }

    mach_fuzzer::SendResult result = SendMessage(send_port, message);

    std::move(result.message.ports.begin(), result.message.ports.end(),
              std::back_inserter(ports_to_destroy));
  }

  base::RunLoop().RunUntilIdle();

  channel->ShutDown();
  channel.reset();

  if (peer_channel) {
    peer_channel->ShutDown();
    peer_channel.reset();
  }

  base::RunLoop().RunUntilIdle();
}
