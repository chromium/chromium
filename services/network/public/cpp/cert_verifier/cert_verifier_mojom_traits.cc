// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/cert_verifier_mojom_traits.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "net/cert/x509_certificate.h"

namespace mojo {

bool StructTraits<cert_verifier::mojom::RequestParamsDataView,
                  net::CertVerifier::RequestParams>::
    Read(cert_verifier::mojom::RequestParamsDataView data,
         net::CertVerifier::RequestParams* params) {
  scoped_refptr<net::X509Certificate> certificate;
  std::string hostname, ocsp_response, sct_list;
  if (!data.ReadCertificate(&certificate) || !data.ReadHostname(&hostname) ||
      !data.ReadOcspResponse(&ocsp_response) || !data.ReadSctList(&sct_list))
    return false;
  *params = net::CertVerifier::RequestParams(
      std::move(certificate), std::move(hostname), data.flags(),
      std::move(ocsp_response), std::move(sct_list));
  return true;
}

bool StructTraits<cert_verifier::mojom::CertVerifierConfigDataView,
                  net::CertVerifier::Config>::
    Read(cert_verifier::mojom::CertVerifierConfigDataView data,
         net::CertVerifier::Config* config) {
  mojo_base::BigBuffer crl_set_buffer;
  std::vector<scoped_refptr<net::X509Certificate>> additional_trust_anchors;
  std::vector<scoped_refptr<net::X509Certificate>>
      additional_untrusted_authorities;
  if (!data.ReadCrlSet(&crl_set_buffer) ||
      !data.ReadAdditionalTrustAnchors(&additional_trust_anchors) ||
      !data.ReadAdditionalUntrustedAuthorities(
          &additional_untrusted_authorities))
    return false;

  scoped_refptr<net::CRLSet> crl_set;
  if (crl_set_buffer.size() != 0) {
    // Make a copy from shared memory so we can avoid double-fetch bugs.
    std::string crl_set_string(
        reinterpret_cast<const char*>(crl_set_buffer.data()),
        crl_set_buffer.size());
    net::CRLSet::ParseAndStoreUnparsedData(crl_set_string, &crl_set);
  }

  config->enable_rev_checking = data.enable_rev_checking();
  config->require_rev_checking_local_anchors =
      data.require_rev_checking_local_anchors();
  config->enable_sha1_local_anchors = data.enable_sha1_local_anchors();
  config->disable_symantec_enforcement = data.disable_symantec_enforcement();
  config->crl_set = std::move(crl_set);
  config->additional_trust_anchors = std::move(additional_trust_anchors);
  config->additional_untrusted_authorities =
      std::move(additional_untrusted_authorities);
  return true;
}

}  // namespace mojo
