// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/spki_hash_set.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "net/base/hash_value.h"

namespace network {

// static
SPKIHashSet CreateSPKIHashSet(const std::vector<std::string>& fingerprints) {
  SPKIHashSet spki_hash_list;
  for (const std::string& fingerprint : fingerprints) {
    net::HashValue hash;
    if (!hash.FromString("sha256/" + fingerprint)) {
      LOG(ERROR) << "Invalid SPKI: " << fingerprint;
      continue;
    }
    net::SHA256HashValue sha256;
    DCHECK_EQ(hash.size(), sizeof(sha256));
    memcpy(&sha256, hash.data(), sizeof(sha256));
    spki_hash_list.insert(sha256);
  }
  return spki_hash_list;
}

}  // namespace network
