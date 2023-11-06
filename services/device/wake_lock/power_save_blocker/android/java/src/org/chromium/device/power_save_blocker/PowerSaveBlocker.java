// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.power_save_blocker;

import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

@JNINamespace("device")
class PowerSaveBlocker {
    // Counter associated to a view to know how many PowerSaveBlocker are
    // currently registered. Using WeakHashMap to prevent leaks in Android WebView.
    private static final WeakHashMap<View, Integer> sBlockViewCounter =
            new WeakHashMap<View, Integer>();

    // WeakReference to prevent leaks in Android WebView.
    private WeakReference<View> mKeepScreenOnView;

    @CalledByNative
    private static PowerSaveBlocker create() {
        return new PowerSaveBlocker();
    }

    private PowerSaveBlocker() {}

    @CalledByNative
    private void applyBlock(View view) {
        assert mKeepScreenOnView == null;
        mKeepScreenOnView = new WeakReference<>(view);

        Integer prev_counter = sBlockViewCounter.get(view);

        if (prev_counter == null) {
            sBlockViewCounter.put(view, 1);
        } else {
            assert prev_counter >= 0;
            sBlockViewCounter.put(view, prev_counter + 1);
        }

        if (prev_counter == null || prev_counter == 0) view.setKeepScreenOn(true);
    }

    @CalledByNative
    private void removeBlock() {
        // mKeepScreenOnView may be null since it's possible that |applyBlock()| was
        // not invoked due to having failed to get a view to call |setKeepScrenOn| on.
        if (mKeepScreenOnView == null) return;

        View view = mKeepScreenOnView.get();
        mKeepScreenOnView = null;

        // View has been garbage collected. No need to worry about clean up.
        if (view == null) return;

        Integer prev_counter = sBlockViewCounter.get(view);
        assert prev_counter != null;
        assert prev_counter > 0;
        sBlockViewCounter.put(view, prev_counter - 1);

        if (prev_counter == 1) view.setKeepScreenOn(false);
    }
}
