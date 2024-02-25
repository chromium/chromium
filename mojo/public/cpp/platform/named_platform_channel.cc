// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/named_platform_channel.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace mojo {

const char NamedPlatformChannel::kNamedHandleSwitch[] =
    "mojo-named-platform-channel-pipe";

NamedPlatformChannel::NamedPlatformChannel(const Options& options) {
  server_endpoint_ = PlatformChannelServerEndpoint(
      CreateServerEndpoint(options, &server_name_));
}

NamedPlatformChannel::NamedPlatformChannel(NamedPlatformChannel&& other) =
    default;

NamedPlatformChannel& NamedPlatformChannel::operator=(
    NamedPlatformChannel&& other) = default;

NamedPlatformChannel::~NamedPlatformChannel() = default;

// static
NamedPlatformChannel::ServerName NamedPlatformChannel::ServerNameFromUTF8(
    std::string_view name) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(name);
#else
  return std::string(name);
#endif
}

void NamedPlatformChannel::PassServerNameOnCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchNative(kNamedHandleSwitch, server_name_);
}

// static
PlatformChannelEndpoint NamedPlatformChannel::ConnectToServer(
    const ServerName& server_name) {
  DCHECK(!server_name.empty());
  Options options = {.server_name = server_name};
  return CreateClientEndpoint(options);
}

// static
PlatformChannelEndpoint NamedPlatformChannel::ConnectToServer(
    const Options& options) {
  DCHECK(!options.server_name.empty());
  return CreateClientEndpoint(options);
}

// static
PlatformChannelEndpoint NamedPlatformChannel::ConnectToServer(
    const base::CommandLine& command_line) {
  ServerName name = command_line.GetSwitchValueNative(kNamedHandleSwitch);
  if (name.empty())
    return PlatformChannelEndpoint();
  return ConnectToServer(name);
}

}  // namespace mojo
