// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_empty.h"

#include <utility>

#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

ClientCertStoreEmpty::ClientCertStoreEmpty() = default;

ClientCertStoreEmpty::~ClientCertStoreEmpty() = default;

void ClientCertStoreEmpty::GetClientCerts(
    scoped_refptr<const SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  std::move(callback).Run(ClientCertIdentityList());
}

}  // namespace net
