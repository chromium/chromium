// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/socket_utils_posix.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <vector>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(SocketUtilsPosixTest, ZeroBytePayloadDropsHandles) {
  PlatformChannel channel;
  base::ScopedFD send_fd =
      channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD();
  base::ScopedFD recv_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  std::vector<base::ScopedFD> send_fds;
  for (size_t i = 0; i < kMaxSendmsgHandles; ++i) {
    int pair[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    IGNORE_EINTR(close(pair[1]));
    send_fds.emplace_back(pair[0]);
  }

  // Sending zero data bytes with handles: sendmsg() returns 0, but the kernel
  // drops the ancillary SCM_RIGHTS descriptors when there is no payload.
  struct iovec iov = {nullptr, 0};
  ssize_t result = SendmsgWithHandles(send_fd.get(), &iov, 1, send_fds);
  ASSERT_EQ(0, result);

  char read_buf[16];
  std::vector<base::ScopedFD> recv_fds;
  ssize_t recv_result = SocketRecvmsg(recv_fd.get(), read_buf, sizeof(read_buf),
                                      &recv_fds, /*block=*/false);

  // Verifies that no data or handles were queued by the kernel (EAGAIN).
  ASSERT_EQ(-1, recv_result);
  ASSERT_EQ(EAGAIN, errno);
  ASSERT_TRUE(recv_fds.empty());
}

TEST(SocketUtilsPosixTest, TwoMessagesZeroByteThenOneByteWithHandles) {
  PlatformChannel channel;
  base::ScopedFD send_fd =
      channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD();
  base::ScopedFD recv_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  std::vector<base::ScopedFD> send_fds_1;
  std::vector<base::ScopedFD> peer_fds_1;
  std::vector<base::ScopedFD> send_fds_2;
  std::vector<base::ScopedFD> peer_fds_2;

  for (size_t i = 0; i < kMaxSendmsgHandles; ++i) {
    int pair1[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, pair1));
    send_fds_1.emplace_back(pair1[0]);
    peer_fds_1.emplace_back(pair1[1]);

    int pair2[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, pair2));
    send_fds_2.emplace_back(pair2[0]);
    peer_fds_2.emplace_back(pair2[1]);

    // Tag each FD by writing a distinct byte through its peer.
    char tag1 = '1';
    ASSERT_EQ(1, HANDLE_EINTR(write(peer_fds_1[i].get(), &tag1, 1)));
    char tag2 = '2';
    ASSERT_EQ(1, HANDLE_EINTR(write(peer_fds_2[i].get(), &tag2, 1)));
  }

  // Message 1: 0 data bytes + 128 FDs (tagged with '1').
  struct iovec iov_1 = {nullptr, 0};
  ssize_t res1 = SendmsgWithHandles(send_fd.get(), &iov_1, 1, send_fds_1);
  ASSERT_EQ(0, res1);

  // Message 2: 1 data byte ('A') + 128 FDs (tagged with '2').
  char payload_2 = 'A';
  struct iovec iov_2 = {&payload_2, 1};
  ssize_t res2 = SendmsgWithHandles(send_fd.get(), &iov_2, 1, send_fds_2);
  ASSERT_EQ(1, res2);

  // Read from receiver.
  char read_buf[16] = {};
  std::vector<base::ScopedFD> recv_fds;
  ssize_t recv_result = SocketRecvmsg(recv_fd.get(), read_buf, sizeof(read_buf),
                                      &recv_fds, /*block=*/true);

  EXPECT_EQ(1, recv_result);
  EXPECT_EQ('A', read_buf[0]);
  ASSERT_EQ(kMaxSendmsgHandles, recv_fds.size());

  // Explicitly verify each received descriptor reads the Message 2 tag ('2').
  for (size_t i = 0; i < recv_fds.size(); ++i) {
    char tag = 0;
    ASSERT_EQ(1, HANDLE_EINTR(read(recv_fds[i].get(), &tag, 1)));
    EXPECT_EQ('2', tag) << "FD at index " << i << " was not from Message 2!";
  }
}

}  // namespace
}  // namespace mojo
