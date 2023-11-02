// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector.h"

namespace remoting {

// static
// Dummy implementation that returns nullptr for unsupported platforms, i.e.
// Mac.
// TODO(yuweih): Implement MojoServerEndpointConnector for Mac.
std::unique_ptr<MojoServerEndpointConnector>
MojoServerEndpointConnector::Create(Delegate* delegate) {
  return nullptr;
}

}  // namespace remoting
