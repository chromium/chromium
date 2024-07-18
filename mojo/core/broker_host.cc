// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/broker_host.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/platform_handle_utils.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace core {

BrokerHost::BrokerHost(base::Process client_process,
                       ConnectionParams connection_params,
                       const ProcessErrorCallback& process_error_callback)
    : process_error_callback_(process_error_callback)
#if BUILDFLAG(IS_WIN)
      ,
      client_process_(std::move(client_process))
#endif
{
  base::CurrentThread::Get()->AddDestructionObserver(this);
  CHECK(connection_params.endpoint().is_valid());
  channel_ = Channel::Create(this, std::move(connection_params),
#if BUILDFLAG(IS_WIN)
                             client_process_
#else
                             client_process
#endif
                                     .IsValid()
                                 ? Channel::HandlePolicy::kAcceptHandles
                                 : Channel::HandlePolicy::kRejectHandles,
                             base::SingleThreadTaskRunner::GetCurrentDefault());
  channel_->Start();
}

BrokerHost::~BrokerHost() {
  // We're always destroyed on the creation thread, which is the IO thread.
  base::CurrentThread::Get()->RemoveDestructionObserver(this);

  if (channel_)
    channel_->ShutDown();
}

bool BrokerHost::PrepareHandlesForClient(
    std::vector<PlatformHandleInTransit>* handles) {
#if BUILDFLAG(IS_WIN)
  if (!client_process_.IsValid())
    return false;
  bool handles_ok = true;
  for (auto& handle : *handles) {
    if (!handle.TransferToProcess(client_process_.Duplicate()))
      handles_ok = false;
  }
  return handles_ok;
#else
  return true;
#endif
}

bool BrokerHost::SendChannel(PlatformHandle handle) {
  CHECK(handle.is_valid());
  CHECK(channel_);

#if BUILDFLAG(IS_WIN)
  InitData* data;
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, 0, &data);
  data->pipe_name_length = 0;
#else
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, nullptr);
#endif
  std::vector<PlatformHandleInTransit> handles(1);
  handles[0] = PlatformHandleInTransit(std::move(handle));

  // This may legitimately fail on Windows if the client process is in another
  // session, e.g., is an elevated process.
  if (!PrepareHandlesForClient(&handles))
    return false;

  message->SetHandles(std::move(handles));
  channel_->Write(std::move(message));
  return true;
}

#if BUILDFLAG(IS_WIN)

void BrokerHost::SendNamedChannel(std::wstring_view pipe_name) {
  InitData* data;
  wchar_t* name_data;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::INIT, 0, sizeof(*name_data) * pipe_name.length(),
      &data, reinterpret_cast<void**>(&name_data));
  data->pipe_name_length = static_cast<uint32_t>(pipe_name.length());
  base::ranges::copy(pipe_name, name_data);
  channel_->Write(std::move(message));
}

#endif  // BUILDFLAG(IS_WIN)

void BrokerHost::OnBufferRequest(uint32_t num_bytes) {
  base::subtle::PlatformSharedMemoryRegion region =
      base::subtle::PlatformSharedMemoryRegion::CreateWritable(num_bytes);

  std::vector<PlatformHandleInTransit> handles;
  handles.reserve(2);
  if (region.IsValid()) {
    PlatformHandle h[2];
    ExtractPlatformHandlesFromSharedMemoryRegionHandle(
        region.PassPlatformHandle(), &h[0], &h[1]);
    handles.emplace_back(std::move(h[0]));
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
    // Non-POSIX systems, as well as Android and Mac, only use a single handle
    // to represent a writable region.
    DCHECK(!h[1].is_valid());
#else
    DCHECK(h[1].is_valid());
    handles.emplace_back(std::move(h[1]));
#endif
  }

  BufferResponseData* response;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_RESPONSE, handles.size(), 0, &response);
  if (handles.empty()) {
    response->guid_high = 0;
    response->guid_low = 0;
  } else {
    const base::UnguessableToken& guid = region.GetGUID();
    response->guid_high = guid.GetHighForSerialization();
    response->guid_low = guid.GetLowForSerialization();
    PrepareHandlesForClient(&handles);
    message->SetHandles(std::move(handles));
  }

  channel_->Write(std::move(message));
}

void BrokerHost::OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  std::vector<PlatformHandle> handles) {
  if (payload_size < sizeof(BrokerMessageHeader))
    return;

  const BrokerMessageHeader* header =
      static_cast<const BrokerMessageHeader*>(payload);
  switch (header->type) {
    case BrokerMessageType::BUFFER_REQUEST:
      if (payload_size ==
          sizeof(BrokerMessageHeader) + sizeof(BufferRequestData)) {
        const BufferRequestData* request =
            reinterpret_cast<const BufferRequestData*>(header + 1);
        OnBufferRequest(request->size);
      }
      break;

    default:
      DLOG(ERROR) << "Unexpected broker message type: " << header->type;
      break;
  }
}

void BrokerHost::OnChannelError(Channel::Error error) {
  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Broker host received malformed message");
  }

  delete this;
}

void BrokerHost::WillDestroyCurrentMessageLoop() {
  delete this;
}

}  // namespace core
}  // namespace mojo
