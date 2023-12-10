// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for MagnifierAnimator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MagnifierAnimatorTest {
    private MagnifierWrapper mMagnifier;
    private MagnifierAnimator mAnimator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;

        mMagnifier = Mockito.mock(MagnifierWrapper.class);
        when(mMagnifier.isAvailable()).thenReturn(true);

        mAnimator = new MagnifierAnimator(mMagnifier);
    }

    @Test
    @Feature({"Selection", "TextSelectionMagnifier"})
    public void testNormalFlow() {
        InOrder order = inOrder(mMagnifier);

        // Start to move magnifier.
        mAnimator.handleDragStartedOrMoved(10.f, 10.f);
        order.verify(mMagnifier).show(10.f, 10.f);
        assertFalse("Animator shouldn't run before we change position", isAnimatorRunning());

        // Second move doesn't change y-coordinate.
        mAnimator.handleDragStartedOrMoved(11.f, 10.f);
        order.verify(mMagnifier).show(11.f, 10.f);

        // Change y-coordinate. Should trigger animator.
        mAnimator.handleDragStartedOrMoved(11.f, 15.f);
        assertTrue("Animator should started to run", isAnimatorRunning());

        for (int i = 0; i <= 10; ++i) {
            final float fraction = 0.1f * i;
            setAnimatorCurrentFraction(fraction);
        }

        order.verify(mMagnifier).show(11.f, 15.f);

        mAnimator.handleDragStopped();
        order.verify(mMagnifier).dismiss();
        assertFalse("Animator should be cancelled", isAnimatorRunning());
    }

    @Test
    @Feature({"Selection", "TextSelectionMagnifier"})
    public void testTwoConsecutiveYMoves() {
        InOrder order = inOrder(mMagnifier);

        // Start to move magnifier.
        mAnimator.handleDragStartedOrMoved(10.f, 10.f);
        order.verify(mMagnifier).show(10.f, 10.f);
        assertFalse("Animator shouldn't run before we change position", isAnimatorRunning());

        // Change y-coordinate. Should trigger animator.
        mAnimator.handleDragStartedOrMoved(11.f, 15.f);
        order.verify(mMagnifier).show(10.f, 10.f);
        assertTrue("Animator should started to run", isAnimatorRunning());

        float currentX = 0.f;
        float currentY = 0.f;
        // Animation running.
        for (int i = 0; i < 5; ++i) {
            final float fraction = 0.1f * i;
            currentX = currentValue(10.f, 11.f, fraction);
            currentY = currentValue(10.f, 15.f, fraction);
            setAnimatorCurrentFraction(fraction);
            order.verify(mMagnifier).show(currentX, currentY);
        }

        assertTrue("Animator should still run", isAnimatorRunning());
        mAnimator.handleDragStartedOrMoved(13.f, 20.f);
        order.verify(mMagnifier).show(currentX, currentY);
        assertTrue("Animator should started to run again", isAnimatorRunning());

        for (int i = 0; i <= 10; ++i) {
            final float fraction = 0.1f * i;
            setAnimatorCurrentFraction(fraction);
        }

        order.verify(mMagnifier).show(13.f, 20.f);
        order.verify(mMagnifier, never()).show(11.f, 15.f);
    }

    @Test
    @Feature({"Selection", "TextSelectionMagnifier"})
    public void testCancelMagnifierDuringAnimation() {
        InOrder order = inOrder(mMagnifier);

        // Start to move magnifier.
        mAnimator.handleDragStartedOrMoved(10.f, 10.f);
        order.verify(mMagnifier).show(10.f, 10.f);
        assertFalse("Animator shouldn't run before we change position", isAnimatorRunning());

        // Change y-coordinate. Should trigger animator.
        mAnimator.handleDragStartedOrMoved(11.f, 15.f);
        // The previous end point is our current start point.
        order.verify(mMagnifier).show(10.f, 10.f);
        assertTrue("Animator should started to run", isAnimatorRunning());

        // Animation running.
        for (int i = 0; i < 5; ++i) {
            final float fraction = 0.1f * i;
            final float currentX = currentValue(10.f, 11.f, fraction);
            final float currentY = currentValue(10.f, 15.f, fraction);
            setAnimatorCurrentFraction(fraction);
            order.verify(mMagnifier).show(currentX, currentY);
        }

        mAnimator.handleDragStopped();
        order.verify(mMagnifier, never()).show(anyFloat(), anyFloat());
        order.verify(mMagnifier).dismiss();
        assertFalse("Animator should be cancelled", isAnimatorRunning());
    }

    private boolean isAnimatorRunning() {
        return mAnimator.getValueAnimatorForTesting().isRunning();
    }

    private void setAnimatorCurrentFraction(float fraction) {
        mAnimator.getValueAnimatorForTesting().setCurrentFraction(fraction);
    }

    private float currentValue(float start, float target, float fraction) {
        return start + (target - start) * fraction;
    }
}
