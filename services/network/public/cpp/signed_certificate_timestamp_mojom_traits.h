// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIGNED_CERTIFICATE_TIMESTAMP_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIGNED_CERTIFICATE_TIMESTAMP_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "services/network/public/mojom/signed_certificate_timestamp.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::HashAlgorithm,
                  net::ct::DigitallySigned::HashAlgorithm> {
  static network::mojom::HashAlgorithm ToMojom(
      net::ct::DigitallySigned::HashAlgorithm input) {
    switch (input) {
      case net::ct::DigitallySigned::HASH_ALGO_NONE:
        return network::mojom::HashAlgorithm::HASH_ALGO_NONE;
      case net::ct::DigitallySigned::HASH_ALGO_MD5:
        return network::mojom::HashAlgorithm::HASH_ALGO_MD5;
      case net::ct::DigitallySigned::HASH_ALGO_SHA1:
        return network::mojom::HashAlgorithm::HASH_ALGO_SHA1;
      case net::ct::DigitallySigned::HASH_ALGO_SHA224:
        return network::mojom::HashAlgorithm::HASH_ALGO_SHA224;
      case net::ct::DigitallySigned::HASH_ALGO_SHA256:
        return network::mojom::HashAlgorithm::HASH_ALGO_SHA256;
      case net::ct::DigitallySigned::HASH_ALGO_SHA384:
        return network::mojom::HashAlgorithm::HASH_ALGO_SHA384;
      case net::ct::DigitallySigned::HASH_ALGO_SHA512:
        return network::mojom::HashAlgorithm::HASH_ALGO_SHA512;
    }
    NOTREACHED();
  }

  static net::ct::DigitallySigned::HashAlgorithm FromMojom(
      network::mojom::HashAlgorithm input) {
    switch (input) {
      case network::mojom::HashAlgorithm::HASH_ALGO_NONE:
        return net::ct::DigitallySigned::HASH_ALGO_NONE;
      case network::mojom::HashAlgorithm::HASH_ALGO_MD5:
        return net::ct::DigitallySigned::HASH_ALGO_MD5;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA1:
        return net::ct::DigitallySigned::HASH_ALGO_SHA1;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA224:
        return net::ct::DigitallySigned::HASH_ALGO_SHA224;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA256:
        return net::ct::DigitallySigned::HASH_ALGO_SHA256;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA384:
        return net::ct::DigitallySigned::HASH_ALGO_SHA384;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA512:
        return net::ct::DigitallySigned::HASH_ALGO_SHA512;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<network::mojom::SignatureAlgorithm,
                  net::ct::DigitallySigned::SignatureAlgorithm> {
  static network::mojom::SignatureAlgorithm ToMojom(
      net::ct::DigitallySigned::SignatureAlgorithm input) {
    switch (input) {
      case net::ct::DigitallySigned::SIG_ALGO_ANONYMOUS:
        return network::mojom::SignatureAlgorithm::SIG_ALGO_ANONYMOUS;
      case net::ct::DigitallySigned::SIG_ALGO_RSA:
        return network::mojom::SignatureAlgorithm::SIG_ALGO_RSA;
      case net::ct::DigitallySigned::SIG_ALGO_DSA:
        return network::mojom::SignatureAlgorithm::SIG_ALGO_DSA;
      case net::ct::DigitallySigned::SIG_ALGO_ECDSA:
        return network::mojom::SignatureAlgorithm::SIG_ALGO_ECDSA;
    }
    NOTREACHED();
  }

  static net::ct::DigitallySigned::SignatureAlgorithm FromMojom(
      network::mojom::SignatureAlgorithm input) {
    switch (input) {
      case network::mojom::SignatureAlgorithm::SIG_ALGO_ANONYMOUS:
        return net::ct::DigitallySigned::SIG_ALGO_ANONYMOUS;
      case network::mojom::SignatureAlgorithm::SIG_ALGO_RSA:
        return net::ct::DigitallySigned::SIG_ALGO_RSA;
      case network::mojom::SignatureAlgorithm::SIG_ALGO_DSA:
        return net::ct::DigitallySigned::SIG_ALGO_DSA;
      case network::mojom::SignatureAlgorithm::SIG_ALGO_ECDSA:
        return net::ct::DigitallySigned::SIG_ALGO_ECDSA;
    }
    NOTREACHED();
  }
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::DigitallySignedDataView,
                 net::ct::DigitallySigned> {
  static net::ct::DigitallySigned::HashAlgorithm hash_algorithm(
      const net::ct::DigitallySigned& obj) {
    return obj.hash_algorithm;
  }
  static net::ct::DigitallySigned::SignatureAlgorithm signature_algorithm(
      const net::ct::DigitallySigned& obj) {
    return obj.signature_algorithm;
  }
  static base::span<const uint8_t> signature(
      const net::ct::DigitallySigned& obj) {
    return base::as_byte_span(obj.signature_data);
  }

  static bool Read(network::mojom::DigitallySignedDataView obj,
                   net::ct::DigitallySigned* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::SCTVersion,
               net::ct::SignedCertificateTimestamp::Version> {
  static network::mojom::SCTVersion ToMojom(
      net::ct::SignedCertificateTimestamp::Version type);
  static net::ct::SignedCertificateTimestamp::Version FromMojom(
      network::mojom::SCTVersion input);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::SCTOrigin,
               net::ct::SignedCertificateTimestamp::Origin> {
  static network::mojom::SCTOrigin ToMojom(
      net::ct::SignedCertificateTimestamp::Origin type);
  static net::ct::SignedCertificateTimestamp::Origin FromMojom(
      network::mojom::SCTOrigin input);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::SignedCertificateTimestampDataView,
                 scoped_refptr<net::ct::SignedCertificateTimestamp>> {
 public:
  static bool IsNull(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return !sct;
  }
  static void SetToNull(
      scoped_refptr<net::ct::SignedCertificateTimestamp>* output) {
    *output = nullptr;
  }
  static net::ct::SignedCertificateTimestamp::Version version(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return sct->version;
  }
  static const std::string& log_id(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return sct->log_id;
  }
  static base::Time timestamp(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return sct->timestamp;
  }
  static base::span<const uint8_t> extensions(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return base::as_byte_span(sct->extensions);
  }
  static const net::ct::DigitallySigned& signature(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return sct->signature;
  }
  static net::ct::SignedCertificateTimestamp::Origin origin(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return sct->origin;
  }
  static const std::string& log_description(
      const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct) {
    return sct->log_description;
  }

  static bool Read(network::mojom::SignedCertificateTimestampDataView data,
                   scoped_refptr<net::ct::SignedCertificateTimestamp>* out);
};

}  // namespace mojo
#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIGNED_CERTIFICATE_TIMESTAMP_MOJOM_TRAITS_H_
