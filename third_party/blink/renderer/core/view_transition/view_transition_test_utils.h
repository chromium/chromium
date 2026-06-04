// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_TEST_UTILS_H_

#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

namespace blink {

class ViewTransitionTestUtils {
 public:
  static void ProcessPendingDirectives(Document& document,
                                       cc::LayerTreeHost* layer_tree_host) {
    ViewTransitionUtils::ForEachTransition(
        document, &ViewTransitionTestUtils::CommitCaptureIfNeeded);
    for (auto& callback :
         layer_tree_host->TakeViewTransitionCallbacksForTesting()) {
      std::move(callback).Run({});
    }
  }

  static void CommitCaptureIfNeeded(ViewTransition& transition) {
    if (transition.state_ == ViewTransition::State::kCapturing) {
      // If rendering has already been paused (e.g., the test manually called
      // WillCommitCompositorFrame), we should not pause it again.
      if (transition.style_tracker_ &&
          !transition.style_tracker_->IsOldSnapshotFrozen()) {
        transition.PauseRendering();
      }
      if (auto* delegate = transition.delegate_;
          delegate && delegate->IsEarlyCallbackEnabled()) {
        delegate->OnCaptureCommitted(&transition);
      }
    }
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_TEST_UTILS_H_
