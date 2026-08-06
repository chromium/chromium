// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_SKIP_REASON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_SKIP_REASON_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Reasons why a view transition is skipped/aborted.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ViewTransitionSkipReason {
  // Expected skip during normal transition lifecycle or replacement.
  kExpected = 0,

  // Script explicitly called viewTransition.skipTransition().
  kUserSkipped = 1,

  // A new view transition was started while an existing transition was active.
  kNewTransitionStarted = 2,

  // The document became hidden (e.g. tab switched or window minimized).
  kDocumentHidden = 3,

  // The document is not attached to an active frame view.
  kNoView = 4,

  // The update callback timed out before resolving.
  kTimeout = 5,

  // An element participating in the transition has an unsupported layout or
  // style
  // (e.g., display: contents).
  kUnsupportedCapture = 6,

  // Failed to capture snapshot resource IDs or browser controls snapshot.
  kCaptureFailed = 7,

  // Failed to start animation or setup renderer animation structures.
  kStartFailed = 8,

  // The snapshot root (viewport) changed size between capture and animate
  // phases.
  kSnapshotRootChangedSize = 9,

  // The execution context or document was destroyed or navigated away.
  kContextDestroyed = 10,

  // Premature compositor frame commit occurred before snapshotting completed.
  kCompositorCommit = 11,

  // Cross-document or page navigation was aborted.
  kNavigationAborted = 12,

  // Cross-document @view-transition opt-in feature is disabled or missing.
  kOptInDisabled = 13,

  // The incoming page has already completed its initial reveal before
  // transition started.
  kPageAlreadyRevealed = 14,

  // Scope element has incompatible style changes during transition lifecycle.
  kIncompatibleStyle = 15,

  // Post-prepaint layout check failed (unexpected style or layout mutation
  // during snapshotting).
  kPostPrePaintFailed = 16,

  // Multiple elements shared the same view-transition-name.
  kDuplicateTag = 17,

  // A cross-document navigation transition is already active, blocking script
  // transitions.
  kNavigationTransitionActive = 18,

  kMaxValue = kNavigationTransitionActive,
};

CORE_EXPORT const char* SkipReasonToString(ViewTransitionSkipReason reason);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_SKIP_REASON_H_
