// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_INFO_H_
#define NET_SSL_SSL_INFO_H_

#include "base/memory/scoped_refptr.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "third_party/boringssl/src/pki/ocsp_verify_result.h"

namespace net {

class X509Certificate;

// SSL connection info.
// This is really a struct.  All members are public.
class NET_EXPORT SSLInfo {
 public:
  // HandshakeType enumerates the possible resumption cases after an SSL
  // handshake.
  enum HandshakeType {
    HANDSHAKE_UNKNOWN = 0,
    HANDSHAKE_RESUME,  // we resumed a previous session.
    HANDSHAKE_FULL,  // we negotiated a new session.
  };

  SSLInfo();
  SSLInfo(const SSLInfo& info);
  ~SSLInfo();
  SSLInfo& operator=(const SSLInfo& info);

  void Reset();

  bool is_valid() const { return cert.get() != nullptr; }

  // The SSL certificate.
  scoped_refptr<X509Certificate> cert;

  // The SSL certificate as received by the client. Can be different
  // from |cert|, which is the chain as built by the client during
  // validation.
  scoped_refptr<X509Certificate> unverified_cert;

  // Bitmask of status info of |cert|, representing, for example, known errors
  // and extended validation (EV) status.
  // See cert_status_flags.h for values.
  CertStatus cert_status = 0;

  // The ID of the (EC)DH group used by the key exchange or zero if unknown
  // (older cache entries may not store the value) or not applicable.
  uint16_t key_exchange_group = 0;

  // The signature algorithm used by the peer in the TLS handshake, as defined
  // by the TLS SignatureScheme registry
  // (https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-signaturescheme).
  // These correspond to |SSL_SIGN_*| constants in BoringSSL. The value is zero
  // if unknown (older cache entries may not store the value) or not applicable.
  uint16_t peer_signature_algorithm = 0;

  // Information about the SSL connection itself. See
  // ssl_connection_status_flags.h for values. The protocol version,
  // ciphersuite, and compression in use are encoded within.
  int connection_status = 0;

  // If the certificate is valid, then this is true iff it was rooted at a
  // standard CA root. (As opposed to a user-installed root.)
  bool is_issued_by_known_root = false;

  // True if pinning was bypassed on this connection.
  bool pkp_bypassed = false;

  // True if a client certificate was sent to the server.  Note that sending
  // a Certificate message with no client certificate in it does not count.
  bool client_cert_sent = false;

  // True if data was received over early data on the server. This field is only
  // set for server sockets.
  bool early_data_received = false;

  // True if the connection negotiated the Encrypted ClientHello extension.
  bool encrypted_client_hello = false;

  HandshakeType handshake_type = HANDSHAKE_UNKNOWN;

  // The hashes, in several algorithms, of the SubjectPublicKeyInfos from
  // each certificate in the chain.
  HashValueVector public_key_hashes;

  // List of SignedCertificateTimestamps and their corresponding validation
  // status.
  SignedCertificateTimestampAndStatusList signed_certificate_timestamps;

  // Whether the connection complied with the CT cert policy, and if
  // not, why not.
  ct::CTPolicyCompliance ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE;

  // OCSP stapling details.
  bssl::OCSPVerifyResult ocsp_result;

  // True if there was a certificate error which should be treated as fatal,
  // and false otherwise.
  bool is_fatal_cert_error = false;
};

}  // namespace net

#endif  // NET_SSL_SSL_INFO_H_
