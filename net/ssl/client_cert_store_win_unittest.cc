// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_win.h"

#include "net/ssl/client_cert_store_unittest-inl.h"

namespace net {

class ClientCertStoreWinTestDelegate {
 public:
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         ClientCertIdentityList* selected_certs) {
    return store_.SelectClientCertsForTesting(
        input_certs, cert_request_info, selected_certs);
  }

 private:
  ClientCertStoreWin store_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(Win,
                               ClientCertStoreTest,
                               ClientCertStoreWinTestDelegate);

}  // namespace net
