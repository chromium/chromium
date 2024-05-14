// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/stream_socket.h"

#include <string_view>

#include "base/notreached.h"

namespace net {

void StreamSocket::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  NOTREACHED_IN_MIGRATION();
}

std::optional<std::string_view> StreamSocket::GetPeerApplicationSettings()
    const {
  return std::nullopt;
}

void StreamSocket::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) const {
  NOTREACHED_IN_MIGRATION();
}

int StreamSocket::ConfirmHandshake(CompletionOnceCallback callback) {
  return OK;
}

}  // namespace net
