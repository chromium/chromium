// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "mojo/core/broker_host.h"
#include "mojo/core/broker_messages.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core {
namespace {

constexpr size_t kRegionSize = 64 * 1024;

void SetBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  CHECK_NE(flags, -1);
  CHECK_NE(fcntl(fd, F_SETFL, flags & ~O_NONBLOCK), -1);
}

// The result of reading a single message from a BrokerHost.
struct ReadResult {
  ssize_t num_bytes = -1;
  BrokerBufferResponse response = BrokerBufferResponse::kFailure;
  std::vector<base::ScopedFD> fds;
};

class BrokerTest : public testing::Test {
 protected:
  // Starts a BrokerHost on the main thread for one end of a new socket pair
  // and returns the other end. Both ends are blocking: the client creates the
  // pair and controls its mode, so this is the BrokerHost's worst case.
  base::ScopedFD StartBrokerHost() {
    PlatformChannel channel;
    base::ScopedFD host =
        channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD();
    base::ScopedFD client =
        channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
    SetBlocking(host.get());
    SetBlocking(client.get());
    // Make `host`'s send buffer as small as the kernel allows, so that a
    // client which stops reading fills it after a few responses. A single
    // response always fits.
    const int send_buffer_size = 1;
    CHECK_EQ(setsockopt(host.get(), SOL_SOCKET, SO_SNDBUF, &send_buffer_size,
                        sizeof(send_buffer_size)),
             0);
    new BrokerHost(std::move(host));  // Owns itself.
    return client;
  }

  // Runs `task` on a thread which may block while the main thread services the
  // BrokerHost, and returns the result.
  template <typename T>
  T RunBlocking(base::OnceCallback<T()> task) {
    base::test::TestFuture<T> future;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, std::move(task), future.GetCallback());
    return future.Take();
  }

  base::WritableSharedMemoryRegion AllocateRegion(Broker& broker,
                                                  size_t num_bytes) {
    return RunBlocking(base::BindOnce(&Broker::GetWritableSharedMemoryRegion,
                                      base::Unretained(&broker), num_bytes));
  }

  // Blocks until the BrokerHost sends a message or closes the connection.
  ReadResult ReadFromHost(const base::ScopedFD& client) {
    return RunBlocking(base::BindOnce(
        [](int fd) {
          ReadResult result;
          result.num_bytes =
              SocketRecvmsg(fd, &result.response, sizeof(result.response),
                            &result.fds, /*block=*/true);
          return result;
        },
        client.get()));
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

// End-to-end allocation through Broker and BrokerHost.

TEST_F(BrokerTest, CreatesWritableRegion) {
  Broker broker{StartBrokerHost()};
  base::WritableSharedMemoryRegion region = AllocateRegion(broker, kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_EQ(kRegionSize, region.GetSize());

  base::WritableSharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  std::ranges::fill(mapping.GetMemoryAsSpan<uint8_t>(), 0x42);

  // Converting the region to read-only requires the read-only descriptor that
  // is sent along with the writable one.
  base::ReadOnlySharedMemoryRegion read_only_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
  ASSERT_TRUE(read_only_region.IsValid());
  base::ReadOnlySharedMemoryMapping read_only_mapping = read_only_region.Map();
  ASSERT_TRUE(read_only_mapping.IsValid());
  EXPECT_TRUE(std::ranges::all_of(read_only_mapping.GetMemoryAsSpan<uint8_t>(),
                                  [](uint8_t byte) { return byte == 0x42; }));
}

TEST_F(BrokerTest, AllocationFailureKeepsConnectionOpen) {
  Broker broker{StartBrokerHost()};

  // The BrokerHost cannot create an empty region. It reports the failure
  // without closing the connection.
  EXPECT_FALSE(AllocateRegion(broker, 0).IsValid());
#if defined(ARCH_CPU_64_BITS)
  // Sizes that don't fit in a request fail cleanly.
  EXPECT_FALSE(AllocateRegion(broker, size_t{UINT32_MAX} + 1).IsValid());
#endif
  EXPECT_TRUE(AllocateRegion(broker, kRegionSize).IsValid());
}

// BrokerHost hardening against an untrusted client.

TEST_F(BrokerTest, HandlesRequestSplitAcrossWrites) {
  base::ScopedFD client = StartBrokerHost();

  const BrokerBufferRequest request = {.num_bytes = kRegionSize};
  const auto [first, second] = base::byte_span_from_ref(request).split_at(1u);
  ASSERT_EQ(static_cast<ssize_t>(first.size()),
            SocketWrite(client.get(), first.data(), first.size()));
  // The write above made the BrokerHost's end readable before returning, and
  // the IO loop handles ready descriptors before going idle, so this lets the
  // BrokerHost read the first part on its own.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(static_cast<ssize_t>(second.size()),
            SocketWrite(client.get(), second.data(), second.size()));

  ReadResult result = ReadFromHost(client);
  ASSERT_EQ(static_cast<ssize_t>(sizeof(BrokerBufferResponse)),
            result.num_bytes);
  EXPECT_EQ(BrokerBufferResponse::kSuccess, result.response);
  EXPECT_EQ(2u, result.fds.size());
}

TEST_F(BrokerTest, DiscardsDescriptorsSentByClient) {
  base::ScopedFD client = StartBrokerHost();
  base::ScopedFD pipe_read_end;
  base::ScopedFD pipe_write_end;
  ASSERT_TRUE(base::CreatePipe(&pipe_read_end, &pipe_write_end,
                               /*non_blocking=*/true));

  // Send a valid request with the pipe's only write end attached.
  BrokerBufferRequest request = {.num_bytes = kRegionSize};
  struct iovec iov = {&request, sizeof(request)};
  std::vector<base::ScopedFD> fds;
  fds.push_back(std::move(pipe_write_end));
  ASSERT_EQ(static_cast<ssize_t>(sizeof(request)),
            SendmsgWithHandles(client.get(), &iov, 1, fds));
  fds.clear();

  // The BrokerHost handles the request, and the kernel closes the attached
  // write end, so reading from the pipe returns EOF.
  ReadResult result = ReadFromHost(client);
  ASSERT_EQ(static_cast<ssize_t>(sizeof(BrokerBufferResponse)),
            result.num_bytes);
  EXPECT_EQ(BrokerBufferResponse::kSuccess, result.response);
  EXPECT_EQ(2u, result.fds.size());
  char byte;
  EXPECT_EQ(0, HANDLE_EINTR(read(pipe_read_end.get(), &byte, 1)));
}

TEST_F(BrokerTest, ClosesConnectionWhenClientStopsReading) {
  // Check both kinds of response: kFailure, which carries no descriptors, and
  // kSuccess, which carries two.
  for (const uint32_t num_bytes : {uint32_t{0}, uint32_t{kRegionSize}}) {
    SCOPED_TRACE(num_bytes);
    base::ScopedFD client = StartBrokerHost();

    // Send requests without reading the responses until they fill the socket's
    // buffer. The BrokerHost must then close the connection rather than block.
    const int error = RunBlocking(base::BindOnce(
        [](int fd, BrokerBufferRequest request) {
          while (SocketWrite(fd, &request, sizeof(request)) ==
                 static_cast<ssize_t>(sizeof(request))) {
          }
          return errno;
        },
        client.get(), BrokerBufferRequest{.num_bytes = num_bytes}));
    EXPECT_TRUE(error == EPIPE || error == ECONNRESET) << error;
  }
}

// Broker behavior, with the test acting as the BrokerHost.

TEST_F(BrokerTest, BrokerStopsUsingConnectionAfterProtocolError) {
  PlatformChannel channel;
  base::ScopedFD host =
      channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD();
  Broker broker{channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD()};

  // Queue an invalid reply to the first request: kSuccess without descriptors.
  const BrokerBufferResponse response = BrokerBufferResponse::kSuccess;
  ASSERT_EQ(static_cast<ssize_t>(sizeof(response)),
            SocketWrite(host.get(), &response, sizeof(response)));
  EXPECT_FALSE(broker.GetWritableSharedMemoryRegion(kRegionSize).IsValid());

  // The connection may be out of sync, so later requests fail without being
  // sent: the host sees only the first request, then end-of-stream.
  EXPECT_FALSE(broker.GetWritableSharedMemoryRegion(kRegionSize).IsValid());
  BrokerBufferRequest request;
  EXPECT_EQ(static_cast<ssize_t>(sizeof(request)),
            HANDLE_EINTR(read(host.get(), &request, sizeof(request))));
  EXPECT_EQ(0, HANDLE_EINTR(read(host.get(), &request, sizeof(request))));
}

}  // namespace
}  // namespace mojo::core
