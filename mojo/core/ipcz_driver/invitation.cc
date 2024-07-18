// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/invitation.h"

#include <string.h>

#include <algorithm>
#include <cstdint>

#include "build/build_config.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/base_shared_memory_service.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/process/process_info.h"
#endif

namespace mojo::core::ipcz_driver {

namespace {

MojoDefaultProcessErrorHandler g_default_process_error_handler = nullptr;

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
// bound (7) then it's treated as zero.
size_t GetAttachmentIndex(base::span<const uint8_t> name) {
  if (name.size() != sizeof(uint32_t) && name.size() != sizeof(uint64_t)) {
    // Use index 0 if the invitation name does not match a simple integer size.
    // This is assumed to be case (a) above, where this will be the only
    // attachment.
    return 0;
  }

  // Otherwise interpret the first 4 bytes as an integer.
  uint32_t index;
  memcpy(&index, name.data(), sizeof(uint32_t));
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

struct TransportOptions {
  bool is_peer_trusted = false;
  bool is_trusted_by_peer = false;
  bool leak_channel_on_shutdown = false;
};

IpczDriverHandle CreateTransportForMojoEndpoint(
    Transport::EndpointTypes endpoint_types,
    const MojoInvitationTransportEndpoint& endpoint,
    const TransportOptions& options,
    base::Process remote_process = base::Process(),
    MojoProcessErrorHandler error_handler = nullptr,
    uintptr_t error_handler_context = 0,
    bool is_remote_process_untrusted = false) {
  CHECK_EQ(endpoint.num_platform_handles, 1u);
  auto handle =
      PlatformHandle::FromMojoPlatformHandle(&endpoint.platform_handles[0]);
  if (!handle.is_valid()) {
    return IPCZ_INVALID_DRIVER_HANDLE;
  }

  auto transport = base::MakeRefCounted<Transport>(
      endpoint_types, PlatformChannelEndpoint(std::move(handle)),
      std::move(remote_process), is_remote_process_untrusted);
  transport->SetErrorHandler(error_handler, error_handler_context);
  transport->set_leak_channel_on_shutdown(options.leak_channel_on_shutdown);
  transport->set_is_peer_trusted(options.is_peer_trusted);
  transport->set_is_trusted_by_peer(options.is_trusted_by_peer);
  return ObjectBase::ReleaseAsHandle(std::move(transport));
}

#if BUILDFLAG(IS_WIN)
// Helper function on Windows platform to open the remote server/client process
// given a handle to a connected named pipe. It may return an invalid process
// object if either the handle does not refer to a named pipe or the handle
// refers to a named pipe that is not connected.
base::Process OpenRemoteProcess(const MojoInvitationTransportEndpoint& endpoint,
                                bool remote_is_server) {
  base::ProcessId remote_process_id = 0;
  // Extract the handle to the connected named pipe from mojo invitation
  // transport endpoint.
  HANDLE handle =
      LongToHandle(static_cast<long>(endpoint.platform_handles[0].value));
  auto get_remote_pid = remote_is_server ? &GetNamedPipeServerProcessId
                                         : &GetNamedPipeClientProcessId;
  // Try to get the remote client process id given the extracted handle via
  // GetNamedPipe(Server|Client)ProcessId API.
  if ((!get_remote_pid(handle, &remote_process_id) ||
       remote_process_id == base::Process::Current().Pid())) {
    DVLOG(2) << "Failed to get remote process id via the connected named pipe";
    return base::Process();
  }

  if (remote_process_id == 0) {
    DVLOG(2) << "Remote process id is invalid";
    return base::Process();
  }

  // Try to open the remote process.
  base::Process remote_process =
      base::Process::OpenWithAccess(remote_process_id, PROCESS_DUP_HANDLE);
  if (!remote_process.IsValid()) {
    DVLOG(2) << "Remote process is invalid";
    return base::Process();
  }

  return remote_process;
}
#endif

}  // namespace

Invitation::Invitation() = default;

Invitation::~Invitation() {
  Close();
}

// static
void Invitation::SetDefaultProcessErrorHandler(
    MojoDefaultProcessErrorHandler handler) {
  g_default_process_error_handler = handler;
}

// static
void Invitation::InvokeDefaultProcessErrorHandler(const std::string& error) {
  if (!g_default_process_error_handler) {
    return;
  }

  const MojoProcessErrorDetails details{
      .struct_size = sizeof(details),
      .error_message_length = base::checked_cast<uint32_t>(error.size()),
      .error_message = error.c_str(),
      .flags = MOJO_PROCESS_ERROR_FLAG_NONE,
  };
  g_default_process_error_handler(&details);
}

MojoResult Invitation::Attach(base::span<const uint8_t> name,
                              MojoHandle* handle) {
  const size_t index = GetAttachmentIndex(name);
  if (attachments_[index].is_valid()) {
    return MOJO_RESULT_ALREADY_EXISTS;
  }

  // One portal is returned for immediate use; the other is retained so that we
  // can merge it with a portal returned by ConnectNode() in Send() below.
  IpczHandle attachment;
  IpczResult result = GetIpczAPI().OpenPortals(GetIpczNode(), IPCZ_NO_FLAGS,
                                               nullptr, &attachment, handle);
  CHECK_EQ(result, IPCZ_RESULT_OK);

  attachments_[index] = ScopedIpczHandle(attachment);
  max_attachment_index_ = std::max(max_attachment_index_, index);
  ++num_attachments_;
  return MOJO_RESULT_OK;
}

MojoResult Invitation::Extract(base::span<const uint8_t> name,
                               MojoHandle* handle) {
  // We expect attachments to have been populated by Accept() already.
  const size_t index = GetAttachmentIndex(name);
  if (!attachments_[index].is_valid()) {
    return MOJO_RESULT_NOT_FOUND;
  }

  *handle = attachments_[index].release();
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

  const bool share_broker =
      options && (options->flags & MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);
  const bool is_isolated =
      options && (options->flags & MOJO_SEND_INVITATION_FLAG_ISOLATED) != 0;
  const IpczNodeOptions& config = GetIpczNodeOptions();
  if (share_broker && (is_isolated || config.is_broker)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  IpczConnectNodeFlags flags = 0;
  if (!config.is_broker) {
    if (share_broker) {
      flags |= IPCZ_CONNECT_NODE_SHARE_BROKER;
    } else {
      // If we're a non-broker not sharing our broker, we have to assume the
      // target is itself a broker.
      flags |= IPCZ_CONNECT_NODE_TO_BROKER;
    }
    if (!config.use_local_shared_memory_allocation) {
      flags |= IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
    }
  }

  if (is_isolated) {
    // Nodes using isolated invitations are required by MojoIpcz to both be
    // brokers.
    CHECK(config.is_broker);
    flags |= IPCZ_CONNECT_NODE_TO_BROKER;
  }

  // NOTE: "Untrusted" from Mojo flags here means something different than the
  // notion of trust captured by Transport below. The latter is about general
  // relative trust between two Transport endpoints, while Mojo's "untrusted"
  // bit essentially means that the remote process is especially untrustworthy
  // (e.g. a Chrome renderer) and should be subject to additional constraints
  // regarding what types of objects can be transferred to it.
  const bool is_remote_process_untrusted =
      options &&
      (options->flags & MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS) != 0;

  const bool is_peer_elevated =
      options && (options->flags & MOJO_SEND_INVITATION_FLAG_ELEVATED);
#if !BUILDFLAG(IS_WIN)
  // For now, the concept of an elevated process is only meaningful on Windows.
  DCHECK(!is_peer_elevated);
#endif

#if BUILDFLAG(IS_WIN)
  // On Windows, if `remote_process` is invalid when sending invitation, that
  // usually means the required remote process is not set in advance by sender,
  // in such case, rely on the connected named pipe to get the remote process
  // id, then open and set the remote process.
  if (!remote_process.IsValid()) {
    remote_process =
        OpenRemoteProcess(*transport_endpoint, /* remote_is_server= */ false);
  }
#endif

  IpczDriverHandle transport = CreateTransportForMojoEndpoint(
      {.source = config.is_broker ? Transport::kBroker : Transport::kNonBroker,
       .destination = is_isolated ? Transport::kBroker : Transport::kNonBroker},
      *transport_endpoint,
      {.is_peer_trusted = is_peer_elevated, .is_trusted_by_peer = true},
      std::move(remote_process), error_handler, error_handler_context,
      is_remote_process_untrusted);
  if (transport == IPCZ_INVALID_DRIVER_HANDLE) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  if (num_attachments_ == 0 || max_attachment_index_ != num_attachments_ - 1) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  // Note that we reserve the first initial portal for internal use, hence the
  // additional (kMaxAttachments + 1) portal here. Portals corresponding to
  // application-provided attachments begin at index 1.
  IpczHandle portals[kMaxAttachments + 1];
  IpczResult result = GetIpczAPI().ConnectNode(
      GetIpczNode(), transport, num_attachments_ + 1, flags, nullptr, portals);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  BaseSharedMemoryService::CreateService(ScopedIpczHandle(portals[0]));
  for (size_t i = 0; i < num_attachments_; ++i) {
    result = GetIpczAPI().MergePortals(attachments_[i].release(),
                                       portals[i + 1], IPCZ_NO_FLAGS, nullptr);
    CHECK_EQ(result, IPCZ_RESULT_OK);
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

  // Chromium's browser process may distinguish between normal child process
  // termination and a child process crash, based on the state of its Mojo
  // connection to the child process. This flag is implemented to allow child
  // processes to leak their transport so that it can stay alive right up until
  // normal process termination.
  bool leak_transport = false;
  bool is_isolated = false;
  bool inherit_broker = false;
  if (options) {
    if (options->struct_size < sizeof(*options)) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    leak_transport =
        (options->flags & MOJO_ACCEPT_INVITATION_FLAG_LEAK_TRANSPORT_ENDPOINT);
    is_isolated = (options->flags & MOJO_ACCEPT_INVITATION_FLAG_ISOLATED);
    inherit_broker =
        (options->flags & MOJO_ACCEPT_INVITATION_FLAG_INHERIT_BROKER);
  }

  auto invitation = base::MakeRefCounted<Invitation>();

  const IpczNodeOptions& config = GetIpczNodeOptions();
  if (is_isolated) {
    // Nodes using isolated invitations are required by MojoIpcz to both be
    // brokers.
    CHECK(config.is_broker && !inherit_broker);
  } else {
    CHECK(!config.is_broker);
  }

  IpczConnectNodeFlags flags = IPCZ_NO_FLAGS;
  if (!config.use_local_shared_memory_allocation) {
    flags |= IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
  }

  if (inherit_broker) {
    flags |= IPCZ_CONNECT_NODE_INHERIT_BROKER;
  } else {
    flags |= IPCZ_CONNECT_NODE_TO_BROKER;
  }

  const bool is_elevated =
      options && (options->flags & MOJO_ACCEPT_INVITATION_FLAG_ELEVATED) != 0;
#if !BUILDFLAG(IS_WIN)
  // For now, the concept of an elevated process is only meaningful on Windows.
  DCHECK(!is_elevated);
#endif

  // When accepting an invitation, we ConnectNode() with the maximum possible
  // number of initial portals: unlike ipcz, Mojo APIs have no way for this end
  // of a connection to express the expected number of attachments prior to
  // calling MojoAcceptInvitation().
  //
  // As the application extracts attachments, the corresponding initial portals
  // will be extracted from this set. Any unclaimed initial portals (which will
  // not have a peer on the sending node anyway) will be cleaned up when the
  // Invitation itself is destroyed.
  //
  // Note that we reserve the first portal slot for internal use, hence an
  // the additional (kMaxAttachments + 1) portal here. Portals corresponding to
  // application-provided attachments begin at index 1.
  IpczHandle portals[kMaxAttachments + 1];
  IpczDriverHandle transport = CreateTransportForMojoEndpoint(
      {.source = is_isolated ? Transport::kBroker : Transport::kNonBroker,
       .destination = Transport::kBroker},
      *transport_endpoint,
      {
          .is_peer_trusted = true,
          .is_trusted_by_peer = is_elevated,
          .leak_channel_on_shutdown = leak_transport,
      });
  if (transport == IPCZ_INVALID_DRIVER_HANDLE) {
    return IPCZ_INVALID_DRIVER_HANDLE;
  }

  // In an elevated Windows process our transport needs a handle to the broker's
  // own process. This is required to support transmission of arbitrary Windows
  // handles to and from the elevated process.
  base::Process remote_process;
#if BUILDFLAG(IS_WIN)
  if (is_elevated) {
    remote_process =
        OpenRemoteProcess(*transport_endpoint, /* remote_is_server= */ true);
  }
#endif
  if (remote_process.IsValid()) {
    Transport::FromHandle(transport)->set_remote_process(
        std::move(remote_process));
  }

  IpczResult result = GetIpczAPI().ConnectNode(
      GetIpczNode(), transport, kMaxAttachments + 1, flags, nullptr, portals);
  CHECK_EQ(result, IPCZ_RESULT_OK);

  BaseSharedMemoryService::CreateClient(ScopedIpczHandle(portals[0]));
  for (size_t i = 0; i < kMaxAttachments; ++i) {
    IpczHandle attachment;
    IpczHandle bridge;
    GetIpczAPI().OpenPortals(GetIpczNode(), IPCZ_NO_FLAGS, nullptr, &attachment,
                             &bridge);
    result = GetIpczAPI().MergePortals(portals[i + 1], bridge, IPCZ_NO_FLAGS,
                                       nullptr);
    invitation->attachments_[i] = ScopedIpczHandle(attachment);
  }
  invitation->num_attachments_ = kMaxAttachments;
  invitation->max_attachment_index_ = kMaxAttachments - 1;
  return Box(std::move(invitation));
}

void Invitation::Close() {}

}  // namespace mojo::core::ipcz_driver
