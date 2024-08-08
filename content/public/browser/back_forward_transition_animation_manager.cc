// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/back_forward_transition_animation_manager.h"

#include "third_party/blink/public/common/features_generated.h"
#include "ui/gfx/animation/animation.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/base/l10n/l10n_util_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

// static
bool BackForwardTransitionAnimationManager::ShouldAnimateNavigationTransition(
    NavigationDirection navigation_direction,
    ui::BackGestureEventSwipeEdge edge) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions)) {
    return false;
  }

  if (gfx::Animation::PrefersReducedMotion()) {
    return false;
  }

  ui::BackGestureEventSwipeEdge semantic_back_edge =
      l10n_util::ShouldMirrorBackForwardGestures()
          ? ui::BackGestureEventSwipeEdge::RIGHT
          : ui::BackGestureEventSwipeEdge::LEFT;

  // Currently we only have approved UX for the history back navigation on the
  // back edge (left in LTR), in both gesture mode and 3-button mode.
  if (navigation_direction != NavigationDirection::kBackward ||
      edge != semantic_back_edge) {
    return false;
  }

  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}
}  // namespace content
