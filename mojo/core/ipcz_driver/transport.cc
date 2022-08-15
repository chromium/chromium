// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/transport.h"

#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/core/core.h"
#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace mojo::core::ipcz_driver {

Transport::Transport(Destination destination,
                     PlatformChannelEndpoint endpoint,
                     base::Process remote_process)
    : destination_(destination),
      remote_process_(std::move(remote_process)),
      inactive_endpoint_(std::move(endpoint)) {}

// static
std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>>
Transport::CreatePair(Destination first_destination,
                      Destination second_destination) {
  PlatformChannel channel;
  auto one = base::MakeRefCounted<Transport>(first_destination,
                                             channel.TakeLocalEndpoint());
  auto two = base::MakeRefCounted<Transport>(second_destination,
                                             channel.TakeRemoteEndpoint());
  return {one, two};
}

Transport::~Transport() = default;

bool Transport::Activate(IpczHandle transport,
                         IpczTransportActivityHandler activity_handler) {
  scoped_refptr<Channel> channel;
  std::vector<PendingTransmission> pending_transmissions;
  {
    base::AutoLock lock(lock_);
    if (channel_ || !inactive_endpoint_.is_valid()) {
      return false;
    }

    ipcz_transport_ = transport;
    activity_handler_ = activity_handler;
    self_reference_for_channel_ = base::WrapRefCounted(this);
    channel_ = Channel::CreateForIpczDriver(
        this, std::move(inactive_endpoint_),
        Core::Get()->GetNodeController()->io_task_runner());
    channel_->Start();

    if (!pending_transmissions_.empty()) {
      pending_transmissions_.swap(pending_transmissions);
      channel = channel_;
    }
  }

  for (auto& transmission : pending_transmissions) {
    channel->Write(Channel::Message::CreateIpczMessage(
        base::make_span(transmission.bytes), std::move(transmission.handles)));
  }

  return true;
}

bool Transport::Deactivate() {
  scoped_refptr<Channel> channel;
  {
    base::AutoLock lock(lock_);
    if (!channel_) {
      return false;
    }

    channel = std::move(channel_);
  }

  // This will post a task to the Channel's IO thread to complete shutdown. Once
  // the last Channel reference is dropped, it will invoke OnChannelDestroyed()
  // on this Transport. The Transport is kept alive in the meantime by its own
  // retained `self_reference_for_channel_`.
  channel->ShutDown();
  return true;
}

bool Transport::Transmit(base::span<const uint8_t> data,
                         base::span<const IpczDriverHandle> handles) {
#if BUILDFLAG(IS_WIN)
  // All Windows handles must be inlined as message data as part of object
  // serialization, so the driver should never attempt to transmit handles
  // out-of-band there.
  DCHECK(handles.empty());
#endif

  std::vector<PlatformHandle> platform_handles;
  platform_handles.reserve(handles.size());
  for (IpczDriverHandle handle : handles) {
    auto transmissible_handle =
        TransmissiblePlatformHandle::TakeFromHandle(handle);
    DCHECK(transmissible_handle);
    platform_handles.push_back(transmissible_handle->TakeHandle());
  }

  scoped_refptr<Channel> channel;
  {
    base::AutoLock lock(lock_);
    if (inactive_endpoint_.is_valid()) {
      PendingTransmission transmission;
      transmission.bytes = std::vector<uint8_t>(data.begin(), data.end());
      transmission.handles = std::move(platform_handles);
      pending_transmissions_.push_back(std::move(transmission));
      return true;
    }

    if (!channel_) {
      return false;
    }
    channel = channel_;
  }

  channel->Write(
      Channel::Message::CreateIpczMessage(data, std::move(platform_handles)));
  return true;
}

void Transport::Close() {
  Deactivate();
}

bool Transport::IsIpczTransport() const {
  return true;
}

void Transport::OnChannelMessage(const void* payload,
                                 size_t payload_size,
                                 std::vector<PlatformHandle> handles) {
  std::vector<IpczDriverHandle> driver_handles(handles.size());
  for (size_t i = 0; i < handles.size(); ++i) {
    driver_handles[i] = TransmissiblePlatformHandle::ReleaseAsHandle(
        base::MakeRefCounted<TransmissiblePlatformHandle>(
            std::move(handles[i])));
  }

  const IpczResult result = activity_handler_(
      ipcz_transport_, static_cast<const uint8_t*>(payload), payload_size,
      driver_handles.data(), driver_handles.size(), IPCZ_NO_FLAGS, nullptr);
  if (result != IPCZ_RESULT_OK && result != IPCZ_RESULT_UNIMPLEMENTED) {
    OnChannelError(Channel::Error::kReceivedMalformedData);
  }
}

void Transport::OnChannelError(Channel::Error error) {
  activity_handler_(ipcz_transport_, nullptr, 0, nullptr, 0,
                    IPCZ_TRANSPORT_ACTIVITY_ERROR, nullptr);
}

void Transport::OnChannelDestroyed() {
  activity_handler_(ipcz_transport_, nullptr, 0, nullptr, 0,
                    IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr);

  // Drop our self-reference now that the Channel is definitely done calling us.
  // May delete `this` as the stack unwinds.
  scoped_refptr<Transport> self;
  base::AutoLock lock(lock_);
  self = std::move(self_reference_for_channel_);
}

Transport::PendingTransmission::PendingTransmission() = default;

Transport::PendingTransmission::PendingTransmission(PendingTransmission&&) =
    default;

Transport::PendingTransmission& Transport::PendingTransmission::operator=(
    PendingTransmission&&) = default;

Transport::PendingTransmission::~PendingTransmission() = default;

}  // namespace mojo::core::ipcz_driver
