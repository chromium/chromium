// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Provides utility methods relating to measuring accessibility state on the current platform (i.e.
 * Android in this case). See content::BrowserAccessibilityStateImpl.
 */
@JNINamespace("content")
public class BrowserAccessibilityState {
    /* The ANIMATIONS_STATE_* enumerations are used to measure whether or not animations have been
     * disabled via the ANIAMTOR_DURATION_SCALE API. Values must be kept in sync with thier
     * definition in //tools/metrics/histograms/histograms.xml, and both the numbering
     * and meaning of the values must remain constant as they're recorded by UMA.
     *
     * These are visible for BrowserAccessibilityStateTest.
     */
    static final int ANIMATIONS_STATE_DEFAULT_VALUE = 0;
    static final int ANIMATIONS_STATE_DISABLED = 1;
    static final int ANIMATIONS_STATE_ENABLED = 2;
    static final int ANIMATIONS_STATE_COUNT = ANIMATIONS_STATE_ENABLED + 1;

    private static class AnimatorDurationScaleObserver extends ContentObserver {
        public AnimatorDurationScaleObserver(Handler handler) {
            super(handler);
        }

        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        @Override
        public void onChange(boolean selfChange, Uri uri) {
            assert ThreadUtils.runningOnUiThread();
            BrowserAccessibilityStateJni.get().onAnimatorDurationScaleChanged();
        }
    }

    @CalledByNative
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    static void registerAnimatorDurationScaleObserver() {
        Handler handler = new Handler(ThreadUtils.getUiThreadLooper());
        Uri uri = Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE);
        ContextUtils.getApplicationContext().getContentResolver().registerContentObserver(
                uri, false, new AnimatorDurationScaleObserver(handler));
    }

    @CalledByNative
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    @VisibleForTesting
    static void recordAccessibilityHistograms() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) return;

        float durationScale =
                Settings.Global.getFloat(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, -1);
        int histogramValue = durationScale < 0
                ? ANIMATIONS_STATE_DEFAULT_VALUE
                : (durationScale > 0 ? ANIMATIONS_STATE_ENABLED : ANIMATIONS_STATE_DISABLED);
        RecordHistogram.recordEnumeratedHistogram(
                "Accessibility.Android.AnimationsEnabled2", histogramValue, ANIMATIONS_STATE_COUNT);
    }

    @NativeMethods
    interface Natives {
        void onAnimatorDurationScaleChanged();
    }
}
