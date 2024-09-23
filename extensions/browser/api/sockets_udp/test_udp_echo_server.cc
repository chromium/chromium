// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_udp_socket.h"
#include "services/network/test/udp_socket_test_util.h"

namespace extensions {

class TestUdpSocketListenerImpl : public network::test::UDPSocketListenerImpl {
 public:
  TestUdpSocketListenerImpl() = default;
  ~TestUdpSocketListenerImpl() override = default;

  TestUdpSocketListenerImpl(const TestUdpSocketListenerImpl&) = delete;
  TestUdpSocketListenerImpl& operator=(const TestUdpSocketListenerImpl&) =
      delete;

  using OnReceivedCallback = base::OnceCallback<
      void(int32_t, net::IPEndPoint, base::span<const uint8_t>)>;
  void SetOnReceivedCallback(OnReceivedCallback on_received_callback) {
    on_received_callback_ = std::move(on_received_callback);
  }

 private:
  void OnReceived(int32_t result,
                  const std::optional<net::IPEndPoint>& src_addr,
                  std::optional<base::span<const uint8_t>> data) override {
    if (on_received_callback_) {
      std::move(on_received_callback_)
          .Run(result, src_addr.value(), data.value());
    }
  }

  OnReceivedCallback on_received_callback_;
};

class TestUdpEchoServer::Core {
 public:
  Core() : udp_listener_receiver_(&listener_impl_) {}
  ~Core() = default;

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void Start(network::mojom::NetworkContext* network_context,
             net::HostPortPair* host_port_pair,
             int* result) {
    network_context->CreateUDPSocket(
        server_socket_.BindNewPipeAndPassReceiver(),
        udp_listener_receiver_.BindNewPipeAndPassRemote());

    server_socket_.set_disconnect_handler(
        base::BindLambdaForTesting([]() { NOTREACHED_IN_MIGRATION(); }));

    net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 0);
    auto server_helper =
        std::make_unique<network::test::UDPSocketTestHelper>(&server_socket_);
    *result = server_helper->BindSync(server_addr, nullptr, &server_addr);
    *host_port_pair = net::HostPortPair::FromIPEndPoint(server_addr);
    SetUpRead();
  }

  void SetUpRead() {
    // Set up server socket and listener to expect another read.
    server_socket_->ReceiveMore(1);
    listener_impl_.SetOnReceivedCallback(base::BindOnce(
        &TestUdpEchoServer::Core::OnRead, core_weak_ptr_factory_.GetWeakPtr()));
  }

  void OnRead(int32_t result,
              net::IPEndPoint src_addr,
              base::span<const uint8_t> data) {
    // Consider read errors fatal, to avoid getting into a read error loop.
    if (result < 0) {
      LOG(ERROR) << "Error reading from socket: " << net::ErrorToString(result);
      return;
    }

    server_socket_->SendTo(
        src_addr, data,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        base::BindOnce(&TestUdpEchoServer::Core::OnSend,
                       core_weak_ptr_factory_.GetWeakPtr(), result));
  }

  void OnSend(int32_t read_result, int32_t send_result) {
    if (send_result < 0) {
      LOG(ERROR) << "Error writing to socket: "
                 << net::ErrorToString(send_result);
    } else if (send_result != read_result) {
      LOG(ERROR) << "Failed to write entire message. Expected " << read_result
                 << ", but wrote " << send_result;
    }

    SetUpRead();
  }

 private:
  mojo::Remote<network::mojom::UDPSocket> server_socket_;
  std::unique_ptr<network::test::UDPSocketTestHelper> server_socket_helper_;
  mojo::Receiver<network::mojom::UDPSocketListener> udp_listener_receiver_;
  TestUdpSocketListenerImpl listener_impl_;

  base::WeakPtrFactory<TestUdpEchoServer::Core> core_weak_ptr_factory_{this};
};

TestUdpEchoServer::TestUdpEchoServer() = default;

TestUdpEchoServer::~TestUdpEchoServer() {
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             std::move(core_));
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

bool TestUdpEchoServer::Start(network::mojom::NetworkContext* network_context,
                              net::HostPortPair* host_port_pair) {
  core_ = std::make_unique<Core>();

  int result = net::ERR_UNEXPECTED;
  core_->Start(network_context, host_port_pair, &result);

  return result == net::OK;
}

}  // namespace extensions
