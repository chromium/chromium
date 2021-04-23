// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_constants.h"

#include "base/no_destructor.h"
#include "build/build_config.h"

namespace remoting {

namespace {

#if defined(OS_LINUX)

// The channel name on Linux is the path to a unix domain socket, so it needs
// to be an absolute path to allow the IPC client binary to be executed from
// any working directory.
constexpr char kRemoteOpenUrlChannelName[] = "/tmp/crd_remote_open_url_ipc";

#else

// TODO(yuweih): Check if this IPC name works on other platforms.
constexpr char kRemoteOpenUrlChannelName[] = "crd_remote_open_url_ipc";

#endif

}  // namespace

const char kRemoteOpenUrlDataChannelName[] = "remote-open-url";

const mojo::NamedPlatformChannel::ServerName& GetRemoteOpenUrlIpcChannelName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(mojo::NamedPlatformChannel::ServerNameFromUTF8(
          kRemoteOpenUrlChannelName));
  return *server_name;
}

}  // namespace remoting
