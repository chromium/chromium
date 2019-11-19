// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/invitation.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

namespace {

static constexpr base::StringPiece kIsolatedPipeName = {"\0\0\0\0", 4};

void ProcessHandleToMojoProcessHandle(base::ProcessHandle target_process,
                                      MojoPlatformProcessHandle* handle) {
  handle->struct_size = sizeof(*handle);
#if defined(OS_WIN)
  handle->value =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(target_process));
#else
  handle->value = static_cast<uint64_t>(target_process);
#endif
}

void PlatformHandleToTransportEndpoint(
    PlatformHandle platform_handle,
    MojoPlatformHandle* endpoint_handle,
    MojoInvitationTransportEndpoint* endpoint) {
  PlatformHandle::ToMojoPlatformHandle(std::move(platform_handle),
                                       endpoint_handle);
  CHECK_NE(endpoint_handle->type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  endpoint->struct_size = sizeof(*endpoint);
  endpoint->num_platform_handles = 1;
  endpoint->platform_handles = endpoint_handle;
}

void RunErrorCallback(uintptr_t context,
                      const MojoProcessErrorDetails* details) {
  auto* callback = reinterpret_cast<ProcessErrorCallback*>(context);
  std::string error_message;
  if (details->error_message) {
    error_message =
        std::string(details->error_message, details->error_message_length - 1);
    callback->Run(error_message);
  } else if (details->flags & MOJO_PROCESS_ERROR_FLAG_DISCONNECTED) {
    delete callback;
  }
}

void SendInvitation(ScopedInvitationHandle invitation,
                    base::ProcessHandle target_process,
                    PlatformHandle endpoint_handle,
                    MojoInvitationTransportType transport_type,
                    MojoSendInvitationFlags flags,
                    const ProcessErrorCallback& error_callback,
                    base::StringPiece isolated_connection_name) {
  MojoPlatformProcessHandle process_handle;
  ProcessHandleToMojoProcessHandle(target_process, &process_handle);

  MojoPlatformHandle platform_handle;
  MojoInvitationTransportEndpoint endpoint;
  PlatformHandleToTransportEndpoint(std::move(endpoint_handle),
                                    &platform_handle, &endpoint);
  endpoint.type = transport_type;

  MojoProcessErrorHandler error_handler = nullptr;
  uintptr_t error_handler_context = 0;
  if (error_callback) {
    error_handler = &RunErrorCallback;

    // NOTE: The allocated callback is effectively owned by the error handler,
    // which will delete it on the final invocation for this context (i.e.
    // process disconnection).
    error_handler_context =
        reinterpret_cast<uintptr_t>(new ProcessErrorCallback(error_callback));
  }

  MojoSendInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  if (flags & MOJO_SEND_INVITATION_FLAG_ISOLATED) {
    options.isolated_connection_name = isolated_connection_name.data();
    options.isolated_connection_name_length =
        static_cast<uint32_t>(isolated_connection_name.size());
  }
  MojoResult result =
      MojoSendInvitation(invitation.get().value(), &process_handle, &endpoint,
                         error_handler, error_handler_context, &options);
  // If successful, the invitation handle is already closed for us.
  if (result == MOJO_RESULT_OK)
    ignore_result(invitation.release());
}

}  // namespace

OutgoingInvitation::OutgoingInvitation() {
  MojoHandle invitation_handle;
  MojoResult result = MojoCreateInvitation(nullptr, &invitation_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  handle_.reset(InvitationHandle(invitation_handle));
}

OutgoingInvitation::OutgoingInvitation(OutgoingInvitation&& other) = default;

OutgoingInvitation::~OutgoingInvitation() = default;

OutgoingInvitation& OutgoingInvitation::operator=(OutgoingInvitation&& other) =
    default;

ScopedMessagePipeHandle OutgoingInvitation::AttachMessagePipe(
    base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(name.size()));
  MojoHandle message_pipe_handle;
  MojoResult result = MojoAttachMessagePipeToInvitation(
      handle_.get().value(), name.data(), static_cast<uint32_t>(name.size()),
      nullptr, &message_pipe_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  return ScopedMessagePipeHandle(MessagePipeHandle(message_pipe_handle));
}

ScopedMessagePipeHandle OutgoingInvitation::AttachMessagePipe(uint64_t name) {
  return AttachMessagePipe(
      base::StringPiece(reinterpret_cast<const char*>(&name), sizeof(name)));
}

ScopedMessagePipeHandle OutgoingInvitation::ExtractMessagePipe(
    base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(name.size()));
  MojoHandle message_pipe_handle;
  MojoResult result = MojoExtractMessagePipeFromInvitation(
      handle_.get().value(), name.data(), static_cast<uint32_t>(name.size()),
      nullptr, &message_pipe_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  return ScopedMessagePipeHandle(MessagePipeHandle(message_pipe_handle));
}

ScopedMessagePipeHandle OutgoingInvitation::ExtractMessagePipe(uint64_t name) {
  return ExtractMessagePipe(
      base::StringPiece(reinterpret_cast<const char*>(&name), sizeof(name)));
}

// static
void OutgoingInvitation::Send(OutgoingInvitation invitation,
                              base::ProcessHandle target_process,
                              PlatformChannelEndpoint channel_endpoint,
                              const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                 MOJO_SEND_INVITATION_FLAG_NONE, error_callback, "");
}

// static
void OutgoingInvitation::Send(OutgoingInvitation invitation,
                              base::ProcessHandle target_process,
                              PlatformChannelServerEndpoint server_endpoint,
                              const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 server_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER,
                 MOJO_SEND_INVITATION_FLAG_NONE, error_callback, "");
}

// static
void OutgoingInvitation::SendAsync(OutgoingInvitation invitation,
                                   base::ProcessHandle target_process,
                                   PlatformChannelEndpoint channel_endpoint,
                                   const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_ASYNC,
                 MOJO_SEND_INVITATION_FLAG_NONE, error_callback, "");
}

// static
ScopedMessagePipeHandle OutgoingInvitation::SendIsolated(
    PlatformChannelEndpoint channel_endpoint,
    base::StringPiece connection_name) {
  mojo::OutgoingInvitation invitation;
  ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kIsolatedPipeName);
  SendInvitation(std::move(invitation.handle_), base::kNullProcessHandle,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                 MOJO_SEND_INVITATION_FLAG_ISOLATED, ProcessErrorCallback(),
                 connection_name);
  return pipe;
}

// static
ScopedMessagePipeHandle OutgoingInvitation::SendIsolated(
    PlatformChannelServerEndpoint server_endpoint,
    base::StringPiece connection_name) {
  mojo::OutgoingInvitation invitation;
  ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kIsolatedPipeName);
  SendInvitation(std::move(invitation.handle_), base::kNullProcessHandle,
                 server_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER,
                 MOJO_SEND_INVITATION_FLAG_ISOLATED, ProcessErrorCallback(),
                 connection_name);
  return pipe;
}

IncomingInvitation::IncomingInvitation() = default;

IncomingInvitation::IncomingInvitation(IncomingInvitation&& other) = default;

IncomingInvitation::IncomingInvitation(ScopedInvitationHandle handle)
    : handle_(std::move(handle)) {}

IncomingInvitation::~IncomingInvitation() = default;

IncomingInvitation& IncomingInvitation::operator=(IncomingInvitation&& other) =
    default;

// static
IncomingInvitation IncomingInvitation::Accept(
    PlatformChannelEndpoint channel_endpoint,
    MojoAcceptInvitationFlags flags) {
  MojoPlatformHandle endpoint_handle;
  PlatformHandle::ToMojoPlatformHandle(channel_endpoint.TakePlatformHandle(),
                                       &endpoint_handle);
  CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;

  MojoAcceptInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;

  MojoHandle invitation_handle;
  MojoResult result =
      MojoAcceptInvitation(&transport_endpoint, &options, &invitation_handle);
  if (result != MOJO_RESULT_OK)
    return IncomingInvitation();

  return IncomingInvitation(
      ScopedInvitationHandle(InvitationHandle(invitation_handle)));
}

// static
IncomingInvitation IncomingInvitation::AcceptAsync(
    PlatformChannelEndpoint channel_endpoint) {
  MojoPlatformHandle endpoint_handle;
  PlatformHandle::ToMojoPlatformHandle(channel_endpoint.TakePlatformHandle(),
                                       &endpoint_handle);
  CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_ASYNC;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;

  MojoHandle invitation_handle;
  MojoResult result =
      MojoAcceptInvitation(&transport_endpoint, nullptr, &invitation_handle);
  if (result != MOJO_RESULT_OK)
    return IncomingInvitation();

  return IncomingInvitation(
      ScopedInvitationHandle(InvitationHandle(invitation_handle)));
}

// static
ScopedMessagePipeHandle IncomingInvitation::AcceptIsolated(
    PlatformChannelEndpoint channel_endpoint) {
  MojoPlatformHandle endpoint_handle;
  PlatformHandle::ToMojoPlatformHandle(channel_endpoint.TakePlatformHandle(),
                                       &endpoint_handle);
  CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;

  MojoAcceptInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_ACCEPT_INVITATION_FLAG_ISOLATED;

  MojoHandle invitation_handle;
  MojoResult result =
      MojoAcceptInvitation(&transport_endpoint, &options, &invitation_handle);
  if (result != MOJO_RESULT_OK)
    return ScopedMessagePipeHandle();

  IncomingInvitation invitation{
      ScopedInvitationHandle(InvitationHandle(invitation_handle))};
  return invitation.ExtractMessagePipe(kIsolatedPipeName);
}

ScopedMessagePipeHandle IncomingInvitation::ExtractMessagePipe(
    base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(name.size()));
  DCHECK(handle_.is_valid());
  MojoHandle message_pipe_handle;
  MojoResult result = MojoExtractMessagePipeFromInvitation(
      handle_.get().value(), name.data(), static_cast<uint32_t>(name.size()),
      nullptr, &message_pipe_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  return ScopedMessagePipeHandle(MessagePipeHandle(message_pipe_handle));
}

ScopedMessagePipeHandle IncomingInvitation::ExtractMessagePipe(uint64_t name) {
  return ExtractMessagePipe(
      base::StringPiece(reinterpret_cast<const char*>(&name), sizeof(name)));
}

}  // namespace mojo
