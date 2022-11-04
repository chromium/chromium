// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_
#define NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_

#include <vector>

#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/net_buildflags.h"

namespace net {

class TrustStore;

// The SystemTrustStore interface is used to encapsulate a TrustStore for the
// current platform, with some extra bells and whistles. Implementations must be
// thread-safe.
//
// This is primarily used to abstract out the platform-specific bits that
// relate to configuring the TrustStore needed for path building.
class SystemTrustStore {
 public:
  virtual ~SystemTrustStore() = default;

  // Returns an aggregate TrustStore that can be used by the path builder. The
  // store composes the system trust store (if implemented) with manually added
  // trust anchors added via AddTrustAnchor(). This pointer is non-owned, and
  // valid only for the lifetime of |this|. Any TrustStore objects returned from
  // this method must be thread-safe.
  virtual TrustStore* GetTrustStore() = 0;

  // Returns false if the implementation of SystemTrustStore doesn't actually
  // make use of the system's trust store. This might be the case for
  // unsupported platforms. In the case where this returns false, the trust
  // store returned by GetTrustStore() is made up solely of the manually added
  // trust anchors (via AddTrustAnchor()).
  //
  // TODO(hchao): Rename this to something more sensible now that we're
  // introducing the idea of a Chrome Root Store that doesn't use all parts of a
  // system's trust store.
  virtual bool UsesSystemTrustStore() const = 0;

  // IsKnownRoot() returns true if the given certificate originated from the
  // system trust store and is a "standard" one. The meaning of "standard" is
  // that it is one of default trust anchors for the system, as opposed to a
  // user-installed one.
  virtual bool IsKnownRoot(const ParsedCertificate* cert) const = 0;

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // Returns the current version of the Chrome Root Store being used. If
  // Chrome Root Store is not in use, returns 0.
  virtual int64_t chrome_root_store_version() = 0;
#endif
};

// Creates an instance of SystemTrustStore that wraps the current platform's SSL
// trust store. This cannot return nullptr, even in the case where system trust
// store integration is not supported.
//
// In cases where system trust store integration is not supported, the
// SystemTrustStore will not give access to the platform's SSL trust store, to
// avoid trusting a CA that the user has disabled on their system. In this
// case, UsesSystemTrustStore() will return false, and only manually-added trust
// anchors will be used.
NET_EXPORT std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore();

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class TrustStoreChrome;

// Creates an instance of SystemTrustStore that wraps the current platform's SSL
// trust store for user added roots, but uses the Chrome Root Store trust
// anchors. This cannot return nullptr, even in the case where system trust
// store integration is not supported.
//
// In cases where system trust store integration is not supported, the
// SystemTrustStore will not give access to the Chrome Root Store, to avoid
// trusting a CA that the user has disabled on their system. In this case,
// UsesSystemTrustStore() will return false, and only manually-added trust
// anchors will be used.
NET_EXPORT std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreChromeRoot(
    std::unique_ptr<TrustStoreChrome> chrome_root);

NET_EXPORT_PRIVATE std::unique_ptr<SystemTrustStore>
CreateSystemTrustStoreChromeForTesting(
    std::unique_ptr<TrustStoreChrome> trust_store_chrome,
    std::unique_ptr<TrustStore> trust_store_system);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

// Creates an instance of SystemTrustStore that initially does not have any
// trust roots. (This is the same trust store implementation that will be
// returned by CreateSslSystemTrustStore() on platforms where system trust
// store integration is not supported.)
NET_EXPORT std::unique_ptr<SystemTrustStore> CreateEmptySystemTrustStore();

#if BUILDFLAG(IS_MAC)
// Initializes trust cache on a worker thread, if the builtin verifier is
// enabled.
NET_EXPORT void InitializeTrustStoreMacCache();
#endif

}  // namespace net

#endif  // NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_
