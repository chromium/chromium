// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CIPHER_SUITE_NAMES_H_
#define NET_SSL_SSL_CIPHER_SUITE_NAMES_H_

#include <stdint.h>

#include <string>

#include "net/base/net_export.h"

namespace net {

// SSLCipherSuiteToStrings returns three strings for a given cipher suite
// number, the name of the key exchange algorithm, the name of the cipher and
// the name of the MAC. The cipher suite number is the number as sent on the
// wire and recorded at
// http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
// If the cipher suite is unknown, the strings are set to "???".
// In the case of an AEAD cipher suite, *mac_str is nullptr and *is_aead is
// true.
// In the case of a TLS 1.3 AEAD-only cipher suite, *key_exchange_str is nullptr
// and *is_tls13 is true.
NET_EXPORT void SSLCipherSuiteToStrings(const char** key_exchange_str,
                                        const char** cipher_str,
                                        const char** mac_str,
                                        bool* is_aead,
                                        bool* is_tls13,
                                        uint16_t cipher_suite);

// SSLVersionToString returns the name of the SSL protocol version
// specified by |ssl_version|, which is defined in
// net/ssl/ssl_connection_status_flags.h.
// If the version is unknown, |name| is set to "???".
NET_EXPORT void SSLVersionToString(const char** name, int ssl_version);

// Parses a string literal that represents a SSL/TLS cipher suite.
//
// Supported literal forms:
//   0xAABB, where AA is cipher_suite[0] and BB is cipher_suite[1], as
//     defined in RFC 2246, Section 7.4.1.2. Unrecognized but parsable cipher
//     suites in this form will not return an error.
//
// Returns true if the cipher suite was successfully parsed, storing the
// result in |cipher_suite|.
//
// TODO(rsleevi): Support the full strings defined in the IANA TLS parameters
// list.
NET_EXPORT bool ParseSSLCipherString(const std::string& cipher_string,
                                     uint16_t* cipher_suite);

// Mask definitions for an integer that holds obsolete SSL setting details.
enum ObsoleteSSLMask {
  OBSOLETE_SSL_NONE = 0,  // Modern SSL
  OBSOLETE_SSL_MASK_PROTOCOL = 1 << 0,
  OBSOLETE_SSL_MASK_KEY_EXCHANGE = 1 << 1,
  OBSOLETE_SSL_MASK_CIPHER = 1 << 2,
  OBSOLETE_SSL_MASK_SIGNATURE = 1 << 3,
};

// Takes the given |connection_status| and |signature_algorithm| and returns a
// bitmask indicating which settings do not meet modern best-practice security
// standards - that is, which ones are "obsolete".
//
// Currently, this function uses the following criteria to determine what is
// obsolete:
//
// - Protocol: less than TLS 1.2
// - Key exchange: Does not use ECDHE-based key exchanges authenticated by a
//   certificate
// - Cipher: not an AEAD cipher
// - Signature algorithm: MD5 or SHA-1
NET_EXPORT int ObsoleteSSLStatus(int connection_status,
                                 uint16_t signature_algorithm);

// Returns true if |cipher_suite| is suitable for use with HTTP/2. See
// https://http2.github.io/http2-spec/#rfc.section.9.2.2.
NET_EXPORT bool IsTLSCipherSuiteAllowedByHTTP2(uint16_t cipher_suite);

}  // namespace net

#endif  // NET_SSL_SSL_CIPHER_SUITE_NAMES_H_
