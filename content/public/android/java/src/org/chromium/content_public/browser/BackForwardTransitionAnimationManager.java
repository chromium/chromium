// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content.browser.navigation_transitions.BackForwardTransitionAnimationManagerImpl;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/**
 * Manages the back/forward transition animation for a WebContents. Callers must ensure that the
 * WebContents is visible and being composited when starting a gesture. Each gesture must end with
 * {@link #onGestureCancelled} or {@link #onGestureInvoked} before another gesture starts.
 *
 * <p>The native owner invalidates this manager when destroyed. Calls on an invalidated manager do
 * nothing, and {@link #getCurrentAnimationStage} returns {@link AnimationStage#NONE}.
 */
@NullMarked
public interface BackForwardTransitionAnimationManager {
    /** Whether the requested direction and swipe edge should trigger an animated transition. */
    static boolean shouldAnimateNavigationTransition(
            boolean forward, @BackGestureEventSwipeEdge int edge) {
        return BackForwardTransitionAnimationManagerImpl.shouldAnimateNavigationTransition(
                forward, edge);
    }

    /** Whether back/forward transitions are supported on this device. */
    static boolean shouldAnimateBackForwardTransitions() {
        return BackForwardTransitionAnimationManagerImpl.shouldAnimateBackForwardTransitions();
    }

    /**
     * Starts a history navigation gesture. The caller must check that a destination entry exists in
     * the requested direction. The gesture mode argument is recorded for crash diagnostics.
     */
    void onGestureStarted(
            float progress,
            @BackGestureEventSwipeEdge int edge,
            boolean forward,
            boolean isGestureMode);

    /** Updates the gesture progress, clamping incorrect values supplied by the OS to [0, 1]. */
    void onGestureProgressed(float progress, boolean isGestureMode);

    /** Ends the gesture without navigating. */
    void onGestureCancelled(boolean isGestureMode);

    /** Ends the gesture and starts the history navigation requested at gesture start. */
    void onGestureInvoked(boolean isGestureMode);

    /**
     * Notifies that the embedder has rendered the content for the committed navigation entry at its
     * final state, when the animation is waiting for embedder-provided UI.
     */
    void onContentForNavigationEntryShown();

    /** Returns the current stage of the back/forward transition. */
    @AnimationStage
    int getCurrentAnimationStage();
}
