// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_
#define NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_

#include "base/containers/span.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/net_buildflags.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

struct ChromeRootCertConstraints;

// The SystemTrustStore interface is used to encapsulate a bssl::TrustStore for
// the current platform, with some extra bells and whistles. Implementations
// must be thread-safe.
//
// This is primarily used to abstract out the platform-specific bits that
// relate to configuring the bssl::TrustStore needed for path building.
class SystemTrustStore {
 public:
  virtual ~SystemTrustStore() = default;

  // Returns an aggregate bssl::TrustStore that can be used by the path builder.
  // The store composes the system trust store (if implemented) with manually
  // added trust anchors added via AddTrustAnchor(). This pointer is non-owned,
  // and valid only for the lifetime of |this|. Any bssl::TrustStore objects
  // returned from this method must be thread-safe.
  virtual bssl::TrustStore* GetTrustStore() = 0;

  // IsKnownRoot() returns true if the given certificate originated from the
  // system trust store and is a "standard" one. The meaning of "standard" is
  // that it is one of default trust anchors for the system, as opposed to a
  // user-installed one. (It may *also* be trusted as a user-installed root.)
  virtual bool IsKnownRoot(const bssl::ParsedCertificate* cert) const = 0;

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // Returns the PlatformTrustStore that can be used to look for
  // platform-specific user-added trust settings. This pointer is non-owned,
  // and valid only for the lifetime of |this|. Any net::PlatformTrustStore
  // objects returned from this method must be thread-safe.
  //
  // May return null if there is no PlatformTrustStore.
  virtual net::PlatformTrustStore* GetPlatformTrustStore() = 0;

  // IsLocallyTrustedRoot returns true if the given certificate is trusted in
  // the user-installed root store. (It may *also* be trusted in the Chrome
  // Root Store.)
  virtual bool IsLocallyTrustedRoot(
      const bssl::ParsedCertificate* trust_anchor) = 0;

  // Returns the current version of the Chrome Root Store being used. If
  // Chrome Root Store is not in use, returns 0.
  virtual int64_t chrome_root_store_version() const = 0;

  // Returns the Chrome Root Store constraints for `cert`, or nullptr if the
  // certificate is not constrained.
  virtual base::span<const ChromeRootCertConstraints> GetChromeRootConstraints(
      const bssl::ParsedCertificate* cert) const = 0;
#endif
};

#if BUILDFLAG(IS_FUCHSIA)
// Creates an instance of SystemTrustStore that wraps the current platform's SSL
// trust store. This cannot return nullptr.
NET_EXPORT std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore();
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class TrustStoreChrome;

// Creates an instance of SystemTrustStore that wraps the current platform's SSL
// trust store for user added roots, but uses the Chrome Root Store trust
// anchors. This cannot return nullptr.
NET_EXPORT std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreChromeRoot(
    std::unique_ptr<TrustStoreChrome> chrome_root);

// Creates an instance of SystemTrustStore that only uses the Chrome Root Store
// trust anchors.
// This cannot return nullptr.
NET_EXPORT std::unique_ptr<SystemTrustStore> CreateChromeOnlySystemTrustStore(
    std::unique_ptr<TrustStoreChrome> chrome_root);

NET_EXPORT_PRIVATE std::unique_ptr<SystemTrustStore>
CreateSystemTrustStoreChromeForTesting(
    std::unique_ptr<TrustStoreChrome> trust_store_chrome,
    std::unique_ptr<net::PlatformTrustStore> trust_store_system);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(IS_MAC)
// Initializes trust cache on a worker thread, if the builtin verifier is
// enabled.
NET_EXPORT void InitializeTrustStoreMacCache();
#endif

#if BUILDFLAG(IS_WIN)
// Initializes windows system trust store on a worker thread, if the builtin
// verifier is enabled.
NET_EXPORT void InitializeTrustStoreWinSystem();
#endif

#if BUILDFLAG(IS_ANDROID)
// Initializes Android system trust store on a worker thread, if the builtin
// verifier is enabled.
NET_EXPORT void InitializeTrustStoreAndroid();
#endif

}  // namespace net

#endif  // NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_H_
