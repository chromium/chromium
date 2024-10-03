// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DISABLE_REASON_H_
#define EXTENSIONS_BROWSER_DISABLE_REASON_H_

#include <limits>

namespace extensions {
namespace disable_reason {

// Reasons a Chrome extension may be disabled. These are used in histograms, so
// do not remove/reorder entries - only add at the end just before
// DISABLE_REASON_LAST (and update the shift value for it). Also remember to
// update the enum listing in tools/metrics/histograms.xml.
// Also carefully consider if your reason should sync to other devices, and if
// so, add it to kKnownSyncableDisableReasons in
// chrome/browser/extensions/extension_sync_service.cc.
// Finally, consider whether your disable reason applies to component
// extensions. Reference/update the existing list of applicable reasons in
// ExtensionsPrefs::ClearInapplicableDisableReasonsForComponentExtension.
enum DisableReason {
  DISABLE_NONE = 0,
  DISABLE_USER_ACTION = 1 << 0,
  DISABLE_PERMISSIONS_INCREASE = 1 << 1,
  DISABLE_RELOAD = 1 << 2,
  DISABLE_UNSUPPORTED_REQUIREMENT = 1 << 3,
  DISABLE_SIDELOAD_WIPEOUT = 1 << 4,
  DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC = 1 << 5,
  // DISABLE_PERMISSIONS_CONSENT = 1 << 6,  // Deprecated.
  // DISABLE_KNOWN_DISABLED = 1 << 7,  // Deprecated.
  // Disabled because we could not verify the install.
  DISABLE_NOT_VERIFIED = 1 << 8,
  DISABLE_GREYLIST = 1 << 9,
  DISABLE_CORRUPTED = 1 << 10,
  DISABLE_REMOTE_INSTALL = 1 << 11,
  // DISABLE_INACTIVE_EPHEMERAL_APP = 1 << 12,  // Deprecated.
  // External extensions might be disabled for user prompting.
  DISABLE_EXTERNAL_EXTENSION = 1 << 13,
  // Doesn't meet minimum version requirement.
  DISABLE_UPDATE_REQUIRED_BY_POLICY = 1 << 14,
  // Supervised user needs approval by custodian.
  DISABLE_CUSTODIAN_APPROVAL_REQUIRED = 1 << 15,
  // Blocked due to management policy.
  DISABLE_BLOCKED_BY_POLICY = 1 << 16,
  // DISABLE_BLOCKED_MATURE = 1 << 17, // Deprecated.
  // DISABLE_REMOTELY_FOR_MALWARE = 1 << 18, // Deprecated.
  DISABLE_REINSTALL = 1 << 19,
  // Disabled by Safe Browsing extension allowlist enforcement.
  DISABLE_NOT_ALLOWLISTED = 1 << 20,
  // Disabled by Ash extension keep-list enforcement.
  DISABLE_NOT_ASH_KEEPLISTED = 1 << 21,
  // Disabled by policy when the extension is unpublished from the web store.
  DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY = 1 << 22,
  // Disabled because the extension uses an unsupported manifest version.
  DISABLE_UNSUPPORTED_MANIFEST_VERSION = 1 << 23,
  // Disabled because the extension is a "developer extension" (for example, an
  // unpacked extension) while the developer mode is OFF.
  DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION = 1 << 24,
  // This should always be the last value.
  DISABLE_REASON_LAST = 1LL << 25,
};

static_assert(DISABLE_REASON_LAST - 1 <= std::numeric_limits<int>::max(),
              "The DisableReason bitmask cannot be stored in an int.");

}  // namespace disable_reason
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DISABLE_REASON_H_
