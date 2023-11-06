// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static android.view.inputmethod.HandwritingGesture.GRANULARITY_CHARACTER;
import static android.view.inputmethod.HandwritingGesture.GRANULARITY_WORD;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.content.browser.input.StylusTestHelper.FALLBACK_TEXT;
import static org.chromium.content.browser.input.StylusTestHelper.assertMojoRectsAreEqual;
import static org.chromium.content.browser.input.StylusTestHelper.createMojoRect;
import static org.chromium.content.browser.input.StylusTestHelper.toJavaString;

import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Build;
import android.view.inputmethod.DeleteGesture;
import android.view.inputmethod.DeleteRangeGesture;
import android.view.inputmethod.InsertGesture;
import android.view.inputmethod.JoinOrSplitGesture;
import android.view.inputmethod.RemoveSpaceGesture;
import android.view.inputmethod.SelectGesture;
import android.view.inputmethod.SelectRangeGesture;

import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;

/**
 * Tests for the StylusGestureConverter class which converts stylus gestures from their Android
 * representation to their Blink representation. These tests construct gesture objects and use the
 * converter to convert them into gesture data. The gesture data is then checked for accuracy.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({"enable-features=StylusRichGestures"})
@MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class StylusGestureConverterTest {
    private static final String GESTURE_TYPE_HISTOGRAM = "InputMethod.StylusHandwriting.Gesture";

    @Test
    @SmallTest
    public void testSelectGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM, StylusGestureConverter.UmaGestureType.SELECT);
        SelectGesture gesture =
                new SelectGesture.Builder()
                        .setGranularity(GRANULARITY_CHARACTER)
                        .setSelectionArea(new RectF(0, 0, 10, 10))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.SELECT_TEXT, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(0, 5, 0, 0), gestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(10, 5, 0, 0), gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertNull(gestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testInsertGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM, StylusGestureConverter.UmaGestureType.INSERT);
        InsertGesture gesture =
                new InsertGesture.Builder()
                        .setTextToInsert("Foo")
                        .setInsertionPoint(new PointF(15, 31))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.ADD_SPACE_OR_TEXT, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(15, 31, 0, 0), gestureData.startRect);
        assertNull(gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertEquals("Foo", toJavaString(gestureData.textToInsert));
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testDeleteGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM, StylusGestureConverter.UmaGestureType.DELETE);
        DeleteGesture gesture =
                new DeleteGesture.Builder()
                        .setGranularity(GRANULARITY_WORD)
                        .setDeletionArea(new RectF(0, 0, 10, 10))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.DELETE_TEXT, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.WORD,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(0, 5, 0, 0), gestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(10, 5, 0, 0), gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertNull(gestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testRemoveSpaceGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM, StylusGestureConverter.UmaGestureType.REMOVE_SPACE);
        RemoveSpaceGesture gesture =
                new RemoveSpaceGesture.Builder()
                        .setPoints(new PointF(51, 25), new PointF(105, 30))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.REMOVE_SPACES, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(51, 25, 0, 0), gestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(105, 30, 0, 0), gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertNull(gestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testJoinOrSplitGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM,
                        StylusGestureConverter.UmaGestureType.JOIN_OR_SPLIT);
        JoinOrSplitGesture gesture =
                new JoinOrSplitGesture.Builder()
                        .setJoinOrSplitPoint(new PointF(1, 19))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.SPLIT_OR_MERGE, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(1, 19, 0, 0), gestureData.startRect);
        assertNull(gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertNull(gestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testSelectRangeGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM, StylusGestureConverter.UmaGestureType.SELECT_RANGE);
        SelectRangeGesture gesture =
                new SelectRangeGesture.Builder()
                        .setGranularity(GRANULARITY_WORD)
                        .setSelectionStartArea(new RectF(10, 10, 45, 55))
                        .setSelectionEndArea(new RectF(0, 100, 70, 200))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.SELECT_TEXT, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.WORD,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(10, 10, 35, 45), gestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(0, 100, 70, 100), gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertNull(gestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testDeleteRangeGesture() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        GESTURE_TYPE_HISTOGRAM, StylusGestureConverter.UmaGestureType.DELETE_RANGE);
        DeleteRangeGesture gesture =
                new DeleteRangeGesture.Builder()
                        .setGranularity(GRANULARITY_CHARACTER)
                        .setDeletionStartArea(new RectF(10, 10, 45, 55))
                        .setDeletionEndArea(new RectF(0, 100, 70, 200))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();
        StylusWritingGestureData gestureData = StylusGestureConverter.createGestureData(gesture);

        assertNotNull(gestureData);
        assertEquals(StylusWritingGestureAction.DELETE_TEXT, gestureData.action);
        assertEquals(
                org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                gestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(10, 10, 35, 45), gestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(0, 100, 70, 100), gestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(gestureData.textAlternative));
        assertNull(gestureData.textToInsert);
        histogram.assertExpected();
    }
}
