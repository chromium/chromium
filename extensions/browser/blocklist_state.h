// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOCKLIST_STATE_H_
#define EXTENSIONS_BROWSER_BLOCKLIST_STATE_H_

namespace extensions {

// The numeric values here match the values of the respective enum in
// ClientCRXListInfoResponse proto.
enum BlocklistState {
  NOT_BLOCKLISTED = 0,
  BLOCKLISTED_MALWARE = 1,
  BLOCKLISTED_SECURITY_VULNERABILITY = 2,
  BLOCKLISTED_CWS_POLICY_VIOLATION = 3,
  BLOCKLISTED_POTENTIALLY_UNWANTED = 4,
  BLOCKLISTED_UNKNOWN = 5  // Used when we couldn't connect to server,
                           // e.g. when offline.
};

// The new bit map version of `BlocklistState`. The values should match the
// respective enum in ClientCRXListInfoResponse proto. This enum is added in
// addition to the original `BlocklistState` because there can be multiple
// blocklist states in omaha attributes.
enum class BitMapBlocklistState {
  NOT_BLOCKLISTED = 0,
  BLOCKLISTED_MALWARE = 1 << 0,
  BLOCKLISTED_SECURITY_VULNERABILITY = 1 << 1,
  BLOCKLISTED_CWS_POLICY_VIOLATION = 1 << 2,
  BLOCKLISTED_POTENTIALLY_UNWANTED = 1 << 3,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BLOCKLIST_STATE_H_
