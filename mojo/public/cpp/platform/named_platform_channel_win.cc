// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/named_platform_channel.h"

#include <windows.h>

#include <sddl.h>

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

namespace mojo {

namespace {

// A DACL to grant:
// GA = Generic All
// access to:
// SY = LOCAL_SYSTEM
// BA = BUILTIN_ADMINISTRATORS
// OW = OWNER_RIGHTS
constexpr wchar_t kDefaultSecurityDescriptor[] =
    L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;OW)";

}  // namespace

// static
NamedPlatformChannel::ServerName
NamedPlatformChannel::GenerateRandomServerName() {
  return base::StrCat({base::NumberToWString(::GetCurrentProcessId()), L".",
                       base::NumberToWString(::GetCurrentThreadId()), L".",
                       base::NumberToWString(base::RandUint64())});
}

// static
std::wstring NamedPlatformChannel::GetPipeNameFromServerName(
    const NamedPlatformChannel::ServerName& server_name,
    bool is_local_pipe) {
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea
  // "Windows 10, version 1709:  Pipes are only supported within an
  // app-container; ie, from one UWP process to another UWP process that's part
  // of the same app. Also, named pipes must use the syntax \\.\pipe\LOCAL\ for
  // the pipe name."
  //
  // Without "LOCAL" pipes can't be created inside an AppContainer sandbox.
  // However older versions of mojo didn't include the "LOCAL" segment, and to
  // communicate across versions both ends need to use the same pipe name.
  //
  // As a workaround, "LOCAL" is only included for local pipes that won't be
  // exposed to other apps. So AppContainer sandboxes can create PlatformChannel
  // pipes but not NamedPlatformChannel pipes, which must be opened in an
  // unsandboxed broker.
  return base::StrCat({L"\\\\.\\pipe", is_local_pipe ? L"\\LOCAL" : L"",
                       L"\\mojo.", server_name});
}

// static
PlatformChannelServerEndpoint NamedPlatformChannel::CreateServerEndpoint(
    const Options& options,
    ServerName* server_name) {
  ServerName name = options.server_name;
  if (name.empty())
    name = GenerateRandomServerName();

  PSECURITY_DESCRIPTOR security_desc = nullptr;
  ULONG security_desc_len = 0;
  PCHECK(::ConvertStringSecurityDescriptorToSecurityDescriptor(
      options.security_descriptor.empty() ? kDefaultSecurityDescriptor
                                          : options.security_descriptor.c_str(),
      SDDL_REVISION_1, &security_desc, &security_desc_len));
  std::unique_ptr<void, decltype(::LocalFree)*> p(security_desc, ::LocalFree);
  SECURITY_ATTRIBUTES security_attributes = {sizeof(SECURITY_ATTRIBUTES),
                                             security_desc, FALSE};

  const DWORD kOpenMode = options.enforce_uniqueness
                              ? PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                    FILE_FLAG_FIRST_PIPE_INSTANCE
                              : PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
  const DWORD kPipeMode =
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;

  CHECK(options.max_clients > 0 &&
        options.max_clients <= PIPE_UNLIMITED_INSTANCES);
  std::wstring pipe_name = GetPipeNameFromServerName(name);
  PlatformHandle handle(base::win::ScopedHandle(
      ::CreateNamedPipeW(pipe_name.c_str(), kOpenMode, kPipeMode,
                         options.max_clients,  // Max instances.
                         4096,                 // Out buffer size.
                         4096,                 // In buffer size.
                         5000,                 // Timeout in milliseconds.
                         &security_attributes)));

  *server_name = name;
  return PlatformChannelServerEndpoint(std::move(handle));
}

// static
PlatformChannelEndpoint NamedPlatformChannel::CreateClientEndpoint(
    const Options& options) {
  std::wstring pipe_name = GetPipeNameFromServerName(options.server_name);

  // Note: This may block.
  if (!::WaitNamedPipeW(pipe_name.c_str(), NMPWAIT_USE_DEFAULT_WAIT))
    return PlatformChannelEndpoint();

  const DWORD kDesiredAccess = GENERIC_READ | GENERIC_WRITE;
  // The SECURITY_ANONYMOUS flag means that the server side cannot impersonate
  // the client.
  const DWORD kFlags = SECURITY_SQOS_PRESENT |
                       (options.allow_impersonation ? SECURITY_IMPERSONATION
                                                    : SECURITY_ANONYMOUS) |
                       FILE_FLAG_OVERLAPPED;
  PlatformHandle handle(base::win::ScopedHandle(
      ::CreateFileW(pipe_name.c_str(), kDesiredAccess, 0, nullptr,
                    OPEN_EXISTING, kFlags, nullptr)));

  // The server may have stopped accepting a connection between the
  // WaitNamedPipe() and CreateFile(). If this occurs, an invalid handle is
  // returned.
  DPLOG_IF(ERROR, !handle.is_valid())
      << "Named pipe " << pipe_name
      << " could not be opened after WaitNamedPipe succeeded";
  return PlatformChannelEndpoint(std::move(handle));
}

}  // namespace mojo
