// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/stream_socket.h"

#include "base/notreached.h"

namespace net {

void StreamSocket::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  NOTREACHED();
}

absl::optional<base::StringPiece> StreamSocket::GetPeerApplicationSettings()
    const {
  return absl::nullopt;
}

void StreamSocket::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) const {
  NOTREACHED();
}

int StreamSocket::ConfirmHandshake(CompletionOnceCallback callback) {
  return OK;
}

}  // namespace net
