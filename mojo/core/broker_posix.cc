// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "mojo/core/broker_messages.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace mojo::core {

Broker::Broker(base::ScopedFD socket) : socket_(std::move(socket)) {
  CHECK(socket_.is_valid());

  // Mark the socket as blocking.
  int flags = fcntl(socket_.get(), F_GETFL);
  PCHECK(flags != -1);
  flags = fcntl(socket_.get(), F_SETFL, flags & ~O_NONBLOCK);
  PCHECK(flags != -1);
}

Broker::~Broker() = default;

base::WritableSharedMemoryRegion Broker::GetWritableSharedMemoryRegion(
    size_t num_bytes) {
  // The request carries a 32-bit size. Base never creates regions larger than
  // INT_MAX bytes anyway, so there is no point in asking for one.
  if (!base::IsValueInRangeForNumericType<uint32_t>(num_bytes)) {
    return {};
  }

  base::AutoLock lock(lock_);
  if (!socket_.is_valid()) {
    return {};
  }

  // Move `socket_` out so that any early return on an I/O or protocol error
  // closes the connection permanently rather than leaving it out of sync.
  base::ScopedFD socket = std::move(socket_);

  const BrokerBufferRequest request = {
      .num_bytes = base::checked_cast<uint32_t>(num_bytes),
  };
  const ssize_t write_result =
      SocketWrite(socket.get(), &request, sizeof(request));
  if (write_result < 0) {
    PLOG(ERROR) << "Error sending sync broker message";
    return {};
  }
  if (static_cast<size_t>(write_result) != sizeof(request)) {
    LOG(ERROR) << "Error sending complete broker message";
    return {};
  }

  // A 1-byte response can't arrive in pieces, so unlike requests in
  // BrokerHost, it needs no reassembly.
  static_assert(sizeof(BrokerBufferResponse) == 1);
  BrokerBufferResponse response = BrokerBufferResponse::kFailure;
  std::vector<base::ScopedFD> fds;
  const ssize_t read_result = SocketRecvmsg(
      socket.get(), &response, sizeof(response), &fds, /*block=*/true);
  if (read_result < 0) {
    PLOG(ERROR) << "Recvmsg error";
    return {};
  }
  if (static_cast<size_t>(read_result) != sizeof(response)) {
    LOG(ERROR) << "Connection closed or response descriptors dropped";
    return {};
  }

  if (response == BrokerBufferResponse::kFailure && fds.empty()) {
    socket_ = std::move(socket);
    return {};
  }
  if (response != BrokerBufferResponse::kSuccess || fds.size() != 2) {
    LOG(ERROR) << "Invalid broker response";
    return {};
  }

  socket_ = std::move(socket);
  // A region's GUID only identifies it, e.g. in memory dumps, so the BrokerHost
  // doesn't send one. Mint it here, as for regions created in this process.
  return base::WritableSharedMemoryRegion::Deserialize(
      base::subtle::PlatformSharedMemoryRegion::Take(
          base::subtle::ScopedFDPair(std::move(fds[0]), std::move(fds[1])),
          base::subtle::PlatformSharedMemoryRegion::Mode::kWritable, num_bytes,
          base::UnguessableToken::Create()));
}

}  // namespace mojo::core
