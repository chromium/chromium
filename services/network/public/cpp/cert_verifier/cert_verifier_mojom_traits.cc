// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/cert_verifier_mojom_traits.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/cert/x509_certificate.h"

namespace mojo {

bool StructTraits<cert_verifier::mojom::RequestParamsDataView,
                  net::CertVerifier::RequestParams>::
    Read(cert_verifier::mojom::RequestParamsDataView data,
         net::CertVerifier::RequestParams* params) {
  scoped_refptr<net::X509Certificate> certificate;
  std::string hostname, ocsp_response, sct_list;
  if (!data.ReadCertificate(&certificate) || !data.ReadHostname(&hostname) ||
      !data.ReadOcspResponse(&ocsp_response) || !data.ReadSctList(&sct_list)) {
    return false;
  }
  *params = net::CertVerifier::RequestParams(
      std::move(certificate), std::move(hostname), data.flags(),
      std::move(ocsp_response), std::move(sct_list));
  return true;
}

bool StructTraits<cert_verifier::mojom::CertVerifierConfigDataView,
                  net::CertVerifier::Config>::
    Read(cert_verifier::mojom::CertVerifierConfigDataView data,
         net::CertVerifier::Config* config) {
  config->enable_rev_checking = data.enable_rev_checking();
  config->require_rev_checking_local_anchors =
      data.require_rev_checking_local_anchors();
  config->enable_sha1_local_anchors = data.enable_sha1_local_anchors();
  config->disable_symantec_enforcement = data.disable_symantec_enforcement();
  return true;
}

}  // namespace mojo
