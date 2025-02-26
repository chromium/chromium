// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/disable_reason.h"

#include "base/types/cxx23_to_underlying.h"

namespace extensions {

bool IsValidDisableReason(int reason) {
  static_assert(extensions::disable_reason::DISABLE_REASON_LAST == (1LL << 26),
                "Please update this method whenever a new disable reason is "
                "added / removed.");
  return reason == disable_reason::DISABLE_NONE ||
         reason == disable_reason::DISABLE_USER_ACTION ||
         reason == disable_reason::DISABLE_PERMISSIONS_INCREASE ||
         reason == disable_reason::DISABLE_RELOAD ||
         reason == disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT ||
         reason == disable_reason::DISABLE_SIDELOAD_WIPEOUT ||
         reason == disable_reason::DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC ||
         reason == disable_reason::DISABLE_NOT_VERIFIED ||
         reason == disable_reason::DISABLE_GREYLIST ||
         reason == disable_reason::DISABLE_CORRUPTED ||
         reason == disable_reason::DISABLE_REMOTE_INSTALL ||
         reason == disable_reason::DISABLE_EXTERNAL_EXTENSION ||
         reason == disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY ||
         reason == disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED ||
         reason == disable_reason::DISABLE_BLOCKED_BY_POLICY ||
         reason == disable_reason::DISABLE_REINSTALL ||
         reason == disable_reason::DISABLE_NOT_ALLOWLISTED ||
         reason == disable_reason::DEPRECATED_DISABLE_NOT_ASH_KEEPLISTED ||
         reason ==
             disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY ||
         reason == disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION ||
         reason == disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION ||
         reason == disable_reason::DISABLE_UNKNOWN;
}

int IntegerSetToBitflag(const base::flat_set<int>& set) {
  int bitflag = 0;
  for (int reason : set) {
    CHECK_EQ(reason & (reason - 1), 0)
        << "Only one bit should be set for each reason";
    bitflag |= reason;
  }
  return bitflag;
}

base::flat_set<int> BitflagToIntegerSet(int bit_flag) {
  base::flat_set<int> set;
  for (int i = 0; i < 32; ++i) {
    int val = (1 << i);
    if (bit_flag & val) {
      set.insert(val);
    }
  }
  return set;
}

base::flat_set<int> DisableReasonSetToIntegerSet(const DisableReasonSet& set) {
  base::flat_set<int> result;
  for (disable_reason::DisableReason reason : set) {
    result.insert(base::to_underlying(reason));
  }
  return result;
}
}  // namespace extensions
