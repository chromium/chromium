// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_TRUST_UTIL_H_
#define REMOTING_HOST_MAC_TRUST_UTIL_H_

#include <Security/Security.h>

namespace remoting {

// Validates the signature for the provided `audit_token` and returns true if
// the process is trusted. Note that this always returns true on non-official
// builds.
bool IsProcessTrusted(audit_token_t audit_token);

}  // namespace remoting

#endif  // REMOTING_HOST_MAC_TRUST_UTIL_H_
