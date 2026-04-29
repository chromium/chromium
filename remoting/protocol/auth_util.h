// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTH_UTIL_H_
#define REMOTING_PROTOCOL_AUTH_UTIL_H_

#include <stddef.h>

#include <string>
#include <string_view>

namespace remoting::protocol {

// Size of the HMAC-SHA-256 hash used as shared secret in SPAKE2.
const size_t kSharedSecretHashLength = 32;

// Returns HMAC-SHA-256 hash for |shared_secret| with the specified |tag|.
std::string GetSharedSecretHash(const std::string& tag,
                                const std::string& shared_secret);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUTH_UTIL_H_
