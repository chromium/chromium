// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"

namespace extensions {

// Size of UDP buffer.  Max allowed read size.
static const size_t kIOBufferSize = 4096;

class TestUdpEchoServer::Core {
 public:
  Core() = default;
  ~Core() = default;

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void Start(net::HostPortPair* host_port_pair, int* result) {
    udp_server_socket_ = std::make_unique<net::UDPServerSocket>(
        nullptr /* net_log */, net::NetLogSource());
    io_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kIOBufferSize);

    *result = udp_server_socket_->Listen(
        net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0 /* port */));
    if (*result != net::OK) {
      LOG(ERROR) << "UDP listen failed.";
      udp_server_socket_.reset();
      return;
    }

    net::IPEndPoint local_address;
    *result = udp_server_socket_->GetLocalAddress(&local_address);
    if (*result != net::OK) {
      LOG(ERROR) << "Failed to get local address.";
      udp_server_socket_.reset();
      return;
    }

    *host_port_pair = net::HostPortPair::FromIPEndPoint(local_address);
    ReadLoop();
  }

  void ReadLoop() {
    int result = udp_server_socket_->RecvFrom(
        io_buffer_.get(), io_buffer_->size(), &recv_from_address_,
        base::BindOnce(&Core::OnReadComplete, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnReadComplete(result);
  }

  void OnReadComplete(int result) {
    if (result < 0) {
      // Consider read errors fatal, to avoid getting into a read error loop.
      LOG(ERROR) << "Error reading from socket: " << net::ErrorToString(result);
      return;
    }

    int send_result = udp_server_socket_->SendTo(
        io_buffer_.get(), result, recv_from_address_,
        base::BindOnce(&Core::OnSendComplete, base::Unretained(this), result));
    if (send_result != net::ERR_IO_PENDING)
      OnSendComplete(result, send_result);
  }

  void OnSendComplete(int expected_result, int result) {
    // Don't consider write errors to be fatal.
    if (result < 0) {
      LOG(ERROR) << "Error writing to socket: " << net::ErrorToString(result);
    } else if (result != expected_result) {
      LOG(ERROR) << "Failed to write entire message. Expected "
                 << expected_result << ", but wrote " << result;
    }
    ReadLoop();
  }

 private:
  std::unique_ptr<net::UDPServerSocket> udp_server_socket_;

  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  net::IPEndPoint recv_from_address_;
};

TestUdpEchoServer::TestUdpEchoServer() = default;

TestUdpEchoServer::~TestUdpEchoServer() {
  if (io_thread_) {
    io_thread_->task_runner()->DeleteSoon(FROM_HERE, std::move(core_));
    base::RunLoop run_loop;
    io_thread_->task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    io_thread_.reset();
  }
}

bool TestUdpEchoServer::Start(net::HostPortPair* host_port_pair) {
  DCHECK(!io_thread_);

  core_ = std::make_unique<Core>();

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  io_thread_ = std::make_unique<base::Thread>("EmbeddedTestServer IO Thread");
  CHECK(io_thread_->StartWithOptions(std::move(thread_options)));
  CHECK(io_thread_->WaitUntilThreadStarted());

  base::RunLoop run_loop;
  int result = net::ERR_UNEXPECTED;
  io_thread_->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&TestUdpEchoServer::Core::Start,
                     base::Unretained(core_.get()), host_port_pair, &result),
      run_loop.QuitClosure());
  run_loop.Run();

  return result == net::OK;
}

}  // namespace extensions
