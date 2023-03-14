// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_test_nss_db.h"

#include <cert.h>

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"

namespace crypto {

ScopedTestNSSDB::ScopedTestNSSDB() {
  EnsureNSSInit();
  // NSS is allowed to do IO on the current thread since dispatching
  // to a dedicated thread would still have the affect of blocking
  // the current thread, due to NSS's internal locking requirements
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (!temp_dir_.CreateUniqueTempDir())
    return;

  const char kTestDescription[] = "Test DB";
  slot_ = OpenSoftwareNSSDB(temp_dir_.GetPath(), kTestDescription);
}

ScopedTestNSSDB::~ScopedTestNSSDB() {
  // Remove trust from any certs in the test DB before closing it. Otherwise NSS
  // may cache verification results even after the test DB is gone.
  RemoveTrustFromAllCerts();

  // NSS is allowed to do IO on the current thread since dispatching
  // to a dedicated thread would still have the affect of blocking
  // the current thread, due to NSS's internal locking requirements
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (slot_) {
    SECStatus status = CloseSoftwareNSSDB(slot_.get());
    if (status != SECSuccess)
      PLOG(ERROR) << "CloseSoftwareNSSDB failed: " << PORT_GetError();
  }

  if (!temp_dir_.Delete())
    LOG(ERROR) << "Could not delete temporary directory.";
}

void ScopedTestNSSDB::RemoveTrustFromAllCerts() {
  if (!slot_)
    return;

  ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot_.get()));
  if (!cert_list)
    return;

  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    CERTCertTrust trust = {0};
    if (CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), node->cert, &trust) !=
        SECSuccess) {
      LOG(ERROR) << "CERT_ChangeCertTrust failed: " << PORT_GetError();
    }
  }
}

}  // namespace crypto
