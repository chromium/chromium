// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.HandwritingGestureResult;

/**
 * Tests for the OngoingGesture helper class which is used in association with
 * StylusGestureConverter to process and apply stylus gestures in Chrome and WebView.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OngoingGestureTest {
    private static final String GESTURE_RESULT_HISTOGRAM =
            "InputMethod.StylusHandwriting.GestureResult";

    private static OngoingGesture makeOngoingGesture() {
        return new OngoingGesture(
                new org.chromium.blink.mojom.StylusWritingGestureData(),
                (command) -> {},
                (value) -> {});
    }

    @Test
    @SmallTest
    public void testGestureRequestsHaveIncreasingIDs() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OngoingGesture baseline = makeOngoingGesture();
                    OngoingGesture request1 = makeOngoingGesture();
                    OngoingGesture request2 = makeOngoingGesture();
                    assertEquals(baseline.getId() + 1, request1.getId());
                    assertEquals(baseline.getId() + 2, request2.getId());
                });
    }

    @Test
    @SmallTest
    public void testGestureRequestLogsCorrectResult() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OngoingGesture request = makeOngoingGesture();

                    var histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.SUCCESS);
                    request.onGestureHandled(HandwritingGestureResult.SUCCESS);
                    histogram.assertExpected();

                    histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.FAILED);
                    request.onGestureHandled(HandwritingGestureResult.FAILED);
                    histogram.assertExpected();

                    histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    GESTURE_RESULT_HISTOGRAM, HandwritingGestureResult.FALLBACK);
                    request.onGestureHandled(HandwritingGestureResult.FALLBACK);
                    histogram.assertExpected();
                });
    }
}
