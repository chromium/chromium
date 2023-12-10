// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_NAMED_PLATFORM_CHANNEL_H_
#define MOJO_PUBLIC_CPP_PLATFORM_NAMED_PLATFORM_CHANNEL_H_

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/component_export.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_path.h"
#endif

namespace mojo {

// NamedPlatformChannel encapsulates a Mojo invitation transport channel which
// can listen for inbound connections established by clients connecting to
// a named system resource (i.e. a named pipe server on Windows, a named Unix
// domain socket on POSIX, a Mach bootstrap server on macOS, other platforms
// not supported).
//
// This can be especially useful when the local process has no way to transfer
// handles to the remote process, e.g. it does not control process launch or
// have any pre-existing communication channel to the process.
class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) NamedPlatformChannel {
 public:
  static const char kNamedHandleSwitch[];

#if BUILDFLAG(IS_WIN)
  using ServerName = std::wstring;
#else
  using ServerName = std::string;
#endif

  struct COMPONENT_EXPORT(MOJO_CPP_PLATFORM) Options {
    // Specifies the name to use for the server. If empty, a random name is
    // generated.
    ServerName server_name;

#if BUILDFLAG(IS_WIN)
    // If non-empty, a security descriptor to use when creating the pipe. If
    // empty, a default security descriptor will be used. See
    // |kDefaultSecurityDescriptor|.
    std::wstring security_descriptor;

    // If |true|, only a server endpoint will be allowed with the given name and
    // only one client will be able to connect. Otherwise many
    // NamedPlatformChannel instances can be created with the same name and
    // a different client can connect to each one.
    bool enforce_uniqueness = true;
#elif BUILDFLAG(IS_POSIX)
    // On POSIX, every new unnamed NamedPlatformChannel creates a server socket
    // with a random name. This controls the directory where that happens.
    // Ignored if |server_name| was set explicitly.
    base::FilePath socket_dir;

    // Use an abstract socket address instead of a filesystem path.
    bool use_abstract_namespace = false;
#endif
  };

  NamedPlatformChannel(const Options& options);
  NamedPlatformChannel(NamedPlatformChannel&& other);
  NamedPlatformChannel& operator=(NamedPlatformChannel&& other);
  ~NamedPlatformChannel();

  const PlatformChannelServerEndpoint& server_endpoint() const {
    return server_endpoint_;
  }

  // Helper to create a ServerName from a UTF8 string regardless of platform.
  static ServerName ServerNameFromUTF8(std::string_view name);

#if BUILDFLAG(IS_WIN)
  static ServerName GenerateRandomServerName();
  static std::wstring GetPipeNameFromServerName(const ServerName& server_name);
#endif

  // Passes the local server endpoint for the channel. On Windows, this is a
  // named pipe server; on POSIX it's a bound, listening domain socket. In each
  // case it should accept a single new connection.
  //
  // Use with PlatformChannelServer to wait for a new connection, yielding a
  // PlatformChannelEndpoint that is usable with the Mojo invitations API.
  [[nodiscard]] PlatformChannelServerEndpoint TakeServerEndpoint() {
    return std::move(server_endpoint_);
  }

  // Returns a name that can be used a remote process to connect to the server
  // endpoint.
  const ServerName& GetServerName() const { return server_name_; }

  // Passes the server name on |*command_line| using the common
  // |kNamedHandleSwitch| flag.
  void PassServerNameOnCommandLine(base::CommandLine* command_line);

  // Recovers a functioning client endpoint handle by creating a new endpoint
  // and connecting it to |server_name| if possible.
  [[nodiscard]] static PlatformChannelEndpoint ConnectToServer(
      const ServerName& server_name);

  // Like above, but passing an Options struct instead. |options.server_name|
  // must be a non-empty string.
  [[nodiscard]] static PlatformChannelEndpoint ConnectToServer(
      const Options& options);

  // Like above, but extracts the server name from |command_line| using the
  // common |kNamedHandleSwitch| flag.
  [[nodiscard]] static PlatformChannelEndpoint ConnectToServer(
      const base::CommandLine& command_line);

 private:
  static PlatformChannelServerEndpoint CreateServerEndpoint(
      const Options& options,
      ServerName* server_name);
  static PlatformChannelEndpoint CreateClientEndpoint(const Options& options);

  ServerName server_name_;
  PlatformChannelServerEndpoint server_endpoint_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_NAMED_PLATFORM_CHANNEL_H_
