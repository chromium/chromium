// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_OCSP_VERIFY_RESULT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_OCSP_VERIFY_RESULT_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/mojom/ocsp_verify_result.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/pki/ocsp.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::OCSPVerifyResultResponseStatus,
               bssl::OCSPVerifyResult::ResponseStatus> {
  static network::mojom::OCSPVerifyResultResponseStatus ToMojom(
      bssl::OCSPVerifyResult::ResponseStatus status);
  static bssl::OCSPVerifyResult::ResponseStatus FromMojom(
      network::mojom::OCSPVerifyResultResponseStatus input);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::OCSPRevocationStatus,
               bssl::OCSPRevocationStatus> {
  static network::mojom::OCSPRevocationStatus ToMojom(
      bssl::OCSPRevocationStatus status);
  static bssl::OCSPRevocationStatus FromMojom(
      network::mojom::OCSPRevocationStatus input);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::OCSPVerifyResultDataView,
                 bssl::OCSPVerifyResult> {
 public:
  static bssl::OCSPVerifyResult::ResponseStatus response_status(
      const bssl::OCSPVerifyResult& result) {
    return result.response_status;
  }
  static bssl::OCSPRevocationStatus revocation_status(
      const bssl::OCSPVerifyResult& result) {
    return result.revocation_status;
  }

  static bool Read(network::mojom::OCSPVerifyResultDataView data,
                   bssl::OCSPVerifyResult* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_OCSP_VERIFY_RESULT_MOJOM_TRAITS_H_
