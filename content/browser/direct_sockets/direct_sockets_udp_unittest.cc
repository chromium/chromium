// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_udp_socket_impl.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr uint8_t kData[] = "Message";

class MockNetworkContext;

enum class RecordedCall { kReceiveMore, kSend };

std::unique_ptr<network::mojom::UDPSocket> CreateMockUDPSocket(
    MockNetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext() = default;

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  void Record(RecordedCall call) { history_.push_back(std::move(call)); }

  const std::vector<RecordedCall>& history() const { return history_; }

 private:
  void CreateUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
      override {
    udp_socket_ =
        CreateMockUDPSocket(this, std::move(receiver), std::move(listener));
  }

  std::vector<RecordedCall> history_;
  std::unique_ptr<network::mojom::UDPSocket> udp_socket_;
};

class MockUDPSocket : public network::TestUDPSocket {
 public:
  MockUDPSocket(MockNetworkContext* network_context,
                mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
                mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
      : network_context_(network_context) {
    receiver_.Bind(std::move(receiver));
    listener_.Bind(std::move(listener));
  }

  ~MockUDPSocket() override = default;

 private:
  // network::mojom::UDPSocket:
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::OK,
                                  /*local_addr_out=*/absl::nullopt));
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override {
    network_context_->Record(RecordedCall::kReceiveMore);
  }

  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override {
    network_context_->Record(RecordedCall::kSend);
    std::move(callback).Run(net::OK);
  }

  const raw_ptr<MockNetworkContext> network_context_;
  mojo::Receiver<network::mojom::UDPSocket> receiver_{this};
  mojo::Remote<network::mojom::UDPSocketListener> listener_;
};

std::unique_ptr<network::mojom::UDPSocket> CreateMockUDPSocket(
    MockNetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  return std::make_unique<MockUDPSocket>(network_context, std::move(receiver),
                                         std::move(listener));
}

class MockListener : public network::mojom::UDPSocketListener {
  void OnReceived(int32_t result,
                  const absl::optional<net::IPEndPoint>& src_addr,
                  absl::optional<base::span<const uint8_t>> data) override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

class DirectSocketsUDPUnitTest : public testing::Test {};

TEST_F(DirectSocketsUDPUnitTest, Sending) {
  const uint32_t kNumAdditionalDatagrams = 5;
  base::test::SingleThreadTaskEnvironment task_environment;
  MockNetworkContext mock_network_context;
  MockListener listener;
  mojo::Receiver<network::mojom::UDPSocketListener> socket_listener_receiver{
      &listener};

  DirectUDPSocketImpl socket(
      &mock_network_context,
      socket_listener_receiver.BindNewPipeAndPassRemote());

  {
    base::RunLoop run_loop;
    const net::IPEndPoint remote_addr;
    network::mojom::UDPSocketOptionsPtr options;
    socket.Connect(
        remote_addr, std::move(options),

        base::BindLambdaForTesting(
            [&run_loop](int result, const absl::optional<net::IPEndPoint>&) {
              EXPECT_EQ(result, net::OK);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Successful calls.
  socket.ReceiveMore(kNumAdditionalDatagrams);
  for (unsigned i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    socket.Send(kData, base::BindLambdaForTesting([&run_loop](int32_t result) {
                  EXPECT_EQ(result, net::OK);
                  run_loop.Quit();
                }));
    run_loop.Run();
  }
  socket.Close();

  // Unsuccessful calls after Close.
  socket.ReceiveMore(kNumAdditionalDatagrams);
  {
    base::RunLoop run_loop;
    socket.Send(kData, base::BindLambdaForTesting([&run_loop](int32_t result) {
                  EXPECT_EQ(result, net::ERR_FAILED);
                  run_loop.Quit();
                }));
    run_loop.Run();
  }

  // Only the calls before Close reach the MockUDPSocket.
  DCHECK_EQ(4U, mock_network_context.history().size());
  EXPECT_EQ(mock_network_context.history()[0], RecordedCall::kReceiveMore);
  EXPECT_EQ(mock_network_context.history()[1], RecordedCall::kSend);
  EXPECT_EQ(mock_network_context.history()[2], RecordedCall::kSend);
  EXPECT_EQ(mock_network_context.history()[3], RecordedCall::kSend);
}

}  // namespace content
