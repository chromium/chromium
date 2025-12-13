// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CONFIG_H_
#define NET_SSL_SSL_CONFIG_H_

#include <stdint.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/socket/next_proto.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {

// Supported TLS ProtocolVersion values encoded as uint16_t.
enum {
  SSL_PROTOCOL_VERSION_TLS1_2 = 0x0303,
  SSL_PROTOCOL_VERSION_TLS1_3 = 0x0304,
};

// Default minimum protocol version.
NET_EXPORT extern const uint16_t kDefaultSSLVersionMin;

// Default maximum protocol version.
NET_EXPORT extern const uint16_t kDefaultSSLVersionMax;

// A collection of SSL-related configuration settings.
struct NET_EXPORT SSLConfig {
  using ApplicationSettings = base::flat_map<NextProto, std::vector<uint8_t>>;

  SSLConfig();
  SSLConfig(const SSLConfig& other);
  SSLConfig(SSLConfig&& other);
  ~SSLConfig();
  SSLConfig& operator=(const SSLConfig&);
  SSLConfig& operator=(SSLConfig&&);

  // Returns true if |cert| is one of the certs in |allowed_bad_certs|.
  // The expected cert status is written to |cert_status|. |*cert_status| can
  // be NULL if user doesn't care about the cert status.
  bool IsAllowedBadCert(X509Certificate* cert, CertStatus* cert_status) const;

  // Returns the set of flags to use for certificate verification, which is a
  // bitwise OR of CertVerifier::VerifyFlags that represent this SSLConfig's
  // configuration.
  int GetCertVerifyFlags() const;

  // If specified, the minimum and maximum protocol versions that are enabled.
  // (Use the SSL_PROTOCOL_VERSION_xxx enumerators defined above.) If
  // unspecified, values from the SSLConfigService are used.
  std::optional<uint16_t> version_min_override;
  std::optional<uint16_t> version_max_override;

  // Whether early data is enabled on this connection. Note that early data has
  // weaker security properties than normal data and changes the
  // SSLClientSocket's behavior. The caller must only send replayable data prior
  // to handshake confirmation. See StreamSocket::ConfirmHandshake for details.
  //
  // Additionally, early data may be rejected by the server, resulting in some
  // socket operation failing with ERR_EARLY_DATA_REJECTED or
  // ERR_WRONG_VERSION_ON_EARLY_DATA before any data is returned from the
  // server. The caller must handle these cases, typically by retrying the
  // high-level operation.
  //
  // If unsure, do not enable this option.
  bool early_data_enabled = false;

  // If true, causes only ECDHE cipher suites to be enabled.
  bool require_ecdhe = false;

  // TODO(wtc): move the following members to a new SSLParams structure.  They
  // are not SSL configuration settings.

  struct NET_EXPORT CertAndStatus {
    CertAndStatus();
    CertAndStatus(scoped_refptr<X509Certificate> cert, CertStatus status);
    CertAndStatus(const CertAndStatus&);
    ~CertAndStatus();

    scoped_refptr<X509Certificate> cert;
    CertStatus cert_status = 0;
  };

  // Add any known-bad SSL certificate (with its cert status) to
  // |allowed_bad_certs| that should not trigger an ERR_CERT_* error when
  // calling SSLClientSocket::Connect.  This would normally be done in
  // response to the user explicitly accepting the bad certificate.
  std::vector<CertAndStatus> allowed_bad_certs;

  // True if all certificate errors should be ignored.
  bool ignore_certificate_errors = false;

  // True if, for a single connection, any dependent network fetches should
  // be disabled. This can be used to avoid triggering re-entrancy in the
  // network layer. For example, fetching a PAC script over HTTPS may cause
  // AIA, OCSP, or CRL fetches to block on retrieving the PAC script, while
  // the PAC script fetch is waiting for those dependent fetches, creating a
  // deadlock.
  bool disable_cert_verification_network_fetches = false;

  // The list of application level protocols supported with ALPN (Application
  // Layer Protocol Negotiation), in decreasing order of preference.  Protocols
  // will be advertised in this order during TLS handshake.
  NextProtoVector alpn_protos;

  // True if renegotiation should be allowed for the default application-level
  // protocol when the peer does not negotiate ALPN.
  bool renego_allowed_default = false;

  // The list of application-level protocols to enable renegotiation for.
  NextProtoVector renego_allowed_for_protos;

  // ALPS data for each supported protocol in |alpn_protos|. Specifying a
  // protocol in this map offers ALPS for that protocol and uses the
  // corresponding value as the client settings string. The value may be empty.
  // Keys which do not appear in |alpn_protos| are ignored.
  ApplicationSettings application_settings;

  // If the PartitionConnectionsByNetworkIsolationKey feature is enabled, the
  // session cache is partitioned by this value.
  NetworkAnonymizationKey network_anonymization_key;

  // If non-empty, a serialized ECHConfigList to use to encrypt the ClientHello.
  // If this field is non-empty, callers should handle |ERR_ECH_NOT_NEGOTIATED|
  // errors from Connect() by calling GetECHRetryConfigs() to determine how to
  // retry the connection.
  std::vector<uint8_t> ech_config_list;

  // An additional boolean to partition the session cache by.
  //
  // TODO(https://crbug.com/775438, https://crbug.com/951205): This should
  // additionally disable client certificates, once client certificate handling
  // is moved into SSLClientContext. With client certificates are disabled, the
  // current session cache partitioning behavior will be needed to correctly
  // implement it. For now, it acts as an incomplete version of
  // PartitionConnectionsByNetworkIsolationKey.
  PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;

  // True if the post-handshake peeking of the transport should be skipped. This
  // logic ensures tickets are resolved early, but can interfere with some unit
  // tests.
  bool disable_post_handshake_peek_for_testing = false;

  // The proxy chain involving this SSL session, and the session's position
  // within that chain. If the session is to the destination, or (for QUIC) the
  // proxy chain only includes a prefix of the proxies, then `proxy_chain_index`
  // may be equal to `proxy_chain.length()`.
  ProxyChain proxy_chain = ProxyChain::Direct();
  size_t proxy_chain_index = 0;

  // The usage of this session. This supports distinguishing connections to a
  // proxy as an endpoint from connections to that same proxy as a proxy.
  SessionUsage session_usage = SessionUsage::kDestination;

  // If not nullopt, a list of TLS Trust Anchor IDs in wire format
  // (https://tlswg.org/tls-trust-anchor-ids/draft-ietf-tls-trust-anchor-ids.html#name-tls-extension),
  // i.e. a series of non-empty, 8-bit length-prefixed strings. These trust
  // anchor IDs will be sent on the TLS ClientHello message to help the server
  // select a certificate that the client will accept.
  //
  // If an empty vector, the Trust Anchor IDs extension will be sent, but with
  // no trust anchors. This will signal to the server that the client is new
  // enough to implement the extension and can process the server's list of
  // available trust anchors.
  std::optional<std::vector<uint8_t>> trust_anchor_ids;
};

}  // namespace net

#endif  // NET_SSL_SSL_CONFIG_H_
