// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TEST_ROOT_CERTS_H_
#define NET_CERT_TEST_ROOT_CERTS_H_

#include <set>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

#if BUILDFLAG(IS_IOS)
#include <CoreFoundation/CFArray.h>
#include <Security/SecTrust.h>
#include "base/apple/scoped_cftyperef.h"
#endif

namespace net {

class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;

class ThreadSafeTrustStoreInMemory : public bssl::TrustStore {
 public:
  // TrustStoreInMemory wrappers:
  bool IsEmpty() const;
  void Clear();
  void AddCertificate(std::shared_ptr<const bssl::ParsedCertificate> cert,
                      const bssl::CertificateTrust& trust);

  // TrustStore implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;

 private:
  mutable base::Lock lock_;
  bssl::TrustStoreInMemory impl_ GUARDED_BY(lock_);
};

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

  // Returns true if `der_cert` has been marked as a known root for testing.
  bool IsKnownRoot(base::span<const uint8_t> der_cert) const;

#if BUILDFLAG(IS_IOS)
  // Modifies the root certificates of |trust_ref| to include the
  // certificates stored in |temporary_roots_|. If IsEmpty() is true, this
  // does not modify |trust_ref|.
  OSStatus FixupSecTrustRef(SecTrustRef trust_ref) const;
#endif

  bssl::TrustStore* test_trust_store() { return &test_trust_store_; }

 private:
  friend class base::NoDestructor<TestRootCerts>;
  friend class ScopedTestRoot;
  friend class ScopedTestKnownRoot;

  TestRootCerts();
  ~TestRootCerts();

  // Marks |certificate| as trusted in the effective trust store
  // used by CertVerifier::Verify(). Returns false if the
  // certificate could not be marked trusted.
  bool Add(X509Certificate* certificate, bssl::CertificateTrust trust);

  // Marks |der_cert| as a known root. Does not change trust.
  void AddKnownRoot(base::span<const uint8_t> der_cert);

  // Performs platform-dependent operations.
  void Init() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool AddImpl(X509Certificate* certificate) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ClearImpl() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // `test_trust_store_` uses its own internal lock rather than the
  // TestRootCerts::lock_, since a pointer to the trust store is returned by
  // `test_trust_store` the methods on the trust store must themselves be
  // thread-safe.
  ThreadSafeTrustStoreInMemory test_trust_store_;

  mutable base::Lock lock_;

#if BUILDFLAG(IS_IOS)
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> temporary_roots_
      GUARDED_BY(lock_);
#endif

  std::set<std::string, std::less<>> test_known_roots_ GUARDED_BY(lock_);
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
  // |trust| may be specified to change the details of how the trust is
  // interpreted (applies only to CertVerifyProcBuiltin).
  explicit ScopedTestRoot(
      scoped_refptr<X509Certificate> cert,
      bssl::CertificateTrust trust = bssl::CertificateTrust::ForTrustAnchor());
  // Creates a ScopedTestRoot that adds |certs| to the TestRootCerts store.
  // |trust| may be specified to change the details of how the trust is
  // interpreted (applies only to CertVerifyProcBuiltin).
  explicit ScopedTestRoot(
      CertificateList certs,
      bssl::CertificateTrust trust = bssl::CertificateTrust::ForTrustAnchor());

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
  void Reset(
      CertificateList certs,
      bssl::CertificateTrust trust = bssl::CertificateTrust::ForTrustAnchor());

  // Returns true if this ScopedTestRoot has no certs assigned.
  bool IsEmpty() const { return certs_.empty(); }

 private:
  CertificateList certs_;
};

// Scoped helper for unittests to handle safely marking additional roots as
// known roots. Note that this does not trust the root. If the root should be
// trusted, a ScopedTestRoot should also be created.
//
// Limitations:
// Same as for ScopedTestRoot, see comment above.
class NET_EXPORT ScopedTestKnownRoot {
 public:
  ScopedTestKnownRoot();
  explicit ScopedTestKnownRoot(X509Certificate* cert);

  ScopedTestKnownRoot(const ScopedTestKnownRoot&) = delete;
  ScopedTestKnownRoot& operator=(const ScopedTestKnownRoot&) = delete;
  ScopedTestKnownRoot(ScopedTestKnownRoot&& other) = delete;
  ScopedTestKnownRoot& operator=(ScopedTestKnownRoot&& other) = delete;

  ~ScopedTestKnownRoot();

 private:
  CertificateList certs_;
};

}  // namespace net

#endif  // NET_CERT_TEST_ROOT_CERTS_H_
