// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/platform/platform_channel_server.h"

#include <optional>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class RunOnDestruction {
 public:
  template <typename Fn>
  explicit RunOnDestruction(Fn fn)
      : callback_(base::BindLambdaForTesting(fn)) {}
  RunOnDestruction(RunOnDestruction&&) = default;
  ~RunOnDestruction() {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  base::OnceClosure callback_;
};

class TestChannel : public core::Channel::Delegate {
 public:
  explicit TestChannel(PlatformChannelEndpoint endpoint)
      : io_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        channel_(
            core::Channel::Create(this,
                                  core::ConnectionParams{std::move(endpoint)},
                                  core::Channel::HandlePolicy::kRejectHandles,
                                  io_task_runner_)) {
    channel_->Start();
  }

  ~TestChannel() override { CHECK(stopped_); }

  void Stop() {
    CHECK(!stopped_);
    stopped_ = true;

    // We pump the IO task queue after ShutDown() to ensure completion, as
    // Channel implementions post a cleanup task there.
    base::RunLoop shutdown_flush;
    channel_->ShutDown();
    io_task_runner_->PostTask(FROM_HERE, shutdown_flush.QuitClosure());
    shutdown_flush.Run();
  }

  void SendMessage(const std::string& message) {
    auto data = base::make_span(
        reinterpret_cast<const uint8_t*>(message.data()), message.size());
    channel_->Write(core::Channel::Message::CreateIpczMessage(data, {}));
  }

  std::string WaitForSingleMessage() {
    wait_for_message_.Run();
    CHECK(received_message_);
    return *received_message_;
  }

  // core::Channel::Delegate:
  bool IsIpczTransport() const override {
    // We use Channel in ipcz mode because it's simpler. Doesn't matter if
    // MojoIpcz is actually enabled.
    return true;
  }

  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override {
    received_message_ =
        std::string(static_cast<const char*>(payload), payload_size);
    std::move(quit_).Run();
  }

  void OnChannelError(core::Channel::Error error) override {}

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  const scoped_refptr<core::Channel> channel_;
  base::RunLoop wait_for_message_;
  base::OnceClosure quit_{wait_for_message_.QuitClosure()};
  std::optional<std::string> received_message_;
  bool stopped_ = false;
};

class PlatformChannelServerTest : public testing::Test {
 public:
  PlatformChannelServerTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~PlatformChannelServerTest() override = default;

  using NamedChannelDetails = std::tuple<PlatformChannelServerEndpoint,
                                         NamedPlatformChannel::ServerName>;
  NamedChannelDetails CreateRandomChannel() {
    NamedPlatformChannel::Options options;
#if BUILDFLAG(IS_POSIX)
    options.socket_dir = temp_dir_.GetPath();
#endif
    NamedPlatformChannel channel(options);
    return {channel.TakeServerEndpoint(), channel.GetServerName()};
  }

  PlatformChannelServer& server() { return server_; }

  void VerifyEndToEndConnection(PlatformChannelEndpoint a,
                                PlatformChannelEndpoint b) {
    TestChannel channel_a(std::move(a));
    TestChannel channel_b(std::move(b));

    const std::string kMessage1 = "Hello, world?";
    const std::string kMessage2 = "Oh, hi world.";
    channel_a.SendMessage(kMessage1);
    channel_b.SendMessage(kMessage2);
    EXPECT_EQ(kMessage2, channel_a.WaitForSingleMessage());
    EXPECT_EQ(kMessage1, channel_b.WaitForSingleMessage());

    channel_a.Stop();
    channel_b.Stop();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  PlatformChannelServer server_;
};

TEST_F(PlatformChannelServerTest, ConnectAfterListen) {
  // Basic test that a client can connect after Listen() and the server will
  // invoke its user-provided callback with a new functioning endpoint.
  auto [server_endpoint, name] = CreateRandomChannel();

  base::RunLoop loop;
  PlatformChannelEndpoint endpoint_a;
  EXPECT_TRUE(server().Listen(
      std::move(server_endpoint),
      base::BindLambdaForTesting([&](PlatformChannelEndpoint endpoint) {
        endpoint_a = std::move(endpoint);
        loop.Quit();
      })));
  auto endpoint_b = NamedPlatformChannel::ConnectToServer(name);
  EXPECT_TRUE(endpoint_b.is_valid());
  loop.Run();
  VerifyEndToEndConnection(std::move(endpoint_a), std::move(endpoint_b));
}

TEST_F(PlatformChannelServerTest, ConnectBeforeListen) {
  // Basic test that a client can connect *before* Listen() and the server will
  // still invoke its user-provided callback with a new functioning endpoint.
  auto [server_endpoint, name] = CreateRandomChannel();

  base::RunLoop loop;
  auto endpoint_a = NamedPlatformChannel::ConnectToServer(name);
  EXPECT_TRUE(endpoint_a.is_valid());
  PlatformChannelEndpoint endpoint_b;
  EXPECT_TRUE(server().Listen(
      std::move(server_endpoint),
      base::BindLambdaForTesting([&](PlatformChannelEndpoint endpoint) {
        endpoint_b = std::move(endpoint);
        loop.Quit();
      })));
  loop.Run();
  VerifyEndToEndConnection(std::move(endpoint_a), std::move(endpoint_b));
}

TEST_F(PlatformChannelServerTest, WaitForConnection) {
  // Tests the static WaitForConnection() helper.
  auto [server_endpoint, name] = CreateRandomChannel();

  base::RunLoop loop;
  auto endpoint_a = NamedPlatformChannel::ConnectToServer(name);
  PlatformChannelEndpoint endpoint_b;
  PlatformChannelServer::WaitForConnection(
      std::move(server_endpoint),
      base::BindLambdaForTesting([&](PlatformChannelEndpoint endpoint) {
        endpoint_b = std::move(endpoint);
        loop.Quit();
      }));
  loop.Run();
  VerifyEndToEndConnection(std::move(endpoint_a), std::move(endpoint_b));
}

TEST_F(PlatformChannelServerTest, NoCallbackAfterListenConnectStop) {
  // Tests that the ConnectionCallback is never invoked after Stop(), even if
  // we Listen() and the client connects immediately before the Stop() call.
  auto [server_endpoint, name] = CreateRandomChannel();
  bool callback_invoked = false;
  bool callback_destroyed = false;
  base::RunLoop loop;
  EXPECT_TRUE(server().Listen(
      std::move(server_endpoint),
      base::BindOnce(
          // This callback should never run and should be destroyed when we
          // Stop() below.
          [](RunOnDestruction, bool* callback_invoked,
             PlatformChannelEndpoint endpoint) { *callback_invoked = true; },
          // When the above callback is destroyed, this one will run.
          RunOnDestruction([&] {
            callback_destroyed = true;
            loop.Quit();
          }),
          &callback_invoked)));
  auto endpoint = NamedPlatformChannel::ConnectToServer(name);
  server().Stop();
  loop.Run();
  EXPECT_TRUE(callback_destroyed);
  EXPECT_FALSE(callback_invoked);
  EXPECT_TRUE(endpoint.is_valid());
}

TEST_F(PlatformChannelServerTest, NoCallbackAfterConnectListenStop) {
  // Tests that the ConnectionCallback is never invoked after Stop(), even if
  // the client connects before a Listen() which immediately precedes the Stop()
  // call.
  auto [server_endpoint, name] = CreateRandomChannel();
  bool callback_invoked = false;
  bool callback_destroyed = false;
  base::RunLoop loop;
  auto endpoint = NamedPlatformChannel::ConnectToServer(name);
  EXPECT_TRUE(endpoint.is_valid());
  EXPECT_TRUE(server().Listen(
      std::move(server_endpoint),
      base::BindOnce(
          // This callback should never run and should be destroyed when we
          // Stop() below.
          [](RunOnDestruction, bool* callback_invoked,
             PlatformChannelEndpoint endpoint) { *callback_invoked = true; },
          // When the above callback is destroyed, this one will run.
          RunOnDestruction([&] {
            callback_destroyed = true;
            loop.Quit();
          }),
          &callback_invoked)));
  server().Stop();
  loop.Run();
  EXPECT_TRUE(callback_destroyed);
  EXPECT_FALSE(callback_invoked);
}

}  // namespace
}  // namespace mojo
