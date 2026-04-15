// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIGNED_CERTIFICATE_TIMESTAMP_AND_STATUS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIGNED_CERTIFICATE_TIMESTAMP_AND_STATUS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "services/network/public/mojom/signed_certificate_timestamp_and_status.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::SignedCertificateTimestampAndStatusDataView,
                 net::SignedCertificateTimestampAndStatus> {
 public:
  static const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct(
      const net::SignedCertificateTimestampAndStatus& sct_and_status) {
    return sct_and_status.sct;
  }
  static net::ct::SCTVerifyStatus status(
      const net::SignedCertificateTimestampAndStatus& sct_and_status) {
    return sct_and_status.status;
  }

  static bool Read(
      network::mojom::SignedCertificateTimestampAndStatusDataView data,
      net::SignedCertificateTimestampAndStatus* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIGNED_CERTIFICATE_TIMESTAMP_AND_STATUS_MOJOM_TRAITS_H_
