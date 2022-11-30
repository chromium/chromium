// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vibration;

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Vibrator;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.device.mojom.VibrationManager;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Android implementation of the VibrationManager interface defined in
 * services/device/public/mojom/vibration_manager.mojom.
 */
@JNINamespace("device")
public class VibrationManagerImpl implements VibrationManager {
    private static final String TAG = "VibrationManagerImpl";

    private static final long MINIMUM_VIBRATION_DURATION_MS = 1; // 1 millisecond
    private static final long MAXIMUM_VIBRATION_DURATION_MS = 10000; // 10 seconds

    private final AudioManager mAudioManager;
    private final Vibrator mVibrator;
    private final boolean mHasVibratePermission;

    private static long sVibrateMilliSecondsForTesting = -1;
    private static boolean sVibrateCancelledForTesting;

    public VibrationManagerImpl() {
        Context appContext = ContextUtils.getApplicationContext();
        mAudioManager = (AudioManager) appContext.getSystemService(Context.AUDIO_SERVICE);
        mVibrator = (Vibrator) appContext.getSystemService(Context.VIBRATOR_SERVICE);
        // TODO(mvanouwerkerk): What happens if permission is revoked? Handle this better.
        mHasVibratePermission =
                appContext.checkCallingOrSelfPermission(android.Manifest.permission.VIBRATE)
                == PackageManager.PERMISSION_GRANTED;
        if (!mHasVibratePermission) {
            Log.w(TAG, "Failed to use vibrate API, requires VIBRATE permission.");
        }
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    @Override
    public void vibrate(long milliseconds, Vibrate_Response callback) {
        // Though the Blink implementation already sanitizes vibration times, don't
        // trust any values passed from the client.
        long sanitizedMilliseconds = Math.max(MINIMUM_VIBRATION_DURATION_MS,
                Math.min(milliseconds, MAXIMUM_VIBRATION_DURATION_MS));

        if (mAudioManager.getRingerMode() != AudioManager.RINGER_MODE_SILENT
                && mHasVibratePermission) {
            mVibrator.vibrate(sanitizedMilliseconds);
        }
        setVibrateMilliSecondsForTesting(sanitizedMilliseconds);
        callback.call();
    }

    @Override
    public void cancel(Cancel_Response callback) {
        if (mHasVibratePermission) {
            mVibrator.cancel();
        }
        setVibrateCancelledForTesting(true);
        callback.call();
    }

    /**
     * A factory for implementations of the VibrationManager interface.
     */
    public static class Factory implements InterfaceFactory<VibrationManager> {
        public Factory() {}

        @Override
        public VibrationManager createImpl() {
            return new VibrationManagerImpl();
        }
    }

    static void setVibrateMilliSecondsForTesting(long milliseconds) {
        sVibrateMilliSecondsForTesting = milliseconds;
    }

    static void setVibrateCancelledForTesting(boolean cancelled) {
        sVibrateCancelledForTesting = cancelled;
    }

    @CalledByNative
    static long getVibrateMilliSecondsForTesting() {
        return sVibrateMilliSecondsForTesting;
    }

    @CalledByNative
    static boolean getVibrateCancelledForTesting() {
        return sVibrateCancelledForTesting;
    }
}
