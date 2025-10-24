// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/back_forward_transition_animation_manager.h"

#include "base/auto_reset.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/gfx/animation/animation.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "ui/base/l10n/l10n_util_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

// static
bool BackForwardTransitionAnimationManager::
    ShouldAnimateBackForwardTransitions() {
  return content::GetContentClient()
      ->browser()
      ->ShouldAnimateBackForwardTransitions();
}

// static
base::AutoReset<int>
BackForwardTransitionAnimationManager::SetMinRequiredPhysicalRamMbForTesting(
    int mb) {
#if BUILDFLAG(IS_ANDROID)
  return NavigationTransitionConfig::SetMinRequiredPhysicalRamMbForTesting(mb);
#else
  return base::AutoReset<int>(nullptr, 0);
#endif  // BUILDFLAG(IS_ANDROID)
}

// static
bool BackForwardTransitionAnimationManager::ShouldAnimateNavigationTransition(
    NavigationDirection navigation_direction,
    ui::BackGestureEventSwipeEdge edge) {
#if BUILDFLAG(IS_ANDROID)
  if (!ShouldAnimateBackForwardTransitions()) {
    return false;
  }

  if (gfx::Animation::PrefersReducedMotion()) {
    return false;
  }

  ui::BackGestureEventSwipeEdge semantic_back_edge =
      l10n_util::ShouldMirrorBackForwardGestures()
          ? ui::BackGestureEventSwipeEdge::RIGHT
          : ui::BackGestureEventSwipeEdge::LEFT;

  bool is_back = navigation_direction == NavigationDirection::kBackward;
  bool from_semantic_back_edge = edge == semantic_back_edge;

  // If navigating forward, the swipe must come from the semantic forward
  // direction (back can come from either direction in gestural navigation
  // mode).
  CHECK(is_back || !from_semantic_back_edge);

  // We allow back animations from the back edge and forward animations from the
  // forward edge but disallow animations from an opposing edge (e.g. in three
  // button mode where both edges navigate back, in a left-to-right UI we don't
  // animate the back navigation that occurs from the right edge because
  // "semantically" that edge is forward).
  if (is_back != from_semantic_back_edge) {
    return false;
  }

  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}
}  // namespace content
