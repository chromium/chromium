// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CERT_VERIFY_PROC_CHROMEOS_H_
#define SERVICES_NETWORK_CERT_VERIFY_PROC_CHROMEOS_H_

#include "base/component_export.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_verify_proc_nss.h"
#include "net/cert/nss_profile_filter_chromeos.h"

namespace network {

// Wrapper around CertVerifyProcNSS which allows filtering trust decisions on a
// per-slot basis.
//
// Note that only the simple case is currently handled (if a slot contains a new
// trust root, that root should not be trusted by CertVerifyProcChromeOS
// instances using other slots). More complicated cases are not handled (like
// two slots adding the same root cert but with different trust values).
class COMPONENT_EXPORT(NETWORK_SERVICE) CertVerifyProcChromeOS
    : public net::CertVerifyProcNSS {
 public:
  // Creates a CertVerifyProc that doesn't allow any user-provided trust roots.
  CertVerifyProcChromeOS();

  // Creates a CertVerifyProc that doesn't allow trust roots provided by
  // users other than the specified slot.
  explicit CertVerifyProcChromeOS(crypto::ScopedPK11Slot public_slot);

 protected:
  ~CertVerifyProcChromeOS() override;

 private:
  // net::CertVerifyProcNSS implementation:
  int VerifyInternal(net::X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     net::CRLSet* crl_set,
                     const net::CertificateList& additional_trust_anchors,
                     net::CertVerifyResult* verify_result) override;

  // Check if the trust root of |current_chain| is allowed.
  // |is_chain_valid_arg| is actually a ChainVerifyArgs*, which is used to pass
  // state through the NSS CERTChainVerifyCallback.isChainValidArg parameter.
  // If the chain is allowed, |*chain_ok| will be set to PR_TRUE.
  // If the chain is not allowed, |*chain_ok| is set to PR_FALSE, and this
  // function may be called again during a single certificate verification if
  // there are multiple possible valid chains.
  static SECStatus IsChainValidFunc(void* is_chain_valid_arg,
                                    const CERTCertList* current_chain,
                                    PRBool* chain_ok);

  net::NSSProfileFilterChromeOS profile_filter_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_CERT_VERIFY_PROC_CHROMEOS_H_
