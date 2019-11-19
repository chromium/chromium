// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_
#define NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/internal/parsed_certificate.h"

namespace net {

class TrustStore;

// The SystemTrustStore interface is used to encapsulate a TrustStore for the
// current platform, with some extra bells and whistles.
//
// This is primarily used to abstract out the platform-specific bits that
// relate to configuring the TrustStore needed for path building.
//
// Implementations of SystemTrustStore create an effective trust
// store that is the composition of:
//
//   * The platform-specific trust store
//   * A set of manually added trust anchors
//   * Test certificates added via ScopedTestRoot
class SystemTrustStore {
 public:
  virtual ~SystemTrustStore() {}

  // Returns an aggregate TrustStore that can be used by the path builder. The
  // store composes the system trust store (if implemented) with manually added
  // trust anchors added via AddTrustAnchor(). This pointer is non-owned, and
  // valid only for the lifetime of |this|.
  virtual TrustStore* GetTrustStore() = 0;

  // Returns false if the implementation of SystemTrustStore doesn't actually
  // make use of the system's trust store. This might be the case for
  // unsupported platforms. In the case where this returns false, the trust
  // store returned by GetTrustStore() is made up solely of the manually added
  // trust anchors (via AddTrustAnchor()).
  virtual bool UsesSystemTrustStore() const = 0;

  // IsKnownRoot() returns true if the given certificate originated from the
  // system trust store and is a "standard" one. The meaning of "standard" is
  // that it is one of default trust anchors for the system, as opposed to a
  // user-installed one.
  virtual bool IsKnownRoot(const ParsedCertificate* cert) const = 0;

  // Adds a trust anchor to this particular instance of SystemTrustStore,
  // and not globally for the system.
  virtual void AddTrustAnchor(const scoped_refptr<ParsedCertificate>& cert) = 0;

  // Returns true if |trust_anchor| was one added via |AddTrustAnchor()|.
  virtual bool IsAdditionalTrustAnchor(const ParsedCertificate* cert) const = 0;
};

// Creates an instance of SystemTrustStore that wraps the current platform's SSL
// trust store. This canno return nullptr, even in the case where system trust
// store integration is not supported. In this latter case, the SystemTrustStore
// will only give access to the manually added trust anchors. This can be
// inspected by testing whether UsesSystemTrustStore() returns false.
NET_EXPORT std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore();

// Creates an instance of SystemTrustStore that initially does not have any
// trust roots. (This is the same trust store implementation that will be
// returned by CreateSslSystemTrustStore() on platforms where system trust
// store integration is not supported.)
NET_EXPORT std::unique_ptr<SystemTrustStore> CreateEmptySystemTrustStore();

}  // namespace net

#endif  // NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_
