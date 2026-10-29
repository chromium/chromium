// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.navigation_transitions;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.BackForwardTransitionAnimationManager;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/** JNI implementation of the WebContents-owned transition animation manager. */
@JNINamespace("content")
@NullMarked
public class BackForwardTransitionAnimationManagerImpl
        implements BackForwardTransitionAnimationManager {
    private long mNativePtr;

    private BackForwardTransitionAnimationManagerImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private static BackForwardTransitionAnimationManagerImpl create(long nativePtr) {
        return new BackForwardTransitionAnimationManagerImpl(nativePtr);
    }

    @CalledByNative
    private void destroy() {
        mNativePtr = 0;
    }

    @Override
    public void onGestureStarted(
            float progress,
            @BackGestureEventSwipeEdge int edge,
            boolean forward,
            boolean isGestureMode) {
        if (mNativePtr == 0) return;
        BackForwardTransitionAnimationManagerImplJni.get()
                .onGestureStarted(mNativePtr, progress, edge, forward, isGestureMode);
    }

    @Override
    public void onGestureProgressed(float progress, boolean isGestureMode) {
        if (mNativePtr == 0) return;
        BackForwardTransitionAnimationManagerImplJni.get()
                .onGestureProgressed(mNativePtr, progress, isGestureMode);
    }

    @Override
    public void onGestureCancelled(boolean isGestureMode) {
        if (mNativePtr == 0) return;
        BackForwardTransitionAnimationManagerImplJni.get()
                .onGestureCancelled(mNativePtr, isGestureMode);
    }

    @Override
    public void onGestureInvoked(boolean isGestureMode) {
        if (mNativePtr == 0) return;
        BackForwardTransitionAnimationManagerImplJni.get()
                .onGestureInvoked(mNativePtr, isGestureMode);
    }

    @Override
    public void onContentForNavigationEntryShown() {
        if (mNativePtr == 0) return;
        BackForwardTransitionAnimationManagerImplJni.get()
                .onContentForNavigationEntryShown(mNativePtr);
    }

    @Override
    public @AnimationStage int getCurrentAnimationStage() {
        if (mNativePtr == 0) return AnimationStage.NONE;
        return BackForwardTransitionAnimationManagerImplJni.get()
                .getCurrentAnimationStage(mNativePtr);
    }

    /** Whether the requested direction and swipe edge should trigger an animated transition. */
    public static boolean shouldAnimateNavigationTransition(
            boolean forward, @BackGestureEventSwipeEdge int edge) {
        return BackForwardTransitionAnimationManagerImplJni.get()
                .shouldAnimateNavigationTransition(forward, edge);
    }

    /** Whether back/forward transitions are supported on this device. */
    public static boolean shouldAnimateBackForwardTransitions() {
        return BackForwardTransitionAnimationManagerImplJni.get()
                .shouldAnimateBackForwardTransitions();
    }

    @NativeMethods
    public interface Natives {
        void onGestureStarted(
                long nativeBackForwardTransitionAnimationManagerAndroid,
                float progress,
                @JniType("ui::BackGestureEventSwipeEdge") @BackGestureEventSwipeEdge int edge,
                boolean forward,
                boolean isGestureMode);

        void onGestureProgressed(
                long nativeBackForwardTransitionAnimationManagerAndroid,
                float progress,
                boolean isGestureMode);

        void onGestureCancelled(
                long nativeBackForwardTransitionAnimationManagerAndroid, boolean isGestureMode);

        void onGestureInvoked(
                long nativeBackForwardTransitionAnimationManagerAndroid, boolean isGestureMode);

        void onContentForNavigationEntryShown(
                long nativeBackForwardTransitionAnimationManagerAndroid);

        @AnimationStage
        @JniType("content::BackForwardTransitionAnimationManager::AnimationStage")
        int getCurrentAnimationStage(long nativeBackForwardTransitionAnimationManagerAndroid);

        boolean shouldAnimateNavigationTransition(
                boolean forward,
                @JniType("ui::BackGestureEventSwipeEdge") @BackGestureEventSwipeEdge int edge);

        boolean shouldAnimateBackForwardTransitions();

        // Called by BackForwardTransitionAnimationManagerTestUtils, which registers the resetter.
        @JniType("base::OnceClosure")
        Runnable setMinRequiredPhysicalRamMbForTesting(int mb);
    }
}
