// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.blink.mojom.HandwritingGestureResult;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the OngoingGesture helper class which is used in association with StylusGestureHandler
 * to process and apply stylus gestures in Chrome and WebView.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class OngoingGestureTest {
    private static final String GESTURE_RESULT_HISTOGRAM =
            "InputMethod.StylusHandwriting.GestureResult";

    @Rule
    public HistogramTestRule mHistogramTester = new HistogramTestRule();

    @BeforeClass
    public static void setUpClass() {
        // Needed for HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testGestureRequestsHaveIncreasingIDs() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OngoingGesture baseline = new OngoingGesture(null, null, null);
            OngoingGesture request1 = new OngoingGesture(null, null, null);
            OngoingGesture request2 = new OngoingGesture(null, null, null);
            assertEquals(baseline.getId() + 1, request1.getId());
            assertEquals(baseline.getId() + 2, request2.getId());
        });
    }

    @Test
    @SmallTest
    public void testGestureRequestLogsUnknownWithNullExecutor() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OngoingGesture request = new OngoingGesture(
                    new org.chromium.blink.mojom.StylusWritingGestureData(), null, (value) -> {});
            request.onGestureHandled(HandwritingGestureResult.SUCCESS);
            assertEquals(1,
                    mHistogramTester.getHistogramValueCount(
                            GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.UNKNOWN));
        });
    }

    @Test
    @SmallTest
    public void testGestureRequestLogsUnknownWithNullIntConsumer() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OngoingGesture request = new OngoingGesture(
                    new org.chromium.blink.mojom.StylusWritingGestureData(), (command) -> {}, null);
            request.onGestureHandled(HandwritingGestureResult.FAILED);
            assertEquals(1,
                    mHistogramTester.getHistogramValueCount(
                            GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.UNKNOWN));
        });
    }

    @Test
    @SmallTest
    public void testGestureRequestLogsCorrectResult() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OngoingGesture request =
                    new OngoingGesture(new org.chromium.blink.mojom.StylusWritingGestureData(),
                            (command) -> {}, (value) -> {});
            request.onGestureHandled(HandwritingGestureResult.SUCCESS);
            assertEquals(1,
                    mHistogramTester.getHistogramValueCount(
                            GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.SUCCESS));

            request.onGestureHandled(HandwritingGestureResult.FAILED);
            assertEquals(1,
                    mHistogramTester.getHistogramValueCount(
                            GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.FAILED));

            request.onGestureHandled(HandwritingGestureResult.FALLBACK);
            assertEquals(1,
                    mHistogramTester.getHistogramValueCount(
                            GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.FALLBACK));
        });
    }
}
