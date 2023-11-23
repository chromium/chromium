// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Forwards synthetic events to MotionEventSynthesizer. Owned by its native. */
@JNINamespace("content")
public class SyntheticGestureTarget {
    private final MotionEventSynthesizerImpl mMotionEventSynthesizer;

    @CalledByNative
    private static SyntheticGestureTarget create(View target) {
        return new SyntheticGestureTarget(target);
    }

    private SyntheticGestureTarget(View target) {
        mMotionEventSynthesizer = MotionEventSynthesizerImpl.create(target);
    }

    @CalledByNative
    private void inject(int action, int pointerCount, int pointerIndex, long timeInMs) {
        mMotionEventSynthesizer.inject(action, pointerCount, pointerIndex, timeInMs);
    }

    @CalledByNative
    private void setPointer(int index, float x, float y, int id) {
        mMotionEventSynthesizer.setPointer(index, x, y, id);
    }

    @CalledByNative
    private void setScrollDeltas(float x, float y, float dx, float dy) {
        mMotionEventSynthesizer.setScrollDeltas(x, y, dx, dy);
    }
}
