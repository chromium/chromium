// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CAPABILITY_ELEMENT_METRICS_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CAPABILITY_ELEMENT_METRICS_UTIL_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"

namespace blink {

// These values are used for histograms. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(UserInteractionDeniedReason)
enum class UserInteractionDeniedReason {
  kInvalidType = 0,
  kFailedOrHasNotBeenRegistered = 1,
  kRecentlyAttachedToLayoutTree = 2,
  // kIntersectionRecentlyFullyVisible = 3,    Deprecated.
  kInvalidStyle = 4,
  kUntrustedEvent = 5,
  kIntersectionWithViewportChanged = 6,
  kIntersectionVisibilityOutOfViewPortOrClipped = 7,
  kIntersectionVisibilityOccludedOrDistorted = 8,
  kAttributeChanged = 9,
  kInstallDataError = 10,
  kMaxValue = kInstallDataError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:PermissionElementUserInteractionDeniedReason)

// These values are used for histograms. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InvalidStyleReason)
enum class InvalidStyleReason {
  kNoComputedStyle = 0,
  kNonOpaqueColorOrBackgroundColor = 1,
  kLowConstrastColorAndBackgroundColor = 1,
  kTooSmallFontSize = 3,
  kTooLargeFontSize = 4,
  kInvalidDisplayProperty = 5,

  kMaxValue = kInvalidDisplayProperty,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:PermissionElementInvalidStyleReason)

void RecordPermissionElementUseCounter(Document& document,
                                     const QualifiedName& tag_name);

void RecordPermissionElementUserInteractionAccepted(
    const QualifiedName& tag_name,
    bool accepted);

void RecordPermissionElementUserInteractionDeniedReason(
    const QualifiedName& tag_name,
    UserInteractionDeniedReason reason);

void RecordPermissionElementInvalidStyleReason(const QualifiedName& tag_name,
                                               InvalidStyleReason reason);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CAPABILITY_ELEMENT_METRICS_UTIL_H_
