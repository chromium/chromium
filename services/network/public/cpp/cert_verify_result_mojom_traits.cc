// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verify_result_mojom_traits.h"

#include "services/network/public/cpp/ct_policy_status_mojom_traits.h"
#include "services/network/public/cpp/hash_value_mojom_traits.h"
#include "services/network/public/cpp/ocsp_verify_result_mojom_traits.h"
#include "services/network/public/cpp/signed_certificate_timestamp_and_status_mojom_traits.h"
#include "services/network/public/cpp/x509_certificate_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    network::mojom::CertVerifyResultDataView,
    net::CertVerifyResult>::Read(network::mojom::CertVerifyResultDataView data,
                                 net::CertVerifyResult* out) {
  out->cert_status = data.cert_status();
  out->is_issued_by_known_root = data.is_issued_by_known_root();
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  out->crs_root_id = data.crs_root_id();
#endif
  return data.ReadVerifiedCert(&out->verified_cert) &&
         data.ReadPublicKeyHashes(&out->public_key_hashes) &&
         data.ReadOcspResult(&out->ocsp_result) && data.ReadScts(&out->scts) &&
         data.ReadPolicyCompliance(&out->policy_compliance) &&
         data.ReadCtRequirementStatus(&out->ct_requirement_status);
}

}  // namespace mojo
