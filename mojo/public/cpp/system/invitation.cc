// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/invitation.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
#include "mojo/public/cpp/platform/platform_channel_server.h"
#endif

namespace mojo {

namespace {

static constexpr std::string_view kIsolatedPipeName = {"\0\0\0\0", 4};

void ProcessHandleToMojoProcessHandle(base::ProcessHandle target_process,
                                      MojoPlatformProcessHandle* handle) {
  handle->struct_size = sizeof(*handle);
#if BUILDFLAG(IS_WIN)
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
        std::string(details->error_message, details->error_message_length);
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
                    std::string_view isolated_connection_name) {
  std::unique_ptr<MojoPlatformProcessHandle> process_handle;
  if (target_process != base::kNullProcessHandle) {
    process_handle = std::make_unique<MojoPlatformProcessHandle>();
    ProcessHandleToMojoProcessHandle(target_process, process_handle.get());
  }

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
  MojoResult result = MojoSendInvitation(
      invitation.get().value(), process_handle.get(), &endpoint, error_handler,
      error_handler_context, &options);
  // If successful, the invitation handle is already closed for us.
  if (result == MOJO_RESULT_OK)
    std::ignore = invitation.release();
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
void WaitForServerConnection(
    PlatformChannelServerEndpoint server_endpoint,
    PlatformChannelServer::ConnectionCallback callback) {
  core::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformChannelServer::WaitForConnection,
                     std::move(server_endpoint), std::move(callback)));
}

base::Process CloneProcessFromHandle(base::ProcessHandle handle) {
  if (handle == base::kNullProcessHandle) {
    return base::Process{};
  }

#if BUILDFLAG(IS_WIN)
  // We can't use the hack below on Windows, because handle verification will
  // explode when a new Process instance tries to own the already-owned
  // `handle`.
  HANDLE new_handle;
  BOOL ok =
      ::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(),
                        &new_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  CHECK(ok);
  return base::Process(new_handle);
#else
  base::Process temporary_owner(handle);
  base::Process clone = temporary_owner.Duplicate();
  std::ignore = temporary_owner.Release();
  return clone;
#endif
}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)

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
    std::string_view name) {
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
      std::string_view(reinterpret_cast<const char*>(&name), sizeof(name)));
}

ScopedMessagePipeHandle OutgoingInvitation::ExtractMessagePipe(
    std::string_view name) {
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
      std::string_view(reinterpret_cast<const char*>(&name), sizeof(name)));
}

// static
void OutgoingInvitation::Send(OutgoingInvitation invitation,
                              base::ProcessHandle target_process,
                              PlatformChannelEndpoint channel_endpoint,
                              const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                 invitation.extra_flags_, error_callback, "");
}

// static
void OutgoingInvitation::Send(OutgoingInvitation invitation,
                              base::ProcessHandle target_process,
                              PlatformChannelServerEndpoint server_endpoint,
                              const ProcessErrorCallback& error_callback) {
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  WaitForServerConnection(
      std::move(server_endpoint),
      base::BindOnce(
          [](OutgoingInvitation invitation, base::Process target_process,
             const ProcessErrorCallback& error_callback,
             PlatformChannelEndpoint endpoint) {
            SendInvitation(std::move(invitation.handle_),
                           target_process.Handle(),
                           endpoint.TakePlatformHandle(),
                           MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                           invitation.extra_flags_, error_callback, "");
          },
          std::move(invitation), CloneProcessFromHandle(target_process),
          error_callback));
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
}

// static
void OutgoingInvitation::SendAsync(OutgoingInvitation invitation,
                                   base::ProcessHandle target_process,
                                   PlatformChannelEndpoint channel_endpoint,
                                   const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_ASYNC,
                 invitation.extra_flags_, error_callback, "");
}

// static
ScopedMessagePipeHandle OutgoingInvitation::SendIsolated(
    PlatformChannelEndpoint channel_endpoint,
    std::string_view connection_name,
    base::ProcessHandle target_process) {
  OutgoingInvitation invitation;
  ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kIsolatedPipeName);
  SendInvitation(std::move(invitation.handle_), target_process,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                 MOJO_SEND_INVITATION_FLAG_ISOLATED | invitation.extra_flags_,
                 ProcessErrorCallback(), connection_name);
  return pipe;
}

// static
ScopedMessagePipeHandle OutgoingInvitation::SendIsolated(
    PlatformChannelServerEndpoint server_endpoint,
    std::string_view connection_name,
    base::ProcessHandle target_process) {
  OutgoingInvitation invitation;
  ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kIsolatedPipeName);
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  WaitForServerConnection(
      std::move(server_endpoint),
      base::BindOnce(
          [](OutgoingInvitation invitation, base::Process target_process,
             const std::string& connection_name,
             PlatformChannelEndpoint endpoint) {
            SendInvitation(
                std::move(invitation.handle_), target_process.Handle(),
                endpoint.TakePlatformHandle(),
                MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                MOJO_SEND_INVITATION_FLAG_ISOLATED | invitation.extra_flags_,
                ProcessErrorCallback(), connection_name);
          },
          std::move(invitation), CloneProcessFromHandle(target_process),
          std::string(connection_name)));
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
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
    std::string_view name) {
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
      std::string_view(reinterpret_cast<const char*>(&name), sizeof(name)));
}

}  // namespace mojo
