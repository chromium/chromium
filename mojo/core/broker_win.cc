// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <limits>
#include <utility>

#include "base/debug/alias.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "mojo/core/broker.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/channel.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace mojo {
namespace core {

namespace {

// 256 bytes should be enough for anyone!
const size_t kMaxBrokerMessageSize = 256;

bool TakeHandlesFromBrokerMessage(Channel::Message* message,
                                  size_t num_handles,
                                  PlatformHandle* out_handles) {
  if (message->num_handles() != num_handles) {
    DLOG(ERROR) << "Received unexpected number of handles in broker message";
    return false;
  }

  std::vector<PlatformHandleInTransit> handles = message->TakeHandles();
  DCHECK_EQ(handles.size(), num_handles);
  DCHECK(out_handles);

  for (size_t i = 0; i < num_handles; ++i)
    out_handles[i] = handles[i].TakeHandle();
  return true;
}

Channel::MessagePtr WaitForBrokerMessage(HANDLE pipe_handle,
                                         BrokerMessageType expected_type) {
  char buffer[kMaxBrokerMessageSize];
  DWORD bytes_read = 0;
  BOOL result = ::ReadFile(pipe_handle, buffer, kMaxBrokerMessageSize,
                           &bytes_read, nullptr);
  if (!result) {
    // The pipe may be broken if the browser side has been closed, e.g. during
    // browser shutdown. In that case the ReadFile call will fail and we
    // shouldn't continue waiting.
    PLOG(ERROR) << "Error reading broker pipe";
    return nullptr;
  }

  Channel::MessagePtr message =
      Channel::Message::Deserialize(buffer, static_cast<size_t>(bytes_read));
  if (!message || message->payload_size() < sizeof(BrokerMessageHeader)) {
    LOG(ERROR) << "Invalid broker message";

    base::debug::Alias(&buffer[0]);
    base::debug::Alias(&bytes_read);
    CHECK(false);
    return nullptr;
  }

  const BrokerMessageHeader* header =
      reinterpret_cast<const BrokerMessageHeader*>(message->payload());
  if (header->type != expected_type) {
    LOG(ERROR) << "Unexpected broker message type";

    base::debug::Alias(&buffer[0]);
    base::debug::Alias(&bytes_read);
    CHECK(false);
    return nullptr;
  }

  return message;
}

}  // namespace

Broker::Broker(PlatformHandle handle, bool wait_for_channel_handle)
    : sync_channel_(std::move(handle)) {
  CHECK(sync_channel_.is_valid());
  if (!wait_for_channel_handle)
    return;

  Channel::MessagePtr message = WaitForBrokerMessage(
      sync_channel_.GetHandle().Get(), BrokerMessageType::INIT);

  // If we fail to read a message (broken pipe), just return early. The inviter
  // handle will be null and callers must handle this gracefully.
  if (!message)
    return;

  PlatformHandle endpoint_handle;
  if (TakeHandlesFromBrokerMessage(message.get(), 1, &endpoint_handle)) {
    inviter_endpoint_ = PlatformChannelEndpoint(std::move(endpoint_handle));
  } else {
    // If the message has no handles, we expect it to carry pipe name instead.
    const BrokerMessageHeader* header =
        static_cast<const BrokerMessageHeader*>(message->payload());
    CHECK_GE(message->payload_size(),
             sizeof(BrokerMessageHeader) + sizeof(InitData));
    const InitData* data = reinterpret_cast<const InitData*>(header + 1);
    CHECK_EQ(message->payload_size(),
             sizeof(BrokerMessageHeader) + sizeof(InitData) +
                 data->pipe_name_length * sizeof(base::char16));
    const base::char16* name_data =
        reinterpret_cast<const base::char16*>(data + 1);
    CHECK(data->pipe_name_length);
    inviter_endpoint_ = NamedPlatformChannel::ConnectToServer(
        base::StringPiece16(name_data, data->pipe_name_length).as_string());
  }
}

Broker::~Broker() {}

PlatformChannelEndpoint Broker::GetInviterEndpoint() {
  return std::move(inviter_endpoint_);
}

base::WritableSharedMemoryRegion Broker::GetWritableSharedMemoryRegion(
    size_t num_bytes) {
  base::AutoLock lock(lock_);
  BufferRequestData* buffer_request;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_REQUEST, 0, 0, &buffer_request);
  buffer_request->size = base::checked_cast<uint32_t>(num_bytes);
  DWORD bytes_written = 0;
  BOOL result =
      ::WriteFile(sync_channel_.GetHandle().Get(), out_message->data(),
                  static_cast<DWORD>(out_message->data_num_bytes()),
                  &bytes_written, nullptr);
  if (!result ||
      static_cast<size_t>(bytes_written) != out_message->data_num_bytes()) {
    PLOG(ERROR) << "Error sending sync broker message";
    return base::WritableSharedMemoryRegion();
  }

  PlatformHandle handle;
  Channel::MessagePtr response = WaitForBrokerMessage(
      sync_channel_.GetHandle().Get(), BrokerMessageType::BUFFER_RESPONSE);
  if (response && TakeHandlesFromBrokerMessage(response.get(), 1, &handle)) {
    BufferResponseData* data;
    if (!GetBrokerMessageData(response.get(), &data))
      return base::WritableSharedMemoryRegion();
    return base::WritableSharedMemoryRegion::Deserialize(
        base::subtle::PlatformSharedMemoryRegion::Take(
            CreateSharedMemoryRegionHandleFromPlatformHandles(std::move(handle),
                                                              PlatformHandle()),
            base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
            num_bytes,
            base::UnguessableToken::Deserialize(data->guid_high,
                                                data->guid_low)));
  }

  return base::WritableSharedMemoryRegion();
}

}  // namespace core
}  // namespace mojo
