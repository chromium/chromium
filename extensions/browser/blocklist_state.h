// Copyright 2014 The Chromium Authors. All rights reserved.
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

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BLOCKLIST_STATE_H_
