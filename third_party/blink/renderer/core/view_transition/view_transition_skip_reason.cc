// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_skip_reason.h"

#include "base/notreached.h"

namespace blink {

const char* SkipReasonToString(ViewTransitionSkipReason reason) {
  switch (reason) {
    case ViewTransitionSkipReason::kExpected:
      return "";
    case ViewTransitionSkipReason::kUserSkipped:
      return "skipTransition() called";
    case ViewTransitionSkipReason::kNewTransitionStarted:
      return "New ViewTransition started";
    case ViewTransitionSkipReason::kDocumentHidden:
      return "Document hidden";
    case ViewTransitionSkipReason::kNoView:
      return "Document attached view missing";
    case ViewTransitionSkipReason::kTimeout:
      return "DOM update timed out";
    case ViewTransitionSkipReason::kUnsupportedCapture:
      return "Unsupported layout or style";
    case ViewTransitionSkipReason::kCaptureFailed:
      return "Snapshot capture failed";
    case ViewTransitionSkipReason::kStartFailed:
      return "Animation start failed";
    case ViewTransitionSkipReason::kSnapshotRootChangedSize:
      return "Viewport size changed";
    case ViewTransitionSkipReason::kContextDestroyed:
      return "Document destroyed";
    case ViewTransitionSkipReason::kCompositorCommit:
      return "Premature compositor commit";
    case ViewTransitionSkipReason::kNavigationAborted:
      return "Navigation aborted";
    case ViewTransitionSkipReason::kOptInDisabled:
      return "ViewTransition opt-in disabled";
    case ViewTransitionSkipReason::kPageAlreadyRevealed:
      return "Page already revealed";
    case ViewTransitionSkipReason::kIncompatibleStyle:
      return "Incompatible style on scope element";
    case ViewTransitionSkipReason::kPostPrePaintFailed:
      return "Prepaint layout check failed";
    case ViewTransitionSkipReason::kDuplicateTag:
      return "Duplicate view-transition-name";
    case ViewTransitionSkipReason::kNavigationTransitionActive:
      return "Navigation transition active";
  }
  NOTREACHED();
}

}  // namespace blink
