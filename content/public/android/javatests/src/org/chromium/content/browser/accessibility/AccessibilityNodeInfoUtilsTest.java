// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.graphics.Rect;
import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.accessibility.AccessibilityFeatures;

/** Test suite for {@link AccessibilityNodeInfoUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccessibilityNodeInfoUtilsTest {
    @Test
    @SmallTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS)
    public void testResizedRectOnOcclusion_fullyOccluded() {
        Rect nodeBounds = new Rect(0, 0, 100, 100);

        SparseArray<Rect> occludingRects = new SparseArray<>();
        occludingRects.put(1, new Rect(0, 0, 100, 100));

        Assert.assertNull(
                AccessibilityNodeInfoUtils.resizedRectOnOcclusion(nodeBounds, occludingRects));
    }

    @Test
    @SmallTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS)
    public void testResizedRectOnOcclusion_partiallyOccluded_rightSide() {
        Rect nodeBounds = new Rect(0, 0, 100, 100);

        SparseArray<Rect> occludingRects = new SparseArray<>();
        occludingRects.put(1, new Rect(50, 0, 100, 100));

        Rect newBounds =
                AccessibilityNodeInfoUtils.resizedRectOnOcclusion(nodeBounds, occludingRects);
        Assert.assertEquals(new Rect(0, 0, 50, 100), newBounds);
    }

    @Test
    @SmallTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS)
    public void testResizedRectOnOcclusion_partiallyOccluded_leftSide() {
        Rect nodeBounds = new Rect(0, 0, 100, 100);

        SparseArray<Rect> occludingRects = new SparseArray<>();
        occludingRects.put(1, new Rect(0, 0, 50, 100));

        Rect newBounds =
                AccessibilityNodeInfoUtils.resizedRectOnOcclusion(nodeBounds, occludingRects);
        Assert.assertEquals(new Rect(50, 0, 100, 100), newBounds);
    }

    @Test
    @SmallTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS)
    public void testResizedRectOnOcclusion_partiallyOccluded_topSide() {
        Rect nodeBounds = new Rect(0, 0, 100, 100);

        SparseArray<Rect> occludingRects = new SparseArray<>();
        occludingRects.put(1, new Rect(0, 0, 100, 50));

        Rect newBounds =
                AccessibilityNodeInfoUtils.resizedRectOnOcclusion(nodeBounds, occludingRects);
        Assert.assertEquals(new Rect(0, 50, 100, 100), newBounds);
    }

    @Test
    @SmallTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS)
    public void testResizedRectOnOcclusion_partiallyOccluded_bottomSide() {
        Rect nodeBounds = new Rect(0, 0, 100, 100);

        SparseArray<Rect> occludingRects = new SparseArray<>();
        occludingRects.put(1, new Rect(0, 50, 100, 100));

        Rect newBounds =
                AccessibilityNodeInfoUtils.resizedRectOnOcclusion(nodeBounds, occludingRects);
        Assert.assertEquals(new Rect(0, 0, 100, 50), newBounds);
    }

    @Test
    @SmallTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS)
    public void testResizedRectOnOcclusion_notOccluded() {
        Rect nodeBounds = new Rect(0, 0, 100, 100);

        SparseArray<Rect> occludingRects = new SparseArray<>();

        Rect newBounds =
                AccessibilityNodeInfoUtils.resizedRectOnOcclusion(nodeBounds, occludingRects);
        Assert.assertEquals(nodeBounds, newBounds);
    }
}
