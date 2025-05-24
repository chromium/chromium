// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_CALLER_SECURITY_CHECKER_H_
#define REMOTING_HOST_MOJO_CALLER_SECURITY_CHECKER_H_

namespace named_mojo_ipc_server {
struct ConnectionInfo;
}

namespace remoting {

// Returns true if the process referred to by |caller_pid| is a trusted mojo
// endpoint.
bool IsTrustedMojoEndpoint(const named_mojo_ipc_server::ConnectionInfo& caller);

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_CALLER_SECURITY_CHECKER_H_
