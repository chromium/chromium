// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/invitation.h"

#include <algorithm>

#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo::core::ipcz_driver {

namespace {

// The Mojo attach/extract APIs originally took arbitrary string values to
// identify pipe attachments, and there are still application using that
// interface. ipcz on the other hand only allows the application to specify a
// number of initial portals to open during ConnectNode().
//
// Fortunately all Mojo consumers across Chrome and Chrome OS fit into one of
// two categories today:
//
//  (a) using an arbitrary string value (usually a GUID) for the attachment
//      name, but attaching only one pipe.
//
//  (b) attaching multiple pipes, but using 32-bit or 64-bit `name` values that
//      are sequential, zero-based, little-endian integers.
//
// We take the first 4 bytes of any name and interpret it as an index into an
// array of initial portals. If the index is above a reasonably small upper
// bound (8) then it's treated as zero.
size_t GetAttachmentIndex(base::span<const uint8_t> name) {
  if (name.size() != sizeof(uint32_t) && name.size() != sizeof(uint64_t)) {
    // Use index 0 if the invitation name does not match a simple integer size.
    // This is assumed to be case (a) above, where this will be the only
    // attachment.
    return 0;
  }

  // Otherwise interpret the first 4 bytes as an integer.
  uint32_t index = *reinterpret_cast<const uint32_t*>(name.data());
  if (index < Invitation::kMaxAttachments) {
    // The resulting index is small enough to fit within the normal index range,
    // so assume case (b) above:
    return index;
  }

  // With the index out of range, assume the the integer sizing is a
  // coincidence and treat this as case (a), where this should be the only
  // attachment.
  return 0;
}

IpczDriverHandle CreateTransportForMojoEndpoint(
    Transport::Destination destination,
    const MojoInvitationTransportEndpoint& endpoint,
    base::Process remote_process = base::Process()) {
  CHECK_EQ(endpoint.num_platform_handles, 1u);
  auto handle =
      PlatformHandle::FromMojoPlatformHandle(&endpoint.platform_handles[0]);
  if (!handle.is_valid()) {
    return IPCZ_INVALID_DRIVER_HANDLE;
  }

  Channel::Endpoint channel_endpoint;
  if (endpoint.type == MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER) {
    channel_endpoint = PlatformChannelServerEndpoint(std::move(handle));
  } else {
    channel_endpoint = PlatformChannelEndpoint(std::move(handle));
  }
  auto transport = base::MakeRefCounted<Transport>(
      destination, std::move(channel_endpoint), std::move(remote_process));
  return ObjectBase::ReleaseAsHandle(std::move(transport));
}

}  // namespace

Invitation::Invitation() = default;

Invitation::~Invitation() {
  Close();
}

MojoResult Invitation::Attach(base::span<const uint8_t> name,
                              MojoHandle* handle) {
  const size_t index = GetAttachmentIndex(name);
  if (attachments_[index] != IPCZ_INVALID_HANDLE) {
    return MOJO_RESULT_ALREADY_EXISTS;
  }

  // One portal is returned for immediate use; the other is retained so that we
  // can merge it with a portal returned by ConnectNode() in Send() below.
  IpczResult result = GetIpczAPI().OpenPortals(
      GetIpczNode(), IPCZ_NO_FLAGS, nullptr, &attachments_[index], handle);
  CHECK_EQ(result, IPCZ_RESULT_OK);

  max_attachment_index_ = std::max(max_attachment_index_, index);
  ++num_attachments_;
  return MOJO_RESULT_OK;
}

MojoResult Invitation::Extract(base::span<const uint8_t> name,
                               MojoHandle* handle) {
  // We expect attachments to have been populated by Accept() already.
  const size_t index = GetAttachmentIndex(name);
  if (attachments_[index] == IPCZ_INVALID_HANDLE) {
    return MOJO_RESULT_NOT_FOUND;
  }

  *handle = attachments_[index];
  attachments_[index] = IPCZ_INVALID_HANDLE;
  return MOJO_RESULT_OK;
}

MojoResult Invitation::Send(
    const MojoPlatformProcessHandle* process_handle,
    const MojoInvitationTransportEndpoint* transport_endpoint,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    const MojoSendInvitationOptions* options) {
  if (!transport_endpoint ||
      transport_endpoint->struct_size < sizeof(*transport_endpoint) ||
      transport_endpoint->num_platform_handles == 0 ||
      !transport_endpoint->platform_handles) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  if (options && options->struct_size < sizeof(*options)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  base::Process remote_process;
  if (process_handle) {
    if (UnwrapAndClonePlatformProcessHandle(process_handle, remote_process) !=
        MOJO_RESULT_OK) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
  }

  // TODO: Support process error handler hooks and NotifyBadMessage.
  // TODO: Support isolated connections.
  const bool is_isolated =
      options && (options->flags & MOJO_SEND_INVITATION_FLAG_ISOLATED) != 0;
  CHECK(!is_isolated);

  const IpczNodeOptions& config = GetIpczNodeOptions();
  IpczConnectNodeFlags flags = 0;
  if (!config.is_broker) {
    // TODO: Support non-broker to non-broker connection. Requires new flags for
    // MojoSendInvitation and MojoAcceptInvitation, because ipcz requires
    // explicit opt-in from both sides of the connection in order for broker
    // inheritance to be allowed.
    flags |= IPCZ_CONNECT_NODE_TO_BROKER;
    if (!config.use_local_shared_memory_allocation) {
      flags |= IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
    }
  }

  IpczDriverHandle transport = CreateTransportForMojoEndpoint(
      Transport::kToNonBroker, *transport_endpoint, std::move(remote_process));
  if (transport == IPCZ_INVALID_DRIVER_HANDLE) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  if (num_attachments_ == 0 || max_attachment_index_ != num_attachments_ - 1) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  IpczHandle portals[kMaxAttachments];
  IpczResult result = GetIpczAPI().ConnectNode(
      GetIpczNode(), transport, num_attachments_, flags, nullptr, portals);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  for (size_t i = 0; i < num_attachments_; ++i) {
    result = GetIpczAPI().MergePortals(attachments_[i], portals[i],
                                       IPCZ_NO_FLAGS, nullptr);
    CHECK_EQ(result, IPCZ_RESULT_OK);
    attachments_[i] = IPCZ_INVALID_HANDLE;
  }
  return MOJO_RESULT_OK;
}

// static
MojoHandle Invitation::Accept(
    const MojoInvitationTransportEndpoint* transport_endpoint,
    const MojoAcceptInvitationOptions* options) {
  if (!transport_endpoint ||
      transport_endpoint->struct_size < sizeof(*transport_endpoint) ||
      transport_endpoint->num_platform_handles == 0 ||
      !transport_endpoint->platform_handles) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  auto invitation = base::MakeRefCounted<Invitation>();

  const IpczNodeOptions& config = GetIpczNodeOptions();
  CHECK(!config.is_broker);

  IpczConnectNodeFlags flags = IPCZ_CONNECT_NODE_TO_BROKER;
  if (!config.use_local_shared_memory_allocation) {
    flags |= IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
  }

  // When accepting an invitation, we ConnectNode() with the maximum possible
  // number of initial portals: unlike ipcz, Mojo APIs have no way for this end
  // of a connection to express the expected number of attachments prior to
  // calling MojoAcceptInvitation().
  //
  // As the application extracts attachments, the corresponding initial portals
  // will be extracted from this set. Any unclaimed initial portals (which will
  // not have a peer on the sending node anyway) will be cleaned up when the
  // Invitation itself is destroyed.
  IpczHandle portals[kMaxAttachments];
  IpczDriverHandle transport =
      CreateTransportForMojoEndpoint(Transport::kToBroker, *transport_endpoint);
  if (transport == IPCZ_INVALID_DRIVER_HANDLE) {
    return IPCZ_INVALID_DRIVER_HANDLE;
  }

  IpczResult result = GetIpczAPI().ConnectNode(
      GetIpczNode(), transport, kMaxAttachments, flags, nullptr, portals);
  CHECK_EQ(result, IPCZ_RESULT_OK);

  for (size_t i = 0; i < kMaxAttachments; ++i) {
    IpczHandle bridge;
    GetIpczAPI().OpenPortals(GetIpczNode(), IPCZ_NO_FLAGS, nullptr,
                             &invitation->attachments_[i], &bridge);
    result =
        GetIpczAPI().MergePortals(portals[i], bridge, IPCZ_NO_FLAGS, nullptr);
  }
  invitation->num_attachments_ = kMaxAttachments;
  invitation->max_attachment_index_ = kMaxAttachments - 1;
  return Box(std::move(invitation));
}

void Invitation::Close() {
  // Particularly on accepted invitations, some attachments were created
  // speculatively. If they weren't extracted by the application, close them.
  for (IpczHandle& handle : attachments_) {
    if (handle != IPCZ_INVALID_HANDLE) {
      GetIpczAPI().Close(std::exchange(handle, IPCZ_INVALID_HANDLE),
                         IPCZ_NO_FLAGS, nullptr);
    }
  }
}

}  // namespace mojo::core::ipcz_driver
