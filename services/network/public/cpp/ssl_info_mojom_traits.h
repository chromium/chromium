// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SSL_INFO_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SSL_INFO_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/hash_value.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/mojom/ssl_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::SSLInfoHandshakeType,
               net::SSLInfo::HandshakeType> {
  static network::mojom::SSLInfoHandshakeType ToMojom(
      net::SSLInfo::HandshakeType type);
  static net::SSLInfo::HandshakeType FromMojom(
      network::mojom::SSLInfoHandshakeType input);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::SSLInfoDataView, net::SSLInfo> {
 public:
  static const scoped_refptr<net::X509Certificate>& cert(
      const net::SSLInfo& info) {
    return info.cert;
  }
  static const scoped_refptr<net::X509Certificate>& unverified_cert(
      const net::SSLInfo& info) {
    return info.unverified_cert;
  }
  static uint32_t cert_status(const net::SSLInfo& info) {
    return info.cert_status;
  }
  static uint16_t key_exchange_group(const net::SSLInfo& info) {
    return info.key_exchange_group;
  }
  static uint16_t peer_signature_algorithm(const net::SSLInfo& info) {
    return info.peer_signature_algorithm;
  }
  static int32_t connection_status(const net::SSLInfo& info) {
    return info.connection_status;
  }
  static bool is_issued_by_known_root(const net::SSLInfo& info) {
    return info.is_issued_by_known_root;
  }
  static bool pkp_bypassed(const net::SSLInfo& info) {
    return info.pkp_bypassed;
  }
  static bool client_cert_sent(const net::SSLInfo& info) {
    return info.client_cert_sent;
  }
  static bool encrypted_client_hello(const net::SSLInfo& info) {
    return info.encrypted_client_hello;
  }
  static net::SSLInfo::HandshakeType handshake_type(const net::SSLInfo& info) {
    return info.handshake_type;
  }
  static const std::vector<net::SHA256HashValue>& public_key_hashes(
      const net::SSLInfo& info) {
    return info.public_key_hashes;
  }
  static const net::SignedCertificateTimestampAndStatusList&
  signed_certificate_timestamps(const net::SSLInfo& info) {
    return info.signed_certificate_timestamps;
  }
  static net::ct::CTPolicyCompliance ct_policy_compliance(
      const net::SSLInfo& info) {
    return info.ct_policy_compliance;
  }
  static bool is_fatal_cert_error(const net::SSLInfo& info) {
    return info.is_fatal_cert_error;
  }

  static bool Read(network::mojom::SSLInfoDataView data, net::SSLInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SSL_INFO_MOJOM_TRAITS_H_
