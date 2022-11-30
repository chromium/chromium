// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_MOJO_IPC_UTIL_H_
#define REMOTING_HOST_MOJO_IPC_MOJO_IPC_UTIL_H_

#include "base/strings/string_piece_forward.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace remoting {

// Creates a server name that is independent to the working directory, i.e.
// it resolves to the same channel no matter which working directory you are
// running the binary from.
mojo::NamedPlatformChannel::ServerName
WorkingDirectoryIndependentServerNameFromUTF8(base::StringPiece name);

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_MOJO_IPC_UTIL_H_
