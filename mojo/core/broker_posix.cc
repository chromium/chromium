// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker.h"

#include <fcntl.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "build/build_config.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/channel.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace mojo {
namespace core {

namespace {

Channel::MessagePtr WaitForBrokerMessage(
    int socket_fd,
    BrokerMessageType expected_type,
    size_t expected_num_handles,
    size_t expected_data_size,
    std::vector<PlatformHandle>* incoming_handles) {
  Channel::MessagePtr message(new Channel::Message(
      sizeof(BrokerMessageHeader) + expected_data_size, expected_num_handles));
  std::vector<base::ScopedFD> incoming_fds;
  ssize_t read_result =
      SocketRecvmsg(socket_fd, const_cast<void*>(message->data()),
                    message->data_num_bytes(), &incoming_fds, true /* block */);
  bool error = false;
  if (read_result < 0) {
    PLOG(ERROR) << "Recvmsg error";
    error = true;
  } else if (static_cast<size_t>(read_result) != message->data_num_bytes()) {
    LOG(ERROR) << "Invalid node channel message";
    error = true;
  } else if (incoming_fds.size() != expected_num_handles) {
    LOG(ERROR) << "Received unexpected number of handles";
    error = true;
  }

  if (error)
    return nullptr;

  const BrokerMessageHeader* header =
      reinterpret_cast<const BrokerMessageHeader*>(message->payload());
  if (header->type != expected_type) {
    LOG(ERROR) << "Unexpected message";
    return nullptr;
  }

  incoming_handles->reserve(incoming_fds.size());
  for (size_t i = 0; i < incoming_fds.size(); ++i)
    incoming_handles->emplace_back(std::move(incoming_fds[i]));

  return message;
}

}  // namespace

Broker::Broker(PlatformHandle handle, bool wait_for_channel_handle)
    : sync_channel_(std::move(handle)) {
  CHECK(sync_channel_.is_valid());

  int fd = sync_channel_.GetFD().get();
  // Mark the channel as blocking.
  int flags = fcntl(fd, F_GETFL);
  PCHECK(flags != -1);
  flags = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
  PCHECK(flags != -1);

  if (!wait_for_channel_handle)
    return;

  // Wait for the first message, which should contain a handle.
  std::vector<PlatformHandle> incoming_platform_handles;
  if (WaitForBrokerMessage(fd, BrokerMessageType::INIT, 1, 0,
                           &incoming_platform_handles)) {
    inviter_endpoint_ =
        PlatformChannelEndpoint(std::move(incoming_platform_handles[0]));
  }
}

Broker::~Broker() = default;

PlatformChannelEndpoint Broker::GetInviterEndpoint() {
  return std::move(inviter_endpoint_);
}

base::WritableSharedMemoryRegion Broker::GetWritableSharedMemoryRegion(
    size_t num_bytes) {
  base::AutoLock lock(lock_);

  BufferRequestData* buffer_request;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_REQUEST, 0, 0, &buffer_request);
  buffer_request->size = num_bytes;
  ssize_t write_result =
      SocketWrite(sync_channel_.GetFD().get(), out_message->data(),
                  out_message->data_num_bytes());
  if (write_result < 0) {
    PLOG(ERROR) << "Error sending sync broker message";
    return base::WritableSharedMemoryRegion();
  } else if (static_cast<size_t>(write_result) !=
             out_message->data_num_bytes()) {
    LOG(ERROR) << "Error sending complete broker message";
    return base::WritableSharedMemoryRegion();
  }

#if !defined(OS_POSIX) || defined(OS_ANDROID) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
  // Non-POSIX systems, as well as Android, and non-iOS Mac, only use a single
  // handle to represent a writable region.
  constexpr size_t kNumExpectedHandles = 1;
#else
  constexpr size_t kNumExpectedHandles = 2;
#endif

  std::vector<PlatformHandle> handles;
  Channel::MessagePtr message = WaitForBrokerMessage(
      sync_channel_.GetFD().get(), BrokerMessageType::BUFFER_RESPONSE,
      kNumExpectedHandles, sizeof(BufferResponseData), &handles);
  if (message) {
    const BufferResponseData* data;
    if (!GetBrokerMessageData(message.get(), &data))
      return base::WritableSharedMemoryRegion();

    if (handles.size() == 1)
      handles.emplace_back();
    return base::WritableSharedMemoryRegion::Deserialize(
        base::subtle::PlatformSharedMemoryRegion::Take(
            CreateSharedMemoryRegionHandleFromPlatformHandles(
                std::move(handles[0]), std::move(handles[1])),
            base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
            num_bytes,
            base::UnguessableToken::Deserialize(data->guid_high,
                                                data->guid_low)));
  }

  return base::WritableSharedMemoryRegion();
}

}  // namespace core
}  // namespace mojo
