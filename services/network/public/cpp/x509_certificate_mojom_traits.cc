// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/x509_certificate_mojom_traits.h"

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/strings/string_view_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "services/network/public/mojom/x509_certificate.mojom-shared.h"

namespace mojo {

// static
std::vector<base::span<const uint8_t>>
StructTraits<network::mojom::X509CertificateDataView,
             scoped_refptr<net::X509Certificate>>::
    intermediates(const scoped_refptr<net::X509Certificate>& cert) {
  // Mojo serializes eagerly, so the returned vector of spans will not outlive
  // `cert`.
  std::vector<base::span<const uint8_t>> cert_chain;
  cert_chain.reserve(cert->intermediate_buffers().size());
  for (const auto& buffer : cert->intermediate_buffers()) {
    cert_chain.push_back(net::x509_util::CryptoBufferAsSpan(buffer.get()));
  }
  return cert_chain;
}

// static
bool StructTraits<network::mojom::X509CertificateDataView,
                  scoped_refptr<net::X509Certificate>>::
    Read(network::mojom::X509CertificateDataView data,
         scoped_refptr<net::X509Certificate>* out) {
  mojo::ArrayDataView<uint8_t> cert_view;
  data.GetCertDataView(&cert_view);
  mojo::ArrayDataView<mojo::ArrayDataView<uint8_t>> intermediates_view;
  data.GetIntermediatesDataView(&intermediates_view);

  std::vector<std::string_view> cert_chain;
  cert_chain.reserve(intermediates_view.size() + 1);
  cert_chain.push_back(base::as_string_view(cert_view));
  for (size_t i = 0; i < intermediates_view.size(); ++i) {
    mojo::ArrayDataView<uint8_t> intermediate_view;
    intermediates_view.GetDataView(i, &intermediate_view);
    cert_chain.push_back(base::as_string_view(intermediate_view));
  }

  net::X509Certificate::UnsafeCreateOptions options;
  // Setting the `printable_string_is_utf8` option to be true here is necessary
  // to round-trip any X509Certificate objects that were parsed with this
  // option in the first place.
  // See https://crbug.com/770323 and https://crbug.com/788655.
  options.printable_string_is_utf8 = true;
  *out = net::X509Certificate::CreateFromDERCertChainUnsafeOptions(cert_chain,
                                                                   options);

  return *out != nullptr;
}

}  // namespace mojo
