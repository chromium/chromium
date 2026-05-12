// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFY_RESULT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFY_RESULT_MOJOM_TRAITS_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/cert_verify_result.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/pki/ocsp.h"

namespace mojo {
template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::CertVerifyResultDataView,
                 net::CertVerifyResult> {
 public:
  static const scoped_refptr<net::X509Certificate>& verified_cert(
      const net::CertVerifyResult& result) {
    return result.verified_cert;
  }
  static uint32_t cert_status(const net::CertVerifyResult& result) {
    return result.cert_status;
  }
  static const std::vector<net::SHA256HashValue>& public_key_hashes(
      const net::CertVerifyResult& result) {
    return result.public_key_hashes;
  }
  static bool is_issued_by_known_root(const net::CertVerifyResult& result) {
    return result.is_issued_by_known_root;
  }
  static const bssl::OCSPVerifyResult& ocsp_result(
      const net::CertVerifyResult& result) {
    return result.ocsp_result;
  }
  static const net::SignedCertificateTimestampAndStatusList& scts(
      const net::CertVerifyResult& result) {
    return result.scts;
  }
  static net::ct::CTPolicyCompliance policy_compliance(
      const net::CertVerifyResult& result) {
    return result.policy_compliance;
  }
  static net::ct::CTRequirementsStatus ct_requirement_status(
      const net::CertVerifyResult& result) {
    return result.ct_requirement_status;
  }
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  static std::optional<int32_t> crs_root_id(
      const net::CertVerifyResult& result) {
    return result.crs_root_id;
  }
#endif

  static bool Read(network::mojom::CertVerifyResultDataView data,
                   net::CertVerifyResult* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFY_RESULT_MOJOM_TRAITS_H_
