// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_X509_CERTIFICATE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_X509_CERTIFICATE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/x509_certificate.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::X509CertificateDataView,
                 scoped_refptr<net::X509Certificate>> {
 public:
  static bool IsNull(const scoped_refptr<net::X509Certificate>& cert) {
    return !cert;
  }
  static void SetToNull(scoped_refptr<net::X509Certificate>* output) {
    *output = nullptr;
  }

  static base::span<const uint8_t> cert(
      const scoped_refptr<net::X509Certificate>& cert) {
    return cert->cert_span();
  }

  static std::vector<base::span<const uint8_t>> intermediates(
      const scoped_refptr<net::X509Certificate>& cert);

  static bool Read(network::mojom::X509CertificateDataView data,
                   scoped_refptr<net::X509Certificate>* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_X509_CERTIFICATE_MOJOM_TRAITS_H_
