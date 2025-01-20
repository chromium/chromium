// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/disable_reason.h"

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
         reason == disable_reason::DISABLE_NOT_ASH_KEEPLISTED ||
         reason ==
             disable_reason::DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY ||
         reason == disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION ||
         reason == disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION ||
         reason == disable_reason::DISABLE_UNKNOWN;
}

}  // namespace extensions
