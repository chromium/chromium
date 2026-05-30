// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ECH_MODE_H_
#define NET_BASE_ECH_MODE_H_

namespace net {

// Specifies the Encrypted Client Hello (ECH) behavior for a TLS connection.
// See https://datatracker.ietf.org/doc/rfc9849/
//
// These options also map to Android's domain encryption modes (see
// https://developer.android.com/privacy-and-security/security-config#EncryptedClientHelloSummary
// and
// https://developer.android.com/reference/kotlin/android/security/NetworkSecurityPolicy#getdomainencryptionmode).
// Modification of these behaviors must be done with extreme care to ensure
// //net embedders can respect platform security policies.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum class EchMode {
  // ECH is explicitly disabled.
  // The client will not attempt ECH and will not send GREASE ECH.
  //
  // The encrypted_client_hello extension is never sent in ClientHello.
  // Outer SNI is never modified.
  kDisabled = 0,

  // Best-effort ECH based on RFC 9849.
  // The client will attempt ECH if possible.
  //  - The client attempts ECH if an ECH configuration is available.
  //  - If no configuration is available, the client will send a GREASE ECH.
  //  - If the server rejects the ECH, the client will fall back to the
  //    ClientHelloOuter handshake to retrieve the retry_configs.
  //  - If no supported configurations are provided during fallback, the client
  //    retries the connection with a GREASE ECH extension.
  //
  // The encrypted_client_hello extension is always sent.
  // Outer SNI is not modified when GREASE ECH is sent.
  //
  // NOTE: RFC 9849 Section 6.1.6 states that if no supported
  // retry_configs are provided, the client SHOULD retry with ECH completely
  // disabled. This implementation deliberately deviates by sending a GREASE
  // ECH on the retry to maximize protocol ossification resistance.
  kOpportunistic = 1,

  // ECH is strictly enforced.
  // The client strictly requires a successful ECH connection.
  // It will abort the connection if:
  //  - No valid ECH configuration is available initially.
  //  - The server rejects ECH, and the client fails to establish a new ECH
  //    connection after retrying with the provided retry_configs (or if no
  //    valid retry_configs were provided by the server).
  //
  // The encrypted_client_hello extension is always sent.
  // Outer SNI is always modified to the public_name.
  kStrict = 2,
};

}  // namespace net

#endif  // NET_BASE_ECH_MODE_H_
