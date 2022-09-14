// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/known_roots_mac.h"

#include <Security/Security.h>

#include <algorithm>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "crypto/mac_security_services_lock.h"
#include "net/cert/x509_util_apple.h"

using base::ScopedCFTypeRef;

namespace net {

namespace {

// Helper class for managing the set of OS X Known Roots. This is only safe
// to initialize while the crypto::GetMacSecurityServicesLock() is held, due
// to calling into Security.framework functions; however, once initialized,
// it can be called at any time.
// In practice, due to lazy initialization, it's best to just always guard
// accesses with the lock.
class OSXKnownRootHelper {
 public:
  bool IsKnownRoot(SecCertificateRef cert) {
    // If there are no known roots, then an API failure occurred. For safety,
    // assume that all certificates are issued by known roots.
    if (known_roots_.empty())
      return true;

    HashValue hash(x509_util::CalculateFingerprint256(cert));
    return IsSHA256HashInSortedArray(hash, known_roots_);
  }

  bool IsKnownRoot(const HashValue& cert_sha256) {
    // If there are no known roots, then an API failure occurred. For safety,
    // assume that all certificates are issued by known roots.
    if (known_roots_.empty())
      return true;

    return IsSHA256HashInSortedArray(cert_sha256, known_roots_);
  }

 private:
  friend struct base::LazyInstanceTraitsBase<OSXKnownRootHelper>;

  OSXKnownRootHelper() {
    crypto::GetMacSecurityServicesLock().AssertAcquired();

    CFArrayRef cert_array = nullptr;
    OSStatus rv = SecTrustSettingsCopyCertificates(
        kSecTrustSettingsDomainSystem, &cert_array);
    if (rv != noErr) {
      LOG(ERROR) << "Unable to determine trusted roots; assuming all roots are "
                 << "trusted! Error " << rv;
      return;
    }
    base::ScopedCFTypeRef<CFArrayRef> scoped_array(cert_array);

    known_roots_.reserve(CFArrayGetCount(cert_array));
    for (CFIndex i = 0, size = CFArrayGetCount(cert_array); i < size; ++i) {
      SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(cert_array, i)));
      known_roots_.push_back(x509_util::CalculateFingerprint256(cert));
    }
    std::sort(known_roots_.begin(), known_roots_.end());
  }

  ~OSXKnownRootHelper() = default;

  std::vector<SHA256HashValue> known_roots_;
};

base::LazyInstance<OSXKnownRootHelper>::Leaky g_known_roots =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool IsKnownRoot(SecCertificateRef cert) {
  return g_known_roots.Get().IsKnownRoot(cert);
}

bool IsKnownRoot(const HashValue& cert_sha256) {
  return g_known_roots.Get().IsKnownRoot(cert_sha256);
}

void InitializeKnownRoots() {
  base::AutoLock lock(crypto::GetMacSecurityServicesLock());
  g_known_roots.Get();
}

}  // namespace net
