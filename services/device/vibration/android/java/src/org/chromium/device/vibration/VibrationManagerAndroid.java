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
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Android implementation details for device::VibrationManagerAndroid. */
@JNINamespace("device")
public class VibrationManagerAndroid {
    private static final String TAG = "VibrationManager";

    private static final long MINIMUM_VIBRATION_DURATION_MS = 1; // 1 millisecond
    private static final long MAXIMUM_VIBRATION_DURATION_MS = 10000; // 10 seconds

    private final AudioManager mAudioManager;
    private final Vibrator mVibrator;
    private final boolean mHasVibratePermission;

    public VibrationManagerAndroid() {
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

    @CalledByNative
    static VibrationManagerAndroid getInstance() {
        return new VibrationManagerAndroid();
    }

    @CalledByNative
    public void vibrate(long milliseconds) {
        // Though the Blink implementation already sanitizes vibration times, don't
        // trust any values passed from the client.
        long sanitizedMilliseconds =
                Math.max(
                        MINIMUM_VIBRATION_DURATION_MS,
                        Math.min(milliseconds, MAXIMUM_VIBRATION_DURATION_MS));

        if (mAudioManager.getRingerMode() != AudioManager.RINGER_MODE_SILENT
                && mHasVibratePermission) {
            mVibrator.vibrate(sanitizedMilliseconds);
        }
    }

    @CalledByNative
    public void cancel() {
        if (mHasVibratePermission) {
            mVibrator.cancel();
        }
    }
}
