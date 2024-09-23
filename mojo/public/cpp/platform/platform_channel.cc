// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/core/embedder/features.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include <zircon/process.h>
#include <zircon/processargs.h>

#include "base/fuchsia/fuchsia_logging.h"
#elif BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/global_descriptors.h"
#endif

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include <mach/port.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)
#include <sys/socket.h>
#elif BUILDFLAG(IS_NACL)
#include "native_client/src/public/imc_syscalls.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/binder.h"
#include "mojo/public/cpp/platform/binder_exchange.h"
#endif

namespace mojo {

namespace {

#if BUILDFLAG(IS_WIN)
void CreateChannel(PlatformHandle* local_endpoint,
                   PlatformHandle* remote_endpoint) {
  std::wstring pipe_name = NamedPlatformChannel::GetPipeNameFromServerName(
      NamedPlatformChannel::GenerateRandomServerName());
  DWORD kOpenMode =
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
  const DWORD kPipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE;
  *local_endpoint = PlatformHandle(base::win::ScopedHandle(
      ::CreateNamedPipeW(pipe_name.c_str(), kOpenMode, kPipeMode,
                         1,           // Max instances.
                         4096,        // Output buffer size.
                         4096,        // Input buffer size.
                         5000,        // Timeout in ms.
                         nullptr)));  // Default security descriptor.
  PCHECK(local_endpoint->is_valid());

  const DWORD kDesiredAccess = GENERIC_READ | GENERIC_WRITE;
  // The SECURITY_ANONYMOUS flag means that the server side cannot impersonate
  // the client.
  DWORD kFlags =
      SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS | FILE_FLAG_OVERLAPPED;
  // Allow the handle to be inherited by child processes.
  SECURITY_ATTRIBUTES security_attributes = {sizeof(SECURITY_ATTRIBUTES),
                                             nullptr, TRUE};
  *remote_endpoint = PlatformHandle(base::win::ScopedHandle(
      ::CreateFileW(pipe_name.c_str(), kDesiredAccess, 0, &security_attributes,
                    OPEN_EXISTING, kFlags, nullptr)));
  PCHECK(remote_endpoint->is_valid());

  // Since a client has connected, ConnectNamedPipe() should return zero and
  // GetLastError() should return ERROR_PIPE_CONNECTED.
  CHECK(!::ConnectNamedPipe(local_endpoint->GetHandle().Get(), nullptr));
  PCHECK(::GetLastError() == ERROR_PIPE_CONNECTED);
}
#elif BUILDFLAG(IS_FUCHSIA)
void CreateChannel(PlatformHandle* local_endpoint,
                   PlatformHandle* remote_endpoint) {
  zx::channel handles[2];
  zx_status_t result = zx::channel::create(0, &handles[0], &handles[1]);
  ZX_CHECK(result == ZX_OK, result);

  *local_endpoint = PlatformHandle(std::move(handles[0]));
  *remote_endpoint = PlatformHandle(std::move(handles[1]));
  DCHECK(local_endpoint->is_valid());
  DCHECK(remote_endpoint->is_valid());
}
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
void CreateChannel(PlatformHandle* local_endpoint,
                   PlatformHandle* remote_endpoint) {
  // Mach messaging is simplex; and in order to enable full-duplex
  // communication, the Mojo channel implementation performs an internal
  // handshake with its peer to establish two sets of Mach receive and send
  // rights. The handshake process starts with the creation of one
  // PlatformChannel endpoint.
  base::apple::ScopedMachReceiveRight receive;
  base::apple::ScopedMachSendRight send;
  // The mpl_qlimit specified here should stay in sync with
  // NamedPlatformChannel.
  CHECK(base::apple::CreateMachPort(&receive, &send, MACH_PORT_QLIMIT_LARGE));

  // In a reverse of Mach messaging semantics, in Mojo the "local" endpoint is
  // the send right, while the "remote" end is the receive right.
  *local_endpoint = PlatformHandle(std::move(send));
  *remote_endpoint = PlatformHandle(std::move(receive));
}
#elif BUILDFLAG(IS_POSIX)
void CreateChannel(PlatformHandle* local_endpoint,
                   PlatformHandle* remote_endpoint) {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(core::kMojoUseBinder) &&
      base::android::IsNativeBinderAvailable()) {
    auto [exchange0, exchange1] = CreateBinderExchange();
    *local_endpoint = PlatformHandle(std::move(exchange0));
    *remote_endpoint = PlatformHandle(std::move(exchange1));
    return;
  }
#endif  // BUILDFLAG_IS_ANDROID

  int fds[2];
#if BUILDFLAG(IS_NACL)
  PCHECK(imc_socketpair(fds) == 0);
#else
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  // Set non-blocking on both ends.
  PCHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
  PCHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);

#if BUILDFLAG(IS_APPLE)
  // This turns off |SIGPIPE| when writing to a closed socket, causing the call
  // to fail with |EPIPE| instead. On Linux we have to use |send...()| with
  // |MSG_NOSIGNAL| instead, which is not supported on Mac.
  int no_sigpipe = 1;
  PCHECK(setsockopt(fds[0], SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                    sizeof(no_sigpipe)) == 0);
  PCHECK(setsockopt(fds[1], SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                    sizeof(no_sigpipe)) == 0);
#endif  // BUILDFLAG(IS_APPLE)
#endif  // BUILDFLAG(IS_NACL)

  *local_endpoint = PlatformHandle(base::ScopedFD(fds[0]));
  *remote_endpoint = PlatformHandle(base::ScopedFD(fds[1]));
  DCHECK(local_endpoint->is_valid());
  DCHECK(remote_endpoint->is_valid());
}
#else
#error "Unsupported platform."
#endif

}  // namespace

const char PlatformChannel::kHandleSwitch[] = "mojo-platform-channel-handle";

PlatformChannel::PlatformChannel() {
  PlatformHandle local_handle;
  PlatformHandle remote_handle;
  CreateChannel(&local_handle, &remote_handle);
  local_endpoint_ = PlatformChannelEndpoint(std::move(local_handle));
  remote_endpoint_ = PlatformChannelEndpoint(std::move(remote_handle));
}

PlatformChannel::PlatformChannel(PlatformChannelEndpoint local,
                                 PlatformChannelEndpoint remote)
    : local_endpoint_(std::move(local)), remote_endpoint_(std::move(remote)) {}

PlatformChannel::PlatformChannel(PlatformChannel&& other) = default;

PlatformChannel& PlatformChannel::operator=(PlatformChannel&& other) = default;

PlatformChannel::~PlatformChannel() = default;

void PlatformChannel::PrepareToPassRemoteEndpoint(HandlePassingInfo* info,
                                                  std::string* value) {
  remote_endpoint_.PrepareToPass(*info, *value);
}

std::string PlatformChannel::PrepareToPassRemoteEndpoint(
    base::LaunchOptions& options) {
  return remote_endpoint_.PrepareToPass(options);
}

void PlatformChannel::PrepareToPassRemoteEndpoint(
    HandlePassingInfo* info,
    base::CommandLine* command_line) {
  remote_endpoint_.PrepareToPass(*info, *command_line);
}

void PlatformChannel::PrepareToPassRemoteEndpoint(
    base::LaunchOptions* options,
    base::CommandLine* command_line) {
  remote_endpoint_.PrepareToPass(*options, *command_line);
}

void PlatformChannel::RemoteProcessLaunchAttempted() {
  remote_endpoint_.ProcessLaunchAttempted();
}

// static
PlatformChannelEndpoint PlatformChannel::RecoverPassedEndpointFromString(
    std::string_view value) {
  return PlatformChannelEndpoint::RecoverFromString(value);
}

// static
PlatformChannelEndpoint PlatformChannel::RecoverPassedEndpointFromCommandLine(
    const base::CommandLine& command_line) {
  return RecoverPassedEndpointFromString(
      command_line.GetSwitchValueASCII(kHandleSwitch));
}

// static
bool PlatformChannel::CommandLineHasPassedEndpoint(
    const base::CommandLine& command_line) {
  return command_line.HasSwitch(kHandleSwitch);
}

}  // namespace mojo
