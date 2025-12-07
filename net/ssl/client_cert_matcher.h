// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_MATCHER_H_
#define NET_SSL_CLIENT_CERT_MATCHER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "third_party/boringssl/src/include/openssl/base.h"

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

// Type for a callback that can be passed an IssuerSourceCollection.
using ClientCertIssuerSourceGetterCallback =
    base::OnceCallback<void(ClientCertIssuerSourceCollection)>;

// Type for a callback of a factory function for creating an
// IssuerSourceCollection. The factory callback is run and passed in a
// callback which is run with the result, possibly asynchronously.
using ClientCertIssuerSourceGetter =
    base::OnceCallback<void(ClientCertIssuerSourceGetterCallback)>;

// An implementation of ClientCertIssuerSource that searches a static set of
// certificates in memory.
class NET_EXPORT ClientCertIssuerSourceInMemory
    : public ClientCertIssuerSource {
 public:
  explicit ClientCertIssuerSourceInMemory(
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs);
  ~ClientCertIssuerSourceInMemory() override;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> GetCertsByName(
      base::span<const uint8_t> name) override;

 private:
  // Holds references to all the certificate buffers. This member will be
  // destroyed last, so it is safe for the cert_map_ key to reference the data
  // inside certificate without worrying about whether the key or value will
  // be destroyed first.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs_;

  // Mapping from subject TLV to certificate.
  std::multimap<base::raw_span<const uint8_t>, raw_ptr<CRYPTO_BUFFER>>
      cert_map_;
};

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
