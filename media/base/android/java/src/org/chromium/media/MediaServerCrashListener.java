// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaPlayer;
import android.os.SystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Class for listening to Android MediaServer crashes to throttle media decoding
 * when needed.
 */
@JNINamespace("media")
public class MediaServerCrashListener implements MediaPlayer.OnErrorListener {
    private static final String TAG = "crMediaCrashListener";
    private static final long UNKNOWN_TIME = -1;

    // Watchdog player. Used to listen to all media server crashes.
    private MediaPlayer mPlayer;

    // Protecting the creation/release of the watchdog player.
    private final Object mLock = new Object();

    // Approximate time necessary for the MediaServer to restart after a crash.
    private static final int APPROX_MEDIA_SERVER_RESTART_TIME_IN_MS = 5000;

    // The last time we reported a failure to create the watchdog as a server crash.
    private long mLastReportedWatchdogCreationFailure = UNKNOWN_TIME;

    private long mNativeMediaServerCrashListener;

    @CalledByNative
    private static MediaServerCrashListener create(long nativeMediaServerCrashListener) {
        return new MediaServerCrashListener(nativeMediaServerCrashListener);
    }

    private MediaServerCrashListener(long nativeMediaServerCrashListener) {
        mNativeMediaServerCrashListener = nativeMediaServerCrashListener;
    }

    @CalledByNative
    public void releaseWatchdog() {
        if (mPlayer == null) return;

        mPlayer.release();
        mPlayer = null;
    }

    @CalledByNative
    public boolean startListening() {
        if (mPlayer != null) return true;

        try {
            mPlayer = MediaPlayer.create(ContextUtils.getApplicationContext(), R.raw.empty);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Exception while creating the watchdog player.", e);
        } catch (RuntimeException e) {
            Log.e(TAG, "Exception while creating the watchdog player.", e);
        }

        if (mPlayer != null) {
            mPlayer.setOnErrorListener(MediaServerCrashListener.this);

            // Reset the reported creation failure time on successful
            // watchdog creation.
            mLastReportedWatchdogCreationFailure = UNKNOWN_TIME;
            return true;
        }

        long currentTime = SystemClock.elapsedRealtime();

        // It takes ~5s for the MediaServer to restart. Do not report a
        // failure to create a watchdog MediaPlayer as a crash more than
        // once per 5s, to prevent a burst of calls to startListening() from
        // artificially inflating the number of crashes.
        if (mLastReportedWatchdogCreationFailure == UNKNOWN_TIME
                || (currentTime - mLastReportedWatchdogCreationFailure)
                        > APPROX_MEDIA_SERVER_RESTART_TIME_IN_MS) {
            Log.e(TAG, "Unable to create watchdog player, treating it as server crash.");
            MediaServerCrashListenerJni.get().onMediaServerCrashDetected(
                    mNativeMediaServerCrashListener, MediaServerCrashListener.this, false);
            mLastReportedWatchdogCreationFailure = currentTime;
        }
        return false;
    }

    @Override
    public boolean onError(MediaPlayer mp, int what, int extra) {
        if (what == MediaPlayer.MEDIA_ERROR_SERVER_DIED) {
            MediaServerCrashListenerJni.get().onMediaServerCrashDetected(
                    mNativeMediaServerCrashListener, MediaServerCrashListener.this, true);
            releaseWatchdog();
        }
        return true;
    }

    @NativeMethods
    interface Natives {
        void onMediaServerCrashDetected(long nativeMediaServerCrashListener,
                MediaServerCrashListener caller, boolean watchdogNeedsRelease);
    }
}
