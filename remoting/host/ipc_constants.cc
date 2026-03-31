// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_constants.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_util.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/base/username.h"

namespace remoting {

namespace {

#if !defined(NDEBUG)
// Use a different IPC name for debug builds so that we can run the host
// directly from out/Debug without interfering with the production host that
// might also be running.
constexpr char kChromotingHostServicesIpcName[] =
    "chromoting.host_services_debug_mojo_ipc";

#if BUILDFLAG(IS_LINUX)
constexpr char kLegacyChromotingHostServicesIpcNamePattern[] =
    "chromoting.%s.host_services_debug_mojo_ipc";
#endif

#else  // defined(NDEBUG)
constexpr char kChromotingHostServicesIpcName[] =
    "chromoting.host_services_mojo_ipc";

#if BUILDFLAG(IS_LINUX)
constexpr char kLegacyChromotingHostServicesIpcNamePattern[] =
    "chromoting.%s.host_services_mojo_ipc";
#endif

#endif

#if BUILDFLAG(IS_MAC)

#if !defined(NDEBUG)
constexpr char kAgentProcessBrokerIpcName[] =
    "chromoting.agent_process_broker_debug_mojo_ipc";
#else
// Must match the `MachServices` key in org.chromium.chromoting.broker.plist.
constexpr char kAgentProcessBrokerIpcName[] =
    "chromoting.agent_process_broker_mojo_ipc";
#endif

#endif

#if BUILDFLAG(IS_LINUX)

#if !defined(NDEBUG)
constexpr char kLoginSessionReporterIpcName[] =
    "chromoting.login_session_reporter_debug_mojo_ipc";
constexpr char kLoginSessionServerIpcName[] =
    "chromoting.login_session_server_debug_mojo_ipc";
#else
constexpr char kLoginSessionReporterIpcName[] =
    "chromoting.login_session_reporter_mojo_ipc";
constexpr char kLoginSessionServerIpcName[] =
    "chromoting.login_session_server_mojo_ipc";
#endif

#endif

}  // namespace

const base::FilePath::CharType kHostBinaryName[] =
    FILE_PATH_LITERAL("remoting_host");

const base::FilePath::CharType kDesktopBinaryName[] =
    FILE_PATH_LITERAL("remoting_desktop");

const uint64_t kChromotingHostServicesMessagePipeId = 0u;

bool GetInstalledBinaryPath(const base::FilePath::StringType& binary,
                            base::FilePath* full_path) {
  base::FilePath dir_path;
  if (!base::PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    return false;
  }

  base::FilePath path = dir_path.Append(binary);

#if BUILDFLAG(IS_WIN)
  path = path.ReplaceExtension(FILE_PATH_LITERAL("exe"));
#endif  // BUILDFLAG(IS_WIN)

  *full_path = path;
  return true;
}

const mojo::NamedPlatformChannel::ServerName&
GetChromotingHostServicesServerName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(
          named_mojo_ipc_server::WorkingDirectoryIndependentServerNameFromUTF8(
              kChromotingHostServicesIpcName));
  return *server_name;
}

#if BUILDFLAG(IS_LINUX)
const mojo::NamedPlatformChannel::ServerName&
GetLegacyChromotingHostServicesServerName() {
  // The legacy Linux single-process host is run as the login user, so we put
  // the username in the path in case there are multiple host services running
  // on the same machine.
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(
          named_mojo_ipc_server::WorkingDirectoryIndependentServerNameFromUTF8(
              base::StringPrintf(kLegacyChromotingHostServicesIpcNamePattern,
                                 GetUsername().c_str())));
  return *server_name;
}
#endif

#if BUILDFLAG(IS_MAC)

const char kAgentProcessBrokerMessagePipeId[] = "agent-process-broker";

const mojo::NamedPlatformChannel::ServerName&
GetAgentProcessBrokerServerName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(
          named_mojo_ipc_server::WorkingDirectoryIndependentServerNameFromUTF8(
              kAgentProcessBrokerIpcName));
  return *server_name;
}

#endif

#if BUILDFLAG(IS_LINUX)

const char kLoginSessionReporterMessagePipeId[] = "login-session-reporter";

const mojo::NamedPlatformChannel::ServerName&
GetLoginSessionReporterServerName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(
          named_mojo_ipc_server::WorkingDirectoryIndependentServerNameFromUTF8(
              kLoginSessionReporterIpcName));
  return *server_name;
}

const char kLoginSessionServerMessagePipeId[] = "login-session-server";

const mojo::NamedPlatformChannel::ServerName& GetLoginSessionServerName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(
          named_mojo_ipc_server::WorkingDirectoryIndependentServerNameFromUTF8(
              kLoginSessionServerIpcName));
  return *server_name;
}

#endif  // BUILDFLAG(IS_LINUX)

}  // namespace remoting
