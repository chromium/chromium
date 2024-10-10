// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.media_session;

import android.os.SystemClock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * The MediaPosition class carries the position information.
 * It is the counterpart of media_session::MediaImage.
 */
@JNINamespace("media_session")
public final class MediaPosition {
    private Long mDuration;

    private Long mPosition;

    private Float mPlaybackRate;

    private Long mLastUpdatedTime;

    /** Creates a new MediaPosition. */
    public MediaPosition(long duration, long position, float playbackRate, long lastUpdatedTime) {
        mDuration = duration;
        mPosition = position;
        mPlaybackRate = playbackRate;
        mLastUpdatedTime = lastUpdatedTime;
    }

    /**
     * @return The duration of the media in ms.
     */
    public long getDuration() {
        return mDuration;
    }

    /**
     * @return The position of the media in ms.
     */
    public long getPosition() {
        return mPosition;
    }

    /**
     * @return The playback rate of the media as a coefficient.
     */
    public float getPlaybackRate() {
        return mPlaybackRate;
    }

    /**
     * @return The time the position was last updated in ms relative to the
     * boot time.
     */
    public long getLastUpdatedTime() {
        return mLastUpdatedTime;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (!(obj instanceof MediaPosition)) return false;

        MediaPosition other = (MediaPosition) obj;
        return mDuration == other.getDuration()
                && mPosition == other.getPosition()
                && mPlaybackRate == other.getPlaybackRate()
                && mLastUpdatedTime == other.getLastUpdatedTime();
    }

    @Override
    public int hashCode() {
        int result = mDuration.hashCode();
        result = 31 * result + mPosition.hashCode();
        result = 31 * result + mPlaybackRate.hashCode();
        result = 31 * result + mLastUpdatedTime.hashCode();
        return result;
    }

    @Override
    public String toString() {
        return "duration="
                + mDuration
                + ", position="
                + mPosition
                + ", rate="
                + mPlaybackRate
                + ", updated="
                + mLastUpdatedTime;
    }

    /**
     * Create a new {@link MediaPosition} from the C++ code.
     *
     * @param duration The duration of the media in ms.
     * @param position The position of the media in ms.
     * @param playbackRate The playback rate of the media as a coefficient.
     * @param lastUpdatedTime The time the position was last updated in ms (epoch time).
     */
    @CalledByNative
    private static MediaPosition create(
            long duration, long position, float playbackRate, long lastUpdatedTime) {
        long currentTime = System.currentTimeMillis();
        long elapsedRealtime = SystemClock.elapsedRealtime();
        lastUpdatedTime -= (currentTime - elapsedRealtime);

        return new MediaPosition(duration, position, playbackRate, lastUpdatedTime);
    }
}
