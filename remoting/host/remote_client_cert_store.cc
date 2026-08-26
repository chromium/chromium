// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_client_cert_store.h"

#include <utility>

#include "base/functional/callback.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace remoting {

RemoteClientCertStore::RemoteClientCertStore() = default;

RemoteClientCertStore::~RemoteClientCertStore() = default;

void RemoteClientCertStore::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  // Client certificate queries will be routed to the daemon process via Mojo
  // in a follow-up change.
  std::move(callback).Run({});
}

}  // namespace remoting
