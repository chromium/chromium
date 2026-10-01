// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.power_save_blocker;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.device.mojom.WakeLockType;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

@JNINamespace("device")
@NullMarked
class PowerSaveBlocker {
    private static final String TAG = "PowerSaveBlocker";
    private static final String WAKE_LOCK_TAG = "chromium:PowerSaveBlocker";

    // Counter associated to a view to know how many PowerSaveBlocker are
    // currently registered. Using WeakHashMap to prevent leaks in Android WebView.
    private static final WeakHashMap<View, Integer> sBlockViewCounter =
            new WeakHashMap<View, Integer>();

    // WeakReference to prevent leaks in Android WebView.
    private @Nullable WeakReference<View> mKeepScreenOnView;

    private @Nullable WakeLock mWakeLock;

    @CalledByNative
    static PowerSaveBlocker create() {
        return new PowerSaveBlocker();
    }

    private PowerSaveBlocker() {}

    @CalledByNative
    void applyBlock(View view) {
        assert mKeepScreenOnView == null;
        mKeepScreenOnView = new WeakReference<>(view);

        Integer prevCounter = sBlockViewCounter.get(view);

        if (prevCounter == null) {
            sBlockViewCounter.put(view, 1);
        } else {
            assert prevCounter >= 0;
            sBlockViewCounter.put(view, prevCounter + 1);
        }

        if (prevCounter == null || prevCounter == 0) view.setKeepScreenOn(true);
    }

    @CalledByNative
    @SuppressWarnings("deprecation")
    @SuppressLint("WakelockTimeout")
    void applyWakeLock(@WakeLockType.EnumType int type) {
        assert mWakeLock == null;
        Context context = ContextUtils.getApplicationContext();
        if (context == null) {
            Log.w(TAG, "Failed to acquire wake lock, no application context.");
            return;
        }
        if (context.checkSelfPermission(Manifest.permission.WAKE_LOCK)
                != PackageManager.PERMISSION_GRANTED) {
            Log.w(TAG, "Failed to acquire wake lock, requires WAKE_LOCK permission.");
            return;
        }

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (powerManager == null) return;

        // The screen levels are deprecated in favour of FLAG_KEEP_SCREEN_ON, which
        // needs a View. A contextless wake lock has none.
        int levelAndFlags;
        switch (type) {
            case WakeLockType.PREVENT_APP_SUSPENSION:
                levelAndFlags = PowerManager.PARTIAL_WAKE_LOCK;
                break;
            case WakeLockType.PREVENT_DISPLAY_SLEEP:
                levelAndFlags =
                        PowerManager.SCREEN_BRIGHT_WAKE_LOCK | PowerManager.ON_AFTER_RELEASE;
                break;
            case WakeLockType.PREVENT_DISPLAY_SLEEP_ALLOW_DIMMING:
                levelAndFlags = PowerManager.SCREEN_DIM_WAKE_LOCK | PowerManager.ON_AFTER_RELEASE;
                break;
            default:
                assert false : "Unhandled WakeLockType: " + type;
                return;
        }

        mWakeLock = powerManager.newWakeLock(levelAndFlags, WAKE_LOCK_TAG);
        mWakeLock.setReferenceCounted(false);
        mWakeLock.acquire();
    }

    @CalledByNative
    void removeBlock() {
        if (mWakeLock != null) {
            if (mWakeLock.isHeld()) {
                mWakeLock.release();
            }
            mWakeLock = null;
            return;
        }

        // mKeepScreenOnView may be null since it's possible that |applyBlock()| was
        // not invoked due to having failed to get a view to call |setKeepScreenOn| on.
        if (mKeepScreenOnView == null) return;

        View view = mKeepScreenOnView.get();
        mKeepScreenOnView = null;

        // View has been garbage collected. No need to worry about clean up.
        if (view == null) return;

        Integer prevCounter = sBlockViewCounter.get(view);
        assert prevCounter != null;
        assert prevCounter > 0;
        sBlockViewCounter.put(view, prevCounter - 1);

        if (prevCounter == 1) view.setKeepScreenOn(false);
    }
}
