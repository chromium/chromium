// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_INVARIANTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_INVARIANTS_H_

#include <string_view>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Intrinsic invariant violations checked by `Manifest::ValidateInvariants()`
// during `Manifest::Builder::Build()` and Mojo deserialization.
//
// These values are persisted to logs (`WebApp.Manifest.InvariantViolation`).
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(ManifestInvariantError)
enum class ManifestInvariantError {
  kStartUrlInvalid = 0,
  kIdInvalid = 1,
  kScopeInvalid = 2,
  kOriginOpaque = 3,
  kIdCrossOrigin = 4,
  kScopeCrossOrigin = 5,
  kShareTargetActionCrossOrigin = 6,
  kFileHandlerActionCrossOrigin = 7,
  kFileHandlerInvalidMimeType = 8,
  kFileHandlerEmptyExtensionList = 9,
  kFileHandlerInvalidExtension = 10,
  kFileHandlerExtensionContainsControlOrFormat = 11,
  kProtocolHandlerUrlCrossOrigin = 12,
  kProtocolHandlerInvalidScheme = 13,
  kNoteTakingUrlCrossOrigin = 14,
  kLockScreenStartUrlCrossOrigin = 15,
  kMigrateFromIdInvalid = 16,
  kMigrateFromIdCrossSite = 17,
  kMigrateFromInstallUrlInvalid = 18,
  kMigrateFromInstallUrlCrossSite = 19,
  kMigrateFromInstallUrlCrossOriginWithId = 20,
  kMigrateToIdInvalid = 21,
  kMigrateToIdCrossSite = 22,
  kMigrateToInstallUrlInvalid = 23,
  kMigrateToInstallUrlCrossSite = 24,
  kMigrateToInstallUrlCrossOriginWithId = 25,
  kShortcutUrlInvalid = 26,
  kShortcutUrlOutOfScope = 27,
  kScopeExtensionOriginOpaque = 28,
  kScopeExtensionSchemeNotHttps = 29,
  kIconSrcInvalid = 30,
  kIconSrcInvalidScheme = 31,
  kMaxValue = kIconSrcInvalidScheme,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:ManifestInvariantError)

// Returns a human-readable name for `error` for logging and crash keys.
BLINK_COMMON_EXPORT std::string_view ManifestInvariantErrorToString(
    ManifestInvariantError error);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_INVARIANTS_H_
