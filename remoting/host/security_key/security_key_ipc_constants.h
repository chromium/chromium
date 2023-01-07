// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CONSTANTS_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CONSTANTS_H_

#include <string>

#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace remoting {

// Used to indicate an error during a security key forwarding session.
extern const char kSecurityKeyConnectionError[];

// Returns the name of the well-known IPC server channel used to initiate a
// security key forwarding session.
const mojo::NamedPlatformChannel::ServerName& GetSecurityKeyIpcChannel();

// Sets the name of the well-known IPC server channel for testing purposes.
void SetSecurityKeyIpcChannelForTest(
    const mojo::NamedPlatformChannel::ServerName& server_name);

// Returns a path appropriate for placing a channel name.  Without this path
// prefix, we may not have permission on linux to bind(2) a socket to a name in
// the current directory.
std::string GetChannelNamePathPrefixForTest();

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CONSTANTS_H_
