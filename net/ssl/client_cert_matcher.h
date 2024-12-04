// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_MATCHER_H_
#define NET_SSL_CLIENT_CERT_MATCHER_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

class NET_EXPORT ClientCertIssuerSource {
 public:
  virtual ~ClientCertIssuerSource() = default;

  // Returns certs from this source whose subject TLV is `name`.
  virtual std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> GetCertsByName(
      base::span<const uint8_t> name) = 0;
};

using ClientCertIssuerSourceCollection =
    std::vector<std::unique_ptr<ClientCertIssuerSource>>;

// Matches client certs against cert requests and builds path using an
// abstraction to get issuers from arbitrary sources.
// Filters the list of client certs in `identities` to only include those
// that match `request.
// This method might need to be run on a worker thread, for example if any
// of the ClientCertIssuerSource implementations can block.
NET_EXPORT void FilterMatchingClientCertIdentities(
    ClientCertIdentityList* identities,
    const SSLCertRequestInfo& request,
    const ClientCertIssuerSourceCollection& sources);

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_MATCHER_H_
