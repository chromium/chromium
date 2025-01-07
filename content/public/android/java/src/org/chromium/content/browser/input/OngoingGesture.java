// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.HandwritingGestureResult;
import org.chromium.blink.mojom.StylusWritingGestureData;

import java.util.concurrent.Executor;
import java.util.function.IntConsumer;

/**
 * Stores data needed to process and record the result of a gesture, reporting it to Android.
 * Also records how long it took to process the gesture.
 */
class OngoingGesture {
    private static int sLastId;

    private final int mId;
    private final @Nullable StylusWritingGestureData mGestureData;
    private final @Nullable Executor mExecutor;
    private final @Nullable IntConsumer mConsumer;
    private final long mCreationTimestamp;

    OngoingGesture(
            @Nullable StylusWritingGestureData gestureData,
            @Nullable Executor executor,
            @Nullable IntConsumer consumer) {
        ThreadUtils.assertOnUiThread();
        mId = ++sLastId;
        mGestureData = gestureData;
        mExecutor = executor;
        mConsumer = consumer;
        mCreationTimestamp = System.currentTimeMillis();
    }

    void onGestureHandled(@HandwritingGestureResult.EnumType int result) {
        if (mExecutor == null || mConsumer == null) {
            logGestureResult(HandwritingGestureResult.UNKNOWN);
            return;
        }
        mExecutor.execute(() -> mConsumer.accept(result));
        logGestureResult(result);

        long timeTaken = System.currentTimeMillis() - mCreationTimestamp;
        assert timeTaken >= 0;
        // Log time taken to handle gesture.
        // Expected range is from 0ms to 1000ms (1 second) with 50 buckets.
        RecordHistogram.recordCustomTimesHistogram(
                "InputMethod.StylusHandwriting.GestureTime2",
                timeTaken,
                /* min= */ 1L,
                /* max= */ 250L,
                /* numBuckets= */ 50);
    }

    int getId() {
        return mId;
    }

    @Nullable
    StylusWritingGestureData getGestureData() {
        return mGestureData;
    }

    private static void logGestureResult(@HandwritingGestureResult.EnumType int gestureResult) {
        RecordHistogram.recordEnumeratedHistogram(
                "InputMethod.StylusHandwriting.GestureResult",
                gestureResult,
                HandwritingGestureResult.MAX_VALUE);
    }
}
