// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTH_UTIL_H_
#define REMOTING_PROTOCOL_AUTH_UTIL_H_

#include <stddef.h>

#include <string>
#include <string_view>

namespace net {
class SSLSocket;
}  // namespace net

namespace remoting::protocol {

// Labels for use when exporting the SSL shared secret.
extern const char kClientAuthSslExporterLabel[];
extern const char kHostAuthSslExporterLabel[];

// Fake hostname used for SSL connections.
extern const char kSslFakeHostName[];

// Size of the HMAC-SHA-256 hash used as shared secret in SPAKE2.
const size_t kSharedSecretHashLength = 32;

// Size of the HMAC-SHA-256 digest used for channel authentication.
const size_t kAuthDigestLength = 32;

// Returns HMAC-SHA-256 hash for |shared_secret| with the specified |tag|.
std::string GetSharedSecretHash(const std::string& tag,
                                const std::string& shared_secret);

// Returns authentication bytes that must be used for the given
// |socket|. Empty string is returned in case of failure.
std::string GetAuthBytes(net::SSLSocket* socket,
                         const std::string_view& label,
                         const std::string_view& shared_secret);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUTH_UTIL_H_
