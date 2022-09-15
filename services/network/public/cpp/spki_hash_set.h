// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SPKI_HASH_SET_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SPKI_HASH_SET_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"

namespace net {
struct SHA256HashValue;
}  // namespace net

namespace network {

// SPKIHashSet is a set of SHA-256 SPKI fingerprints (RFC 7469, Section 2.4).
using SPKIHashSet = base::flat_set<net::SHA256HashValue>;

// CreateSPKIHashSet converts a vector of Base64-encoded SHA-256 SPKI
// fingerprints into an SPKIHashSet. Invalid fingerprints are logged and
// skipped.
COMPONENT_EXPORT(NETWORK_CPP)
SPKIHashSet CreateSPKIHashSet(const std::vector<std::string>& fingerprints);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SPKI_HASH_SET_H_
