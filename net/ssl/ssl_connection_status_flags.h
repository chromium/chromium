// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CONNECTION_STATUS_FLAGS_H_
#define NET_SSL_SSL_CONNECTION_STATUS_FLAGS_H_

#include <stdint.h>

#include "base/check_op.h"

namespace net {

// Status flags for SSLInfo::connection_status.
enum {
  // The lower 16 bits are reserved for the TLS ciphersuite id.
  SSL_CONNECTION_CIPHERSUITE_MASK = 0xffff,

  // The next two bits are reserved for the compression used.
  SSL_CONNECTION_COMPRESSION_SHIFT = 16,
  SSL_CONNECTION_COMPRESSION_MASK = 3,

  // 1 << 18 was previously used for SSL_CONNECTION_VERSION_FALLBACK.
  // 1 << 19 was previously used for SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION.

  // The next three bits are reserved for the SSL version.
  SSL_CONNECTION_VERSION_SHIFT = 20,
  SSL_CONNECTION_VERSION_MASK = 7,

  // 1 << 31 (the sign bit) is reserved so that the SSL connection status will
  // never be negative.
};

// NOTE: the SSL version enum constants must be between 0 and
// SSL_CONNECTION_VERSION_MASK, inclusive. These values are persisted to disk
// and used in UMA, so they must remain stable.
enum SSLVersion {
  SSL_CONNECTION_VERSION_UNKNOWN = 0,  // Unknown SSL version.
  SSL_CONNECTION_VERSION_SSL2 = 1,
  SSL_CONNECTION_VERSION_SSL3 = 2,
  SSL_CONNECTION_VERSION_TLS1 = 3,
  SSL_CONNECTION_VERSION_TLS1_1 = 4,
  SSL_CONNECTION_VERSION_TLS1_2 = 5,
  SSL_CONNECTION_VERSION_TLS1_3 = 6,
  SSL_CONNECTION_VERSION_QUIC = 7,
  SSL_CONNECTION_VERSION_MAX,
};
static_assert(SSL_CONNECTION_VERSION_MAX - 1 <= SSL_CONNECTION_VERSION_MASK,
              "SSL_CONNECTION_VERSION_MASK too small");

inline uint16_t SSLConnectionStatusToCipherSuite(int connection_status) {
  return static_cast<uint16_t>(connection_status);
}

inline SSLVersion SSLConnectionStatusToVersion(int connection_status) {
  return static_cast<SSLVersion>(
      (connection_status >> SSL_CONNECTION_VERSION_SHIFT) &
      SSL_CONNECTION_VERSION_MASK);
}

inline void SSLConnectionStatusSetCipherSuite(uint16_t cipher_suite,
                                              int* connection_status) {
  // Clear out the old ciphersuite.
  *connection_status &= ~SSL_CONNECTION_CIPHERSUITE_MASK;
  // Set the new ciphersuite.
  *connection_status |= cipher_suite;
}

inline void SSLConnectionStatusSetVersion(int version, int* connection_status) {
  DCHECK_GT(version, 0);
  DCHECK_LT(version, SSL_CONNECTION_VERSION_MAX);

  // Clear out the old version.
  *connection_status &=
      ~(SSL_CONNECTION_VERSION_MASK << SSL_CONNECTION_VERSION_SHIFT);
  // Set the new version.
  *connection_status |=
      ((version & SSL_CONNECTION_VERSION_MASK) << SSL_CONNECTION_VERSION_SHIFT);
}

}  // namespace net

#endif  // NET_SSL_SSL_CONNECTION_STATUS_FLAGS_H_
