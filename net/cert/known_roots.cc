// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/known_roots.h"

#include <string.h>

#include <algorithm>

#include "base/check_op.h"
#include "net/base/hash_value.h"
#include "net/cert/root_cert_list_generated.h"

namespace net {

namespace {

// Comparator-predicate that serves as a < function for comparing a
// RootCertData to a SHA256HashValue.
struct HashValueToRootCertDataComp {
  bool operator()(const SHA256HashValue& hash, const RootCertData& root_cert) {
    return hash < base::span(root_cert.sha256_spki_hash);
  }

  bool operator()(const RootCertData& root_cert, const SHA256HashValue& hash) {
    return base::span(root_cert.sha256_spki_hash) < hash;
  }
};

const RootCertData* GetRootCertData(const SHA256HashValue& spki_hash) {
  auto* it = std::lower_bound(std::begin(kRootCerts), std::end(kRootCerts),
                              spki_hash, HashValueToRootCertDataComp());
  if (it == std::end(kRootCerts) ||
      HashValueToRootCertDataComp()(spki_hash, *it)) {
    return nullptr;
  }
  return it;
}

}  // namespace

int32_t GetNetTrustAnchorHistogramIdForSPKI(const SHA256HashValue& spki_hash) {
  const RootCertData* root_data = GetRootCertData(spki_hash);
  if (!root_data)
    return 0;
  return root_data->histogram_id;
}

}  // namespace net
