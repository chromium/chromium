// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/signed_certificate_timestamp_mojom_traits.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "services/network/public/mojom/signed_certificate_timestamp.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<network::mojom::DigitallySignedDataView,
                  net::ct::DigitallySigned>::
    Read(network::mojom::DigitallySignedDataView data,
         net::ct::DigitallySigned* out) {
  std::vector<uint8_t> signature_data;
  if (!data.ReadHashAlgorithm(&out->hash_algorithm) ||
      !data.ReadSignatureAlgorithm(&out->signature_algorithm) ||
      !data.ReadSignature(&signature_data)) {
    return false;
  }
  if (signature_data.empty()) {
    return false;
  }
  out->signature_data.assign(
      reinterpret_cast<const char*>(signature_data.data()),
      signature_data.size());
  return true;
}

// static
network::mojom::SCTVersion
EnumTraits<network::mojom::SCTVersion,
           net::ct::SignedCertificateTimestamp::Version>::
    ToMojom(net::ct::SignedCertificateTimestamp::Version type) {
  switch (type) {
    case net::ct::SignedCertificateTimestamp::V1:
      return network::mojom::SCTVersion::kV1;
  }
  NOTREACHED();
}

// static
net::ct::SignedCertificateTimestamp::Version
EnumTraits<network::mojom::SCTVersion,
           net::ct::SignedCertificateTimestamp::Version>::
    FromMojom(network::mojom::SCTVersion input) {
  switch (input) {
    case network::mojom::SCTVersion::kV1:
      return net::ct::SignedCertificateTimestamp::V1;
  }
  NOTREACHED();
}

// static
network::mojom::SCTOrigin
EnumTraits<network::mojom::SCTOrigin,
           net::ct::SignedCertificateTimestamp::Origin>::
    ToMojom(net::ct::SignedCertificateTimestamp::Origin type) {
  switch (type) {
    case net::ct::SignedCertificateTimestamp::SCT_EMBEDDED:
      return network::mojom::SCTOrigin::kEmbedded;
    case net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION:
      return network::mojom::SCTOrigin::kFromTlsExtension;
    case net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE:
      return network::mojom::SCTOrigin::kFromOcspResponse;
    case net::ct::SignedCertificateTimestamp::SCT_ORIGIN_MAX:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
net::ct::SignedCertificateTimestamp::Origin
EnumTraits<network::mojom::SCTOrigin,
           net::ct::SignedCertificateTimestamp::Origin>::
    FromMojom(network::mojom::SCTOrigin input) {
  switch (input) {
    case network::mojom::SCTOrigin::kEmbedded:
      return net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
    case network::mojom::SCTOrigin::kFromTlsExtension:
      return net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION;
    case network::mojom::SCTOrigin::kFromOcspResponse:
      return net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
  }
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::SignedCertificateTimestampDataView,
                  scoped_refptr<net::ct::SignedCertificateTimestamp>>::
    Read(network::mojom::SignedCertificateTimestampDataView data,
         scoped_refptr<net::ct::SignedCertificateTimestamp>* out) {
  *out = base::MakeRefCounted<net::ct::SignedCertificateTimestamp>();
  const auto& sct = *out;
  std::vector<uint8_t> extensions;
  if (!data.ReadVersion(&sct->version) || !data.ReadLogId(&sct->log_id) ||
      !data.ReadTimestamp(&sct->timestamp) ||
      !data.ReadExtensions(&extensions) ||
      !data.ReadSignature(&sct->signature) || !data.ReadOrigin(&sct->origin) ||
      !data.ReadLogDescription(&sct->log_description)) {
    return false;
  }
  sct->extensions.assign(extensions.begin(), extensions.end());
  return true;
}

}  // namespace mojo
