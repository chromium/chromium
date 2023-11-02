// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TEST_ROOT_CERTS_H_
#define NET_CERT_TEST_ROOT_CERTS_H_

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/cert/pki/trust_store_in_memory.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#elif BUILDFLAG(IS_APPLE)
#include <CoreFoundation/CFArray.h>
#include <Security/SecTrust.h>
#include "base/mac/scoped_cftyperef.h"
#endif

namespace net {

class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;

// TestRootCerts is a helper class for unit tests that is used to
// artificially mark a certificate as trusted, independent of the local
// machine configuration.
//
// Test roots can be added using the ScopedTestRoot class below. See the
// class documentation for usage and limitations.
class NET_EXPORT TestRootCerts {
 public:
  // Obtains the Singleton instance to the trusted certificates.
  static TestRootCerts* GetInstance();

  TestRootCerts(const TestRootCerts&) = delete;
  TestRootCerts& operator=(const TestRootCerts&) = delete;

  // Returns true if an instance exists, without forcing an initialization.
  static bool HasInstance();

  // Clears the trusted status of any certificates that were previously
  // marked trusted via Add().
  void Clear();

  // Returns true if there are no certificates that have been marked trusted.
  bool IsEmpty() const;

#if BUILDFLAG(IS_APPLE)
  CFArrayRef temporary_roots() const { return temporary_roots_; }

  // Modifies the root certificates of |trust_ref| to include the
  // certificates stored in |temporary_roots_|. If IsEmpty() is true, this
  // does not modify |trust_ref|.
  OSStatus FixupSecTrustRef(SecTrustRef trust_ref) const;
#elif BUILDFLAG(IS_WIN)
  HCERTSTORE temporary_roots() const { return temporary_roots_; }

  // Returns an HCERTCHAINENGINE suitable to be used for certificate
  // validation routines, or NULL to indicate that the default system chain
  // engine is appropriate.
  crypto::ScopedHCERTCHAINENGINE GetChainEngine() const;
#endif

  TrustStore* test_trust_store() { return &test_trust_store_; }

 private:
  friend struct base::LazyInstanceTraitsBase<TestRootCerts>;
  friend class ScopedTestRoot;

  TestRootCerts();
  ~TestRootCerts();

  // Marks |certificate| as trusted in the effective trust store
  // used by CertVerifier::Verify(). Returns false if the
  // certificate could not be marked trusted.
  bool Add(X509Certificate* certificate);

  // Performs platform-dependent operations.
  void Init();
  bool AddImpl(X509Certificate* certificate);
  void ClearImpl();

#if BUILDFLAG(IS_WIN)
  HCERTSTORE temporary_roots_;
#elif BUILDFLAG(IS_APPLE)
  base::ScopedCFTypeRef<CFMutableArrayRef> temporary_roots_;
#endif

  TrustStoreInMemory test_trust_store_;
};

// Scoped helper for unittests to handle safely managing trusted roots.
//
// Limitations:
// Multiple instances of ScopedTestRoot may be created at once, which will
// trust the union of the certs provided. However, when one of the
// ScopedTestRoot instances removes its trust, either by going out of scope, or
// by Reset() being called, *all* test root certs will be untrusted. (This
// limitation could be removed if a reason arises.)
class NET_EXPORT ScopedTestRoot {
 public:
  ScopedTestRoot();
  // Creates a ScopedTestRoot that adds |cert| to the TestRootCerts store.
  explicit ScopedTestRoot(X509Certificate* cert);
  // Creates a ScopedTestRoot that adds |certs| to the TestRootCerts store.
  explicit ScopedTestRoot(CertificateList certs);

  ScopedTestRoot(const ScopedTestRoot&) = delete;
  ScopedTestRoot& operator=(const ScopedTestRoot&) = delete;

  ScopedTestRoot(ScopedTestRoot&& other);
  ScopedTestRoot& operator=(ScopedTestRoot&& other);

  ~ScopedTestRoot();

  // Assigns |certs| to be the new test root certs. If |certs| is empty, undoes
  // any work the ScopedTestRoot may have previously done.
  // If |certs_| contains certificates (due to a prior call to Reset or due to
  // certs being passed at construction), the existing TestRootCerts store is
  // cleared.
  void Reset(CertificateList certs);

  // Returns true if this ScopedTestRoot has no certs assigned.
  bool IsEmpty() const { return certs_.empty(); }

 private:
  CertificateList certs_;
};

}  // namespace net

#endif  // NET_CERT_TEST_ROOT_CERTS_H_
