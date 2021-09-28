// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_constants.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "remoting/host/mojo_ipc/mojo_ipc_util.h"

namespace remoting {

namespace {
constexpr char kRemoteWebAuthnIpcChannelName[] = "crd_remote_webauthn_ipc";
}  // namespace

const char kRemoteWebAuthnDataChannelName[] = "remote-webauthn";
const char kIsUvpaaMessageType[] = "isUvpaa";
const char kIsUvpaaResponseIsAvailableKey[] = "isAvailable";

const mojo::NamedPlatformChannel::ServerName& GetRemoteWebAuthnChannelName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(WorkingDirectoryIndependentServerNameFromUTF8(
          kRemoteWebAuthnIpcChannelName));
  return *server_name;
}

}  // namespace remoting
