// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_constants.h"

#include "base/lazy_instance.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#endif  // BUILDFLAG(IS_POSIX)

namespace {

base::LazyInstance<mojo::NamedPlatformChannel::ServerName>::DestructorAtExit
    g_security_key_ipc_channel_name = LAZY_INSTANCE_INITIALIZER;

constexpr char kSecurityKeyIpcChannelName[] = "security_key_ipc_channel";

}  // namespace

namespace remoting {

const char kSecurityKeyConnectionError[] = "ssh_connection_error";

const mojo::NamedPlatformChannel::ServerName& GetSecurityKeyIpcChannel() {
  if (g_security_key_ipc_channel_name.Get().empty()) {
    g_security_key_ipc_channel_name.Get() =
        mojo::NamedPlatformChannel::ServerNameFromUTF8(
            kSecurityKeyIpcChannelName);
  }

  return g_security_key_ipc_channel_name.Get();
}

void SetSecurityKeyIpcChannelForTest(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  g_security_key_ipc_channel_name.Get() = server_name;
}

std::string GetChannelNamePathPrefixForTest() {
  std::string base_path;
#if BUILDFLAG(IS_POSIX)
  base::FilePath base_file_path;
  if (base::GetTempDir(&base_file_path)) {
    base_path = base_file_path.AsEndingWithSeparator().value();
  } else {
    LOG(ERROR) << "Failed to retrieve temporary directory.";
  }
#endif  // BUILDFLAG(IS_POSIX)
  return base_path;
}

}  // namespace remoting
