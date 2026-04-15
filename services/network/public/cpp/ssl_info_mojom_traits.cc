// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ssl_info_mojom_traits.h"

#include "base/notreached.h"
#include "services/network/public/cpp/ct_policy_status_mojom_traits.h"
#include "services/network/public/cpp/hash_value_mojom_traits.h"
#include "services/network/public/cpp/signed_certificate_timestamp_and_status_mojom_traits.h"
#include "services/network/public/cpp/x509_certificate_mojom_traits.h"

namespace mojo {

// static
network::mojom::SSLInfoHandshakeType EnumTraits<
    network::mojom::SSLInfoHandshakeType,
    net::SSLInfo::HandshakeType>::ToMojom(net::SSLInfo::HandshakeType type) {
  switch (type) {
    case net::SSLInfo::HANDSHAKE_UNKNOWN:
      return network::mojom::SSLInfoHandshakeType::kUnknown;
    case net::SSLInfo::HANDSHAKE_RESUME:
      return network::mojom::SSLInfoHandshakeType::kResume;
    case net::SSLInfo::HANDSHAKE_FULL:
      return network::mojom::SSLInfoHandshakeType::kFull;
  }
  NOTREACHED();
}

// static
net::SSLInfo::HandshakeType
EnumTraits<network::mojom::SSLInfoHandshakeType, net::SSLInfo::HandshakeType>::
    FromMojom(network::mojom::SSLInfoHandshakeType input) {
  switch (input) {
    case network::mojom::SSLInfoHandshakeType::kUnknown:
      return net::SSLInfo::HANDSHAKE_UNKNOWN;
    case network::mojom::SSLInfoHandshakeType::kResume:
      return net::SSLInfo::HANDSHAKE_RESUME;
    case network::mojom::SSLInfoHandshakeType::kFull:
      return net::SSLInfo::HANDSHAKE_FULL;
  }
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::SSLInfoDataView, net::SSLInfo>::Read(
    network::mojom::SSLInfoDataView data,
    net::SSLInfo* out) {
  out->cert_status = data.cert_status();
  out->key_exchange_group = data.key_exchange_group();
  out->peer_signature_algorithm = data.peer_signature_algorithm();
  out->connection_status = data.connection_status();
  out->is_issued_by_known_root = data.is_issued_by_known_root();
  out->pkp_bypassed = data.pkp_bypassed();
  out->client_cert_sent = data.client_cert_sent();
  out->encrypted_client_hello = data.encrypted_client_hello();
  out->is_fatal_cert_error = data.is_fatal_cert_error();
  return data.ReadCert(&out->cert) &&
         data.ReadUnverifiedCert(&out->unverified_cert) &&
         data.ReadPublicKeyHashes(&out->public_key_hashes) &&
         data.ReadSignedCertificateTimestamps(
             &out->signed_certificate_timestamps) &&
         data.ReadHandshakeType(&out->handshake_type) &&
         data.ReadCtPolicyCompliance(&out->ct_policy_compliance);
}

}  // namespace mojo
