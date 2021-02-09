// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_auth_cert_observer.h"

#include "net/base/net_errors.h"

namespace network {

TestAuthCertObserver::TestAuthCertObserver() = default;
TestAuthCertObserver::~TestAuthCertObserver() = default;

mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver>
TestAuthCertObserver::Bind() {
  mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void TestAuthCertObserver::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(ignore_certificate_errors_ ? net::OK : net_error);
}

void TestAuthCertObserver::Clone(
    mojo::PendingReceiver<AuthenticationAndCertificateObserver> observer) {
  receivers_.Add(this, std::move(observer));
}

}  // namespace network
