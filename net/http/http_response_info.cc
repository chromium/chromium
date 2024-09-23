// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include <optional>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using base::Time;

namespace net {

namespace {

bool KeyExchangeGroupIsValid(int ssl_connection_status) {
  // TLS 1.3 and later always treat the field correctly.
  if (SSLConnectionStatusToVersion(ssl_connection_status) >=
      SSL_CONNECTION_VERSION_TLS1_3) {
    return true;
  }

  // Prior to TLS 1.3, only ECDHE ciphers have groups.
  const SSL_CIPHER* cipher = SSL_get_cipher_by_value(
      SSLConnectionStatusToCipherSuite(ssl_connection_status));
  return cipher && SSL_CIPHER_get_kx_nid(cipher) == NID_kx_ecdhe;
}

}  // namespace

// These values can be bit-wise combined to form the flags field of the
// serialized HttpResponseInfo.
enum {
  // The version of the response info used when persisting response info.
  RESPONSE_INFO_VERSION = 3,

  // The minimum version supported for deserializing response info.
  RESPONSE_INFO_MINIMUM_VERSION = 3,

  // We reserve up to 8 bits for the version number.
  RESPONSE_INFO_VERSION_MASK = 0xFF,

  // This bit is set if the response info has a cert at the end.
  // Version 1 serialized only the end-entity certificate, while subsequent
  // versions include the available certificate chain.
  RESPONSE_INFO_HAS_CERT = 1 << 8,

  // This bit was historically set if the response info had a security-bits
  // field (security strength, in bits, of the SSL connection) at the end.
  RESPONSE_INFO_HAS_SECURITY_BITS = 1 << 9,

  // This bit is set if the response info has a cert status at the end.
  RESPONSE_INFO_HAS_CERT_STATUS = 1 << 10,

  // This bit is set if the response info has vary header data.
  RESPONSE_INFO_HAS_VARY_DATA = 1 << 11,

  // This bit is set if the request was cancelled before completion.
  RESPONSE_INFO_TRUNCATED = 1 << 12,

  // This bit is set if the response was received via SPDY.
  RESPONSE_INFO_WAS_SPDY = 1 << 13,

  // This bit is set if the request has ALPN negotiated.
  RESPONSE_INFO_WAS_ALPN = 1 << 14,

  // This bit is set if the request was fetched via an explicit proxy.
  // This bit is deprecated.
  RESPONSE_INFO_WAS_PROXY = 1 << 15,

  // This bit is set if the response info has an SSL connection status field.
  // This contains the ciphersuite used to fetch the resource as well as the
  // protocol version, compression method and whether SSLv3 fallback was used.
  RESPONSE_INFO_HAS_SSL_CONNECTION_STATUS = 1 << 16,

  // This bit is set if the response info has protocol version.
  RESPONSE_INFO_HAS_ALPN_NEGOTIATED_PROTOCOL = 1 << 17,

  // This bit is set if the response info has connection info.
  RESPONSE_INFO_HAS_CONNECTION_INFO = 1 << 18,

  // This bit is set if the request has http authentication.
  RESPONSE_INFO_USE_HTTP_AUTHENTICATION = 1 << 19,

  // This bit is set if ssl_info has SCTs.
  RESPONSE_INFO_HAS_SIGNED_CERTIFICATE_TIMESTAMPS = 1 << 20,

  RESPONSE_INFO_UNUSED_SINCE_PREFETCH = 1 << 21,

  // This bit is set if the response has a key exchange group.
  RESPONSE_INFO_HAS_KEY_EXCHANGE_GROUP = 1 << 22,

  // This bit is set if ssl_info recorded that PKP was bypassed due to a local
  // trust anchor.
  RESPONSE_INFO_PKP_BYPASSED = 1 << 23,

  // This bit is set if stale_revalidate_time is stored.
  RESPONSE_INFO_HAS_STALENESS = 1 << 24,

  // This bit is set if the response has a peer signature algorithm.
  RESPONSE_INFO_HAS_PEER_SIGNATURE_ALGORITHM = 1 << 25,

  // This bit is set if the response is a prefetch whose reuse should be
  // restricted in some way.
  RESPONSE_INFO_RESTRICTED_PREFETCH = 1 << 26,

  // This bit is set if the response has a nonempty `dns_aliases` entry.
  RESPONSE_INFO_HAS_DNS_ALIASES = 1 << 27,

  // This bit is now unused. It may be set on existing entries. Previously it
  // was set for an entry in the single-keyed cache that had been marked
  // unusable due to the cache transparency checksum not matching.
  RESPONSE_INFO_UNUSED_WAS_SINGLE_KEYED_CACHE_ENTRY_UNUSABLE = 1 << 28,

  // This bit is set if the response has `encrypted_client_hello` set.
  RESPONSE_INFO_ENCRYPTED_CLIENT_HELLO = 1 << 29,

  // This bit is set if the response has `browser_run_id` set.
  RESPONSE_INFO_BROWSER_RUN_ID = 1 << 30,

  // This bit is set if the response has extra bit set.
  RESPONSE_INFO_HAS_EXTRA_FLAGS = 1 << 31,
};

// These values can be bit-wise combined to form the extra flags field of the
// serialized HttpResponseInfo.
enum {
  // This bit is set if the request usd a shared dictionary for decoding its
  // body.
  RESPONSE_EXTRA_INFO_DID_USE_SHARED_DICTIONARY = 1,

  // This bit is set if the response has valid `proxy_chain`.
  RESPONSE_EXTRA_INFO_HAS_PROXY_CHAIN = 1 << 1,
};

HttpResponseInfo::HttpResponseInfo() = default;

HttpResponseInfo::HttpResponseInfo(const HttpResponseInfo& rhs) = default;

HttpResponseInfo::~HttpResponseInfo() = default;

HttpResponseInfo& HttpResponseInfo::operator=(const HttpResponseInfo& rhs) =
    default;

bool HttpResponseInfo::InitFromPickle(const base::Pickle& pickle,
                                      bool* response_truncated) {
  base::PickleIterator iter(pickle);

  // Read flags and verify version
  int flags;
  int extra_flags = 0;
  if (!iter.ReadInt(&flags))
    return false;
  if (flags & RESPONSE_INFO_HAS_EXTRA_FLAGS) {
    if (!iter.ReadInt(&extra_flags)) {
      return false;
    }
  }
  int version = flags & RESPONSE_INFO_VERSION_MASK;
  if (version < RESPONSE_INFO_MINIMUM_VERSION ||
      version > RESPONSE_INFO_VERSION) {
    DLOG(ERROR) << "unexpected response info version: " << version;
    return false;
  }

  // Read request-time
  int64_t time_val;
  if (!iter.ReadInt64(&time_val))
    return false;
  request_time = Time::FromInternalValue(time_val);
  was_cached = true;  // Set status to show cache resurrection.

  // Read response-time
  if (!iter.ReadInt64(&time_val))
    return false;
  response_time = Time::FromInternalValue(time_val);

  // Read response-headers
  headers = base::MakeRefCounted<HttpResponseHeaders>(&iter);
  if (headers->response_code() == -1)
    return false;

  // Read ssl-info
  if (flags & RESPONSE_INFO_HAS_CERT) {
    ssl_info.cert = X509Certificate::CreateFromPickle(&iter);
    if (!ssl_info.cert.get())
      return false;
  }
  if (flags & RESPONSE_INFO_HAS_CERT_STATUS) {
    CertStatus cert_status;
    if (!iter.ReadUInt32(&cert_status))
      return false;
    ssl_info.cert_status = cert_status;
  }
  if (flags & RESPONSE_INFO_HAS_SECURITY_BITS) {
    // The security_bits field has been removed from ssl_info. For backwards
    // compatibility, we should still read the value out of iter.
    int security_bits;
    if (!iter.ReadInt(&security_bits))
      return false;
  }

  if (flags & RESPONSE_INFO_HAS_SSL_CONNECTION_STATUS) {
    int connection_status;
    if (!iter.ReadInt(&connection_status))
      return false;

    // SSLv3 is gone, so drop cached entries that were loaded over SSLv3.
    if (SSLConnectionStatusToVersion(connection_status) ==
        SSL_CONNECTION_VERSION_SSL3) {
      return false;
    }
    ssl_info.connection_status = connection_status;
  }

  // Signed Certificate Timestamps are no longer persisted to the cache, so
  // ignore them when reading them out.
  if (flags & RESPONSE_INFO_HAS_SIGNED_CERTIFICATE_TIMESTAMPS) {
    int num_scts;
    if (!iter.ReadInt(&num_scts))
      return false;
    for (int i = 0; i < num_scts; ++i) {
      scoped_refptr<ct::SignedCertificateTimestamp> sct(
          ct::SignedCertificateTimestamp::CreateFromPickle(&iter));
      uint16_t status;
      if (!sct.get() || !iter.ReadUInt16(&status))
        return false;
    }
  }

  // Read vary-data
  if (flags & RESPONSE_INFO_HAS_VARY_DATA) {
    if (!vary_data.InitFromPickle(&iter))
      return false;
  }

  // Read socket_address.
  std::string socket_address_host;
  if (!iter.ReadString(&socket_address_host))
    return false;
  // If the host was written, we always expect the port to follow.
  uint16_t socket_address_port;
  if (!iter.ReadUInt16(&socket_address_port))
    return false;

  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(socket_address_host)) {
    remote_endpoint = IPEndPoint(ip_address, socket_address_port);
  } else if (ParseURLHostnameToAddress(socket_address_host, &ip_address)) {
    remote_endpoint = IPEndPoint(ip_address, socket_address_port);
  }

  // Read protocol-version.
  if (flags & RESPONSE_INFO_HAS_ALPN_NEGOTIATED_PROTOCOL) {
    if (!iter.ReadString(&alpn_negotiated_protocol))
      return false;
  }

  // Read connection info.
  if (flags & RESPONSE_INFO_HAS_CONNECTION_INFO) {
    int value;
    if (!iter.ReadInt(&value))
      return false;

    if (value > static_cast<int>(HttpConnectionInfo::kUNKNOWN) &&
        value <= static_cast<int>(HttpConnectionInfo::kMaxValue)) {
      connection_info = static_cast<HttpConnectionInfo>(value);
    }
  }

  // Read key_exchange_group
  if (flags & RESPONSE_INFO_HAS_KEY_EXCHANGE_GROUP) {
    int key_exchange_group;
    if (!iter.ReadInt(&key_exchange_group))
      return false;

    // Historically, the key_exchange_group field was key_exchange_info which
    // conflated a number of different values based on the cipher suite, so some
    // values must be discarded. See https://crbug.com/639421.
    if (KeyExchangeGroupIsValid(ssl_info.connection_status))
      ssl_info.key_exchange_group = key_exchange_group;
  }

  // Read staleness time.
  if (flags & RESPONSE_INFO_HAS_STALENESS) {
    if (!iter.ReadInt64(&time_val))
      return false;
    stale_revalidate_timeout = base::Time() + base::Microseconds(time_val);
  }

  was_fetched_via_spdy = (flags & RESPONSE_INFO_WAS_SPDY) != 0;

  was_alpn_negotiated = (flags & RESPONSE_INFO_WAS_ALPN) != 0;

  *response_truncated = (flags & RESPONSE_INFO_TRUNCATED) != 0;

  did_use_http_auth = (flags & RESPONSE_INFO_USE_HTTP_AUTHENTICATION) != 0;

  unused_since_prefetch = (flags & RESPONSE_INFO_UNUSED_SINCE_PREFETCH) != 0;

  restricted_prefetch = (flags & RESPONSE_INFO_RESTRICTED_PREFETCH) != 0;

  // RESPONSE_INFO_UNUSED_WAS_SINGLE_KEYED_CACHE_ENTRY_UNUSABLE is unused.

  ssl_info.pkp_bypassed = (flags & RESPONSE_INFO_PKP_BYPASSED) != 0;

  // Read peer_signature_algorithm.
  if (flags & RESPONSE_INFO_HAS_PEER_SIGNATURE_ALGORITHM) {
    int peer_signature_algorithm;
    if (!iter.ReadInt(&peer_signature_algorithm) ||
        !base::IsValueInRangeForNumericType<uint16_t>(
            peer_signature_algorithm)) {
      return false;
    }
    ssl_info.peer_signature_algorithm =
        base::checked_cast<uint16_t>(peer_signature_algorithm);
  }

  // Read DNS aliases.
  if (flags & RESPONSE_INFO_HAS_DNS_ALIASES) {
    int num_aliases;
    if (!iter.ReadInt(&num_aliases))
      return false;

    std::string alias;
    for (int i = 0; i < num_aliases; i++) {
      if (!iter.ReadString(&alias))
        return false;
      dns_aliases.insert(alias);
    }
  }

  ssl_info.encrypted_client_hello =
      (flags & RESPONSE_INFO_ENCRYPTED_CLIENT_HELLO) != 0;

  // Read browser_run_id.
  if (flags & RESPONSE_INFO_BROWSER_RUN_ID) {
    int64_t id;
    if (!iter.ReadInt64(&id))
      return false;
    browser_run_id = std::make_optional(id);
  }

  did_use_shared_dictionary =
      (extra_flags & RESPONSE_EXTRA_INFO_DID_USE_SHARED_DICTIONARY) != 0;

  if (extra_flags & RESPONSE_EXTRA_INFO_HAS_PROXY_CHAIN) {
    if (!proxy_chain.InitFromPickle(&iter)) {
      return false;
    }
  }

  return true;
}

void HttpResponseInfo::Persist(base::Pickle* pickle,
                               bool skip_transient_headers,
                               bool response_truncated) const {
  int flags = RESPONSE_INFO_VERSION;
  int extra_flags = 0;
  if (ssl_info.is_valid()) {
    flags |= RESPONSE_INFO_HAS_CERT;
    flags |= RESPONSE_INFO_HAS_CERT_STATUS;
    if (ssl_info.key_exchange_group != 0)
      flags |= RESPONSE_INFO_HAS_KEY_EXCHANGE_GROUP;
    if (ssl_info.connection_status != 0)
      flags |= RESPONSE_INFO_HAS_SSL_CONNECTION_STATUS;
    if (ssl_info.peer_signature_algorithm != 0)
      flags |= RESPONSE_INFO_HAS_PEER_SIGNATURE_ALGORITHM;
  }
  if (vary_data.is_valid())
    flags |= RESPONSE_INFO_HAS_VARY_DATA;
  if (response_truncated)
    flags |= RESPONSE_INFO_TRUNCATED;
  if (was_fetched_via_spdy)
    flags |= RESPONSE_INFO_WAS_SPDY;
  if (was_alpn_negotiated) {
    flags |= RESPONSE_INFO_WAS_ALPN;
    flags |= RESPONSE_INFO_HAS_ALPN_NEGOTIATED_PROTOCOL;
  }
  if (connection_info != HttpConnectionInfo::kUNKNOWN) {
    flags |= RESPONSE_INFO_HAS_CONNECTION_INFO;
  }
  if (did_use_http_auth)
    flags |= RESPONSE_INFO_USE_HTTP_AUTHENTICATION;
  if (unused_since_prefetch)
    flags |= RESPONSE_INFO_UNUSED_SINCE_PREFETCH;
  if (restricted_prefetch)
    flags |= RESPONSE_INFO_RESTRICTED_PREFETCH;
  // RESPONSE_INFO_UNUSED_WAS_SINGLE_KEYED_CACHE_ENTRY_UNUSABLE is not used.
  if (ssl_info.pkp_bypassed)
    flags |= RESPONSE_INFO_PKP_BYPASSED;
  if (!stale_revalidate_timeout.is_null())
    flags |= RESPONSE_INFO_HAS_STALENESS;
  if (!dns_aliases.empty())
    flags |= RESPONSE_INFO_HAS_DNS_ALIASES;
  if (ssl_info.encrypted_client_hello)
    flags |= RESPONSE_INFO_ENCRYPTED_CLIENT_HELLO;
  if (browser_run_id.has_value())
    flags |= RESPONSE_INFO_BROWSER_RUN_ID;

  if (did_use_shared_dictionary) {
    extra_flags |= RESPONSE_EXTRA_INFO_DID_USE_SHARED_DICTIONARY;
  }

  if (proxy_chain.IsValid()) {
    extra_flags |= RESPONSE_EXTRA_INFO_HAS_PROXY_CHAIN;
  }

  if (extra_flags) {
    flags |= RESPONSE_INFO_HAS_EXTRA_FLAGS;
  }

  pickle->WriteInt(flags);
  if (extra_flags) {
    pickle->WriteInt(extra_flags);
  }
  pickle->WriteInt64(request_time.ToInternalValue());
  pickle->WriteInt64(response_time.ToInternalValue());

  HttpResponseHeaders::PersistOptions persist_options =
      HttpResponseHeaders::PERSIST_RAW;

  if (skip_transient_headers) {
    persist_options = HttpResponseHeaders::PERSIST_SANS_COOKIES |
                      HttpResponseHeaders::PERSIST_SANS_CHALLENGES |
                      HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP |
                      HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE |
                      HttpResponseHeaders::PERSIST_SANS_RANGES |
                      HttpResponseHeaders::PERSIST_SANS_SECURITY_STATE;
  }

  headers->Persist(pickle, persist_options);

  if (ssl_info.is_valid()) {
    ssl_info.cert->Persist(pickle);
    pickle->WriteUInt32(ssl_info.cert_status);
    if (ssl_info.connection_status != 0)
      pickle->WriteInt(ssl_info.connection_status);
  }

  if (vary_data.is_valid())
    vary_data.Persist(pickle);

  pickle->WriteString(remote_endpoint.ToStringWithoutPort());
  pickle->WriteUInt16(remote_endpoint.port());

  if (was_alpn_negotiated)
    pickle->WriteString(alpn_negotiated_protocol);

  if (connection_info != HttpConnectionInfo::kUNKNOWN) {
    pickle->WriteInt(static_cast<int>(connection_info));
  }

  if (ssl_info.is_valid() && ssl_info.key_exchange_group != 0)
    pickle->WriteInt(ssl_info.key_exchange_group);

  if (flags & RESPONSE_INFO_HAS_STALENESS) {
    pickle->WriteInt64(
        (stale_revalidate_timeout - base::Time()).InMicroseconds());
  }

  if (ssl_info.is_valid() && ssl_info.peer_signature_algorithm != 0)
    pickle->WriteInt(ssl_info.peer_signature_algorithm);

  if (!dns_aliases.empty()) {
    pickle->WriteInt(dns_aliases.size());
    for (const auto& alias : dns_aliases)
      pickle->WriteString(alias);
  }

  if (browser_run_id.has_value()) {
    pickle->WriteInt64(browser_run_id.value());
  }

  if (proxy_chain.IsValid()) {
    proxy_chain.Persist(pickle);
  }
}

bool HttpResponseInfo::DidUseQuic() const {
  switch (connection_info) {
    case HttpConnectionInfo::kUNKNOWN:
    case HttpConnectionInfo::kHTTP1_1:
    case HttpConnectionInfo::kDEPRECATED_SPDY2:
    case HttpConnectionInfo::kDEPRECATED_SPDY3:
    case HttpConnectionInfo::kHTTP2:
    case HttpConnectionInfo::kDEPRECATED_HTTP2_14:
    case HttpConnectionInfo::kDEPRECATED_HTTP2_15:
    case HttpConnectionInfo::kHTTP0_9:
    case HttpConnectionInfo::kHTTP1_0:
      return false;
    case HttpConnectionInfo::kQUIC_UNKNOWN_VERSION:
    case HttpConnectionInfo::kQUIC_32:
    case HttpConnectionInfo::kQUIC_33:
    case HttpConnectionInfo::kQUIC_34:
    case HttpConnectionInfo::kQUIC_35:
    case HttpConnectionInfo::kQUIC_36:
    case HttpConnectionInfo::kQUIC_37:
    case HttpConnectionInfo::kQUIC_38:
    case HttpConnectionInfo::kQUIC_39:
    case HttpConnectionInfo::kQUIC_40:
    case HttpConnectionInfo::kQUIC_41:
    case HttpConnectionInfo::kQUIC_42:
    case HttpConnectionInfo::kQUIC_43:
    case HttpConnectionInfo::kQUIC_44:
    case HttpConnectionInfo::kQUIC_45:
    case HttpConnectionInfo::kQUIC_46:
    case HttpConnectionInfo::kQUIC_47:
    case HttpConnectionInfo::kQUIC_Q048:
    case HttpConnectionInfo::kQUIC_T048:
    case HttpConnectionInfo::kQUIC_Q049:
    case HttpConnectionInfo::kQUIC_T049:
    case HttpConnectionInfo::kQUIC_Q050:
    case HttpConnectionInfo::kQUIC_T050:
    case HttpConnectionInfo::kQUIC_Q099:
    case HttpConnectionInfo::kQUIC_T099:
    case HttpConnectionInfo::kQUIC_999:
    case HttpConnectionInfo::kQUIC_DRAFT_25:
    case HttpConnectionInfo::kQUIC_DRAFT_27:
    case HttpConnectionInfo::kQUIC_DRAFT_28:
    case HttpConnectionInfo::kQUIC_DRAFT_29:
    case HttpConnectionInfo::kQUIC_T051:
    case HttpConnectionInfo::kQUIC_RFC_V1:
    case HttpConnectionInfo::kDEPRECATED_QUIC_2_DRAFT_1:
    case HttpConnectionInfo::kQUIC_2_DRAFT_8:
      return true;
  }
}

bool HttpResponseInfo::WasFetchedViaProxy() const {
  return proxy_chain.IsValid() && !proxy_chain.is_direct();
}

}  // namespace net
