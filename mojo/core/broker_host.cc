// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker_host.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace mojo::core {

BrokerHost::BrokerHost(base::ScopedFD socket) : socket_(std::move(socket)) {
  CHECK(socket_.is_valid());
  base::CurrentThread::Get()->AddDestructionObserver(this);
  read_watch_ = base::IOWatcher::Get()->WatchFileDescriptor(
      socket_.get(), base::IOWatcher::FdWatchDuration::kPersistent,
      base::IOWatcher::FdWatchMode::kRead, *this);
}

BrokerHost::~BrokerHost() {
  // We're always destroyed on the creation thread, which is the IO thread.
  base::CurrentThread::Get()->RemoveDestructionObserver(this);
}

void BrokerHost::OnFdReadable(int fd) {
  CHECK_EQ(fd, socket_.get());

  // The socket is a byte stream, so a request may arrive in pieces. Read at
  // most the rest of the current request: a well-behaved client waits for each
  // response before sending another request, and the watch is level-triggered,
  // so any further data is read in a later call.
  base::span<uint8_t> unread =
      base::byte_span_from_ref(request_).subspan(request_bytes_received_);
  // recv() has no buffer for ancillary data, so the kernel discards any
  // descriptors sent by the client instead of passing them to this process.
  const ssize_t result = HANDLE_EINTR(
      recv(socket_.get(), unread.data(), unread.size(), MSG_DONTWAIT));
  if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    return;
  }

  // Close the connection on error or end-of-stream.
  if (result <= 0) {
    delete this;
    return;
  }

  request_bytes_received_ += static_cast<size_t>(result);
  if (request_bytes_received_ < sizeof(request_)) {
    return;
  }
  request_bytes_received_ = 0;
  if (!OnBufferRequest(request_.num_bytes)) {
    delete this;
  }
}

void BrokerHost::OnFdWritable(int fd) {
  // Only `socket_`'s readability is watched.
  NOTREACHED();
}

void BrokerHost::WillDestroyCurrentMessageLoop() {
  delete this;
}

bool BrokerHost::OnBufferRequest(uint32_t num_bytes) {
  base::subtle::PlatformSharedMemoryRegion region =
      base::subtle::PlatformSharedMemoryRegion::CreateWritable(num_bytes);

  // The client reads each response before sending its next request, so there
  // is always enough room in the socket for the 1-byte response. A failed
  // write, including one which would have blocked, means that the client is
  // gone or misbehaving.
  ssize_t result;
  if (region.IsValid()) {
    base::subtle::ScopedFDPair fd_pair = region.PassPlatformHandle();
    CHECK(fd_pair.readonly_fd.is_valid());
    std::vector<base::ScopedFD> fds;
    fds.push_back(std::move(fd_pair.fd));
    fds.push_back(std::move(fd_pair.readonly_fd));
    BrokerBufferResponse response = BrokerBufferResponse::kSuccess;
    struct iovec iov = {&response, sizeof(response)};
    result = SendmsgWithHandles(socket_.get(), &iov, 1, fds, MSG_DONTWAIT);
  } else {
    const BrokerBufferResponse response = BrokerBufferResponse::kFailure;
    result =
        SocketWrite(socket_.get(), &response, sizeof(response), MSG_DONTWAIT);
  }
  return result == static_cast<ssize_t>(sizeof(BrokerBufferResponse));
}

}  // namespace mojo::core
