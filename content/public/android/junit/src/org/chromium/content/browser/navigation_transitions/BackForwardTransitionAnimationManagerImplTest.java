// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.navigation_transitions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.BackForwardTransitionAnimationManager;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/** Tests the Java peer's lifetime independently of its native owner. */
@RunWith(BaseRobolectricTestRunner.class)
public class BackForwardTransitionAnimationManagerImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BackForwardTransitionAnimationManagerImpl.Natives mNative;
    private BackForwardTransitionAnimationManager mManager;

    @Before
    public void setUp() {
        BackForwardTransitionAnimationManagerImplJni.setInstanceForTesting(mNative);
        mManager =
                ReflectionHelpers.callStaticMethod(
                        BackForwardTransitionAnimationManagerImpl.class,
                        "create",
                        ClassParameter.from(long.class, 123L));
    }

    @Test
    public void nativeDestructionInvalidatesRetainedJavaPeer() {
        ReflectionHelpers.callInstanceMethod(mManager, "destroy");
        mManager.onGestureStarted(0.2f, BackGestureEventSwipeEdge.LEFT, false, true);
        mManager.onGestureProgressed(0.5f, true);
        mManager.onGestureCancelled(true);
        mManager.onGestureInvoked(true);
        mManager.onContentForNavigationEntryShown();
        assertEquals(AnimationStage.NONE, mManager.getCurrentAnimationStage());
        verifyNoInteractions(mNative);
    }

    @Test
    public void newPeerRemainsUsableAfterOldPeerIsDestroyed() {
        ReflectionHelpers.callInstanceMethod(mManager, "destroy");
        BackForwardTransitionAnimationManager replacement =
                ReflectionHelpers.callStaticMethod(
                        BackForwardTransitionAnimationManagerImpl.class,
                        "create",
                        ClassParameter.from(long.class, 456L));
        replacement.onGestureStarted(0.2f, BackGestureEventSwipeEdge.LEFT, false, true);
        verify(mNative).onGestureStarted(456L, 0.2f, BackGestureEventSwipeEdge.LEFT, false, true);
    }
}
