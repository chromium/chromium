// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_socket_utils.h"

#include <fcntl.h>

#include <array>

#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

// A test fixture is used to ensure that all sockets are closed down gracefully
// upon test completion.  Also provides a convenient API to Bind not presently
// available in QuicSocketUtils.
class QuicSocketUtilsTest : public QuicTest {
 protected:
  ~QuicSocketUtilsTest() override {
    for (int fd : open_sockets_) {
      close(fd);
    }
  }

  int CreateUDPSocket(const QuicSocketAddress& address) {
    bool overflow_supported = false;
    int fd = QuicSocketUtils::CreateUDPSocket(
        address, /*receive_buffer_size =*/kDefaultSocketReceiveBuffer,
        /*send_buffer_size =*/kDefaultSocketReceiveBuffer, &overflow_supported);
    if (fd != -1) {
      open_sockets_.push_back(fd);
    }
    return fd;
  }

  int CreateBoundUDPSocket(QuicSocketAddress* address) {
    int fd = CreateUDPSocket(*address);
    *address = BindSocket(fd, *address);
    if (!address->IsInitialized()) {
      close(fd);
      fd = -1;
    }
    return fd;
  }

  QuicSocketAddress BindSocket(int fd, const QuicSocketAddress& address) {
    QuicSocketAddress bound_address;

    if (fd == -1) {
      return bound_address;
    }

    sockaddr_storage bind_addr_native = address.generic_address();
    socklen_t bind_addr_size = 0;

    switch (address.host().address_family()) {
      case IpAddressFamily::IP_V4:
        bind_addr_size = sizeof(struct sockaddr_in);
        break;
      case IpAddressFamily::IP_V6:
        bind_addr_size = sizeof(struct sockaddr_in6);
        break;
      case IpAddressFamily::IP_UNSPEC:
        QUIC_LOG(FATAL) << "Unspecified IP address family";
    }

    int rc = bind(fd, reinterpret_cast<sockaddr*>(&bind_addr_native),
                  bind_addr_size);
    if (rc != 0) {
      QUIC_LOG(ERROR) << "Failed to bind socket to " << address.ToString()
                      << ": " << strerror(errno);
      return bound_address;
    }

    rc = bound_address.FromSocket(fd);
    if (rc != 0) {
      QUIC_LOG(ERROR) << "Failed to get bound socket address from fd: "
                      << strerror(errno);
      bound_address = QuicSocketAddress();
    }
    return bound_address;
  }

 private:
  std::vector<int> open_sockets_;
};

// This test verifies that QuicSocketUtils creates a non-blocking socket
// successfully by seeing if a read blocks.
TEST_F(QuicSocketUtilsTest, NonBlockingSocket) {
  std::array<char, 512> buffer;

  QuicIpAddress localhost = QuicIpAddress::Loopback4();
  QuicSocketAddress addr(localhost, 0);

  int fd = CreateUDPSocket(addr);
  ASSERT_NE(-1, fd);

  int fd_flags = fcntl(fd, F_GETFL, 0);

  // Assert so that the test errors out quickly rather than blocking below and
  // relying on timeouts.
  ASSERT_TRUE(fd_flags & O_NONBLOCK) << "Socket not reporting as non-blocking";

  QuicIpAddress target_server_addr;
  auto walltimestamp = QuicWallTime::Zero();
  QuicSocketAddress remote_addr;
  int bytes_read = QuicSocketUtils::ReadPacket(fd, buffer.data(), buffer.size(),
                                               nullptr, &target_server_addr,
                                               &walltimestamp, &remote_addr);
  EXPECT_EQ(-1, bytes_read);
}

// This test verifies that we can successfully WritePacket/ReadPacket between
// two localhost sockets.
TEST_F(QuicSocketUtilsTest, PacketRoundTrip) {
  QuicIpAddress localhost = QuicIpAddress::Loopback4();
  QuicSocketAddress client_addr(localhost, 0);
  QuicSocketAddress server_addr(localhost, 0);

  int server_fd = CreateBoundUDPSocket(&server_addr);
  int client_fd = CreateUDPSocket(client_addr);

  ASSERT_NE(-1, server_fd);
  ASSERT_NE(-1, client_fd);

  {
    std::array<char, 512> write_buffer;
    for (size_t i = 0; i < write_buffer.size(); i++) {
      write_buffer[i] = static_cast<char>(i);
    }
    auto res = QuicSocketUtils::WritePacket(client_fd, write_buffer.data(),
                                            write_buffer.size(),
                                            QuicIpAddress(), server_addr);
    ASSERT_EQ(WRITE_STATUS_OK, res.status)
        << "Failed to write with error " << res.error_code;
    EXPECT_EQ(512, res.bytes_written);
  }

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(server_fd, &read_fds);

  timeval select_timeout;
  select_timeout.tv_sec = 5;
  select_timeout.tv_usec = 0;

  int select_rc =
      select(1 + server_fd, &read_fds, nullptr, nullptr, &select_timeout);
  EXPECT_EQ(select_rc, 1) << "server_fd didn't become read selectable: "
                          << errno;

  {
    std::array<char, 1024> read_buffer;
    QuicIpAddress target_server_addr;
    auto walltimestamp = QuicWallTime::Zero();
    QuicSocketAddress remote_addr;
    int bytes_read = QuicSocketUtils::ReadPacket(
        server_fd, read_buffer.data(), read_buffer.size(), nullptr,
        &target_server_addr, &walltimestamp, &remote_addr);
    EXPECT_EQ(512, bytes_read);
    for (int i = 0; i < bytes_read; i++) {
      EXPECT_EQ(static_cast<char>(i), read_buffer[i]);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
