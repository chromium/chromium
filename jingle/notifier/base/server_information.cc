// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/server_information.h"

#include "base/check_op.h"

namespace notifier {

ServerInformation::ServerInformation(
    const net::HostPortPair& server, SslTcpSupport ssltcp_support)
    : server(server), ssltcp_support(ssltcp_support) {
  DCHECK(!server.host().empty());
  DCHECK_GT(server.port(), 0);
}

ServerInformation::ServerInformation()
    : ssltcp_support(DOES_NOT_SUPPORT_SSLTCP) {}

ServerInformation::~ServerInformation() {}

bool ServerInformation::Equals(const ServerInformation& other) const {
  return
      server.Equals(other.server) &&
      (ssltcp_support == other.ssltcp_support);
}

}  // namespace notifier
