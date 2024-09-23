// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DIGITALLY_SIGNED_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DIGITALLY_SIGNED_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "services/network/public/mojom/digitally_signed.mojom.h"

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
    NOTREACHED_IN_MIGRATION();
    return network::mojom::HashAlgorithm::HASH_ALGO_NONE;
  }

  static bool FromMojom(network::mojom::HashAlgorithm input,
                        net::ct::DigitallySigned::HashAlgorithm* output) {
    switch (input) {
      case network::mojom::HashAlgorithm::HASH_ALGO_NONE:
        *output = net::ct::DigitallySigned::HASH_ALGO_NONE;
        return true;
      case network::mojom::HashAlgorithm::HASH_ALGO_MD5:
        *output = net::ct::DigitallySigned::HASH_ALGO_MD5;
        return true;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA1:
        *output = net::ct::DigitallySigned::HASH_ALGO_SHA1;
        return true;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA224:
        *output = net::ct::DigitallySigned::HASH_ALGO_SHA224;
        return true;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA256:
        *output = net::ct::DigitallySigned::HASH_ALGO_SHA256;
        return true;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA384:
        *output = net::ct::DigitallySigned::HASH_ALGO_SHA384;
        return true;
      case network::mojom::HashAlgorithm::HASH_ALGO_SHA512:
        *output = net::ct::DigitallySigned::HASH_ALGO_SHA512;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
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
    NOTREACHED_IN_MIGRATION();
    return network::mojom::SignatureAlgorithm::SIG_ALGO_ANONYMOUS;
  }

  static bool FromMojom(network::mojom::SignatureAlgorithm input,
                        net::ct::DigitallySigned::SignatureAlgorithm* output) {
    switch (input) {
      case network::mojom::SignatureAlgorithm::SIG_ALGO_ANONYMOUS:
        *output = net::ct::DigitallySigned::SIG_ALGO_ANONYMOUS;
        return true;
      case network::mojom::SignatureAlgorithm::SIG_ALGO_RSA:
        *output = net::ct::DigitallySigned::SIG_ALGO_RSA;
        return true;
      case network::mojom::SignatureAlgorithm::SIG_ALGO_DSA:
        *output = net::ct::DigitallySigned::SIG_ALGO_DSA;
        return true;
      case network::mojom::SignatureAlgorithm::SIG_ALGO_ECDSA:
        *output = net::ct::DigitallySigned::SIG_ALGO_ECDSA;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct StructTraits<network::mojom::DigitallySignedDataView,
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
    return base::as_bytes(base::make_span(obj.signature_data));
  }

  static bool Read(network::mojom::DigitallySignedDataView obj,
                   net::ct::DigitallySigned* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DIGITALLY_SIGNED_MOJOM_TRAITS_H_
