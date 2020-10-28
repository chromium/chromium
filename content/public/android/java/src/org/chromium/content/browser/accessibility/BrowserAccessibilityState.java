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

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides utility methods relating to measuring accessibility state on the current platform (i.e.
 * Android in this case). See content::BrowserAccessibilityStateImpl.
 */
@JNINamespace("content")
public class BrowserAccessibilityState {
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

    @NativeMethods
    interface Natives {
        void onAnimatorDurationScaleChanged();
    }
}
