// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker_host.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/ipcz_driver/envelope.h"
#include "mojo/core/platform_handle_in_transit.h"

namespace mojo {
namespace core {

BrokerHost::BrokerHost(PlatformChannelEndpoint endpoint) {
  base::CurrentThread::Get()->AddDestructionObserver(this);
  CHECK(endpoint.is_valid());
  channel_ = Channel::Create(this, ConnectionParams(std::move(endpoint)),
                             Channel::HandlePolicy::kRejectHandles,
                             base::SingleThreadTaskRunner::GetCurrentDefault());
  channel_->Start();
}

BrokerHost::~BrokerHost() {
  // We're always destroyed on the creation thread, which is the IO thread.
  base::CurrentThread::Get()->RemoveDestructionObserver(this);

  if (channel_) {
    channel_->ShutDown();
  }
}

void BrokerHost::OnBufferRequest(uint32_t num_bytes) {
  base::subtle::PlatformSharedMemoryRegion region =
      base::subtle::PlatformSharedMemoryRegion::CreateWritable(num_bytes);

  std::vector<PlatformHandleInTransit> handles;
  handles.reserve(2);
  if (region.IsValid()) {
    base::subtle::ScopedFDPair fds = region.PassPlatformHandle();
    DCHECK(fds.readonly_fd.is_valid());
    handles.emplace_back(PlatformHandle(std::move(fds.fd)));
    handles.emplace_back(PlatformHandle(std::move(fds.readonly_fd)));
  }

  BufferResponseData* response;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_RESPONSE, handles.size(), &response);
  if (handles.empty()) {
    response->guid_high = 0;
    response->guid_low = 0;
  } else {
    const base::UnguessableToken& guid = region.GetGUID();
    response->guid_high = guid.GetHighForSerialization();
    response->guid_low = guid.GetLowForSerialization();
    message->SetHandles(std::move(handles));
  }

  channel_->Write(std::move(message));
}

void BrokerHost::OnChannelMessage(
    const void* payload,
    size_t payload_size,
    std::vector<PlatformHandle> handles,
    scoped_refptr<ipcz_driver::Envelope> envelope) {
  if (payload_size < sizeof(BrokerMessageHeader)) {
    return;
  }

  const BrokerMessageHeader* header =
      static_cast<const BrokerMessageHeader*>(payload);
  switch (header->type) {
    case BrokerMessageType::BUFFER_REQUEST:
      if (payload_size ==
          sizeof(BrokerMessageHeader) + sizeof(BufferRequestData)) {
        const BufferRequestData* request =
            reinterpret_cast<const BufferRequestData*>(UNSAFE_TODO(header + 1));
        OnBufferRequest(request->size);
      }
      break;

    default:
      DLOG(ERROR) << "Unexpected broker message type: " << header->type;
      break;
  }
}

void BrokerHost::OnChannelError(Channel::Error error) {
  delete this;
}

void BrokerHost::WillDestroyCurrentMessageLoop() {
  delete this;
}

}  // namespace core
}  // namespace mojo
