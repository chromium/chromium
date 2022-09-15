// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_CONSTANTS_H_
#define REMOTING_HOST_IPC_CONSTANTS_H_

#include "base/files/file_path.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace remoting {

// Name of the host process binary.
extern const base::FilePath::CharType kHostBinaryName[];

// Name of the desktop process binary.
extern const base::FilePath::CharType kDesktopBinaryName[];

// Returns the full path to an installed |binary| in |full_path|.
bool GetInstalledBinaryPath(const base::FilePath::StringType& binary,
                            base::FilePath* full_path);

// Returns the server name for chromoting host services.
const mojo::NamedPlatformChannel::ServerName&
GetChromotingHostServicesServerName();

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_CONSTANTS_H_
