// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cert_verify_proc_chromeos.h"

#include <utility>

#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

// NSS doesn't currently define CERT_LIST_TAIL.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=962413
// Can be removed once chrome requires NSS version 3.16 to build.
#ifndef CERT_LIST_TAIL
#define CERT_LIST_TAIL(l) ((CERTCertListNode*)PR_LIST_TAIL(&l->list))
#endif

namespace network {

namespace {

struct ChainVerifyArgs {
  CertVerifyProcChromeOS* cert_verify_proc;
  const net::CertificateList& additional_trust_anchors;
};

}  // namespace

CertVerifyProcChromeOS::CertVerifyProcChromeOS() {}

CertVerifyProcChromeOS::CertVerifyProcChromeOS(
    crypto::ScopedPK11Slot public_slot) {
  // Only the software slot is passed, since that is the only one where user
  // trust settings are stored.
  profile_filter_.Init(std::move(public_slot), crypto::ScopedPK11Slot(),
                       crypto::ScopedPK11Slot());
}

CertVerifyProcChromeOS::~CertVerifyProcChromeOS() {}

int CertVerifyProcChromeOS::VerifyInternal(
    net::X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    net::CRLSet* crl_set,
    const net::CertificateList& additional_trust_anchors,
    net::CertVerifyResult* verify_result) {
  ChainVerifyArgs chain_verify_args = {this, additional_trust_anchors};

  CERTChainVerifyCallback chain_verify_callback;
  chain_verify_callback.isChainValid =
      &CertVerifyProcChromeOS::IsChainValidFunc;
  chain_verify_callback.isChainValidArg =
      static_cast<void*>(&chain_verify_args);

  return VerifyInternalImpl(cert, hostname, ocsp_response, flags, crl_set,
                            additional_trust_anchors, &chain_verify_callback,
                            verify_result);
}

// static
SECStatus CertVerifyProcChromeOS::IsChainValidFunc(
    void* is_chain_valid_arg,
    const CERTCertList* current_chain,
    PRBool* chain_ok) {
  ChainVerifyArgs* args = static_cast<ChainVerifyArgs*>(is_chain_valid_arg);
  CERTCertificate* cert = CERT_LIST_TAIL(current_chain)->cert;

  if (net::TestRootCerts::HasInstance()) {
    if (net::TestRootCerts::GetInstance()->Contains(cert)) {
      // Certs in the TestRootCerts are not stored in any slot, and thus would
      // not be allowed by the profile_filter. This should only be hit in tests.
      DVLOG(3) << cert->subjectName << " is a TestRootCert";
      *chain_ok = PR_TRUE;
      return SECSuccess;
    }
  }

  for (net::CertificateList::const_iterator i =
           args->additional_trust_anchors.begin();
       i != args->additional_trust_anchors.end(); ++i) {
    if (net::x509_util::IsSameCertificate(cert, i->get())) {
      // Certs in the additional_trust_anchors should always be allowed, even if
      // they aren't stored in a slot that would be allowed by the
      // profile_filter.
      DVLOG(3) << cert->subjectName << " is an additional_trust_anchor";
      *chain_ok = PR_TRUE;
      return SECSuccess;
    }
  }

  // TODO(mattm): If crbug.com/334384 is fixed to allow setting trust
  // properly when the same cert is in multiple slots, this would also need
  // updating to check the per-slot trust values.
  *chain_ok = args->cert_verify_proc->profile_filter_.IsCertAllowed(cert)
                  ? PR_TRUE
                  : PR_FALSE;
  DVLOG(3) << cert->subjectName << " is " << (*chain_ok ? "ok" : "not ok");
  return SECSuccess;
}

}  // namespace network
