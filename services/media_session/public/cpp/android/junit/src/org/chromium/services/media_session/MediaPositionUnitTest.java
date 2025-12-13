// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.media_session;

import static org.junit.Assert.assertEquals;

import android.os.SystemClock;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link MediaPosition}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {MediaPositionUnitTest.ShadowSystemClock.class})
public class MediaPositionUnitTest {

    /** Custom shadow to allow independent control of uptimeMillis and elapsedRealtime. */
    @Implements(SystemClock.class)
    public static class ShadowSystemClock {
        private static long sUptimeMillis;
        private static long sElapsedRealtime;

        @Implementation
        public static long uptimeMillis() {
            return sUptimeMillis;
        }

        @Implementation
        public static long elapsedRealtime() {
            return sElapsedRealtime;
        }

        public static void setUptimeMillis(long millis) {
            sUptimeMillis = millis;
        }

        public static void setElapsedRealtime(long millis) {
            sElapsedRealtime = millis;
        }
    }

    @Test
    public void testCreate_NormalPlayback() {
        // Initial state
        long initialPosition = 10000;
        long initialUptime = 50000;
        long initialElapsedRealtime = 60000;

        ShadowSystemClock.setUptimeMillis(initialUptime);
        ShadowSystemClock.setElapsedRealtime(initialElapsedRealtime);

        // Simulate C++ sending uptimeMillis
        MediaPosition mediaPosition =
                MediaPosition.create(100000, initialPosition, 1.0f, initialUptime);

        // Since update time == current time, the position's last updated time should match
        // elapsedRealtime.
        assertEquals(initialElapsedRealtime, mediaPosition.getLastUpdatedTime());

        // Simulate 5 seconds passing (no sleep)
        long elapsed = 5000;
        long newUptime = initialUptime + elapsed;
        long newElapsedRealtime = initialElapsedRealtime + elapsed;

        ShadowSystemClock.setUptimeMillis(newUptime);
        ShadowSystemClock.setElapsedRealtime(newElapsedRealtime);

        MediaPosition updatedPosition =
                MediaPosition.create(100000, initialPosition + elapsed, 1.0f, newUptime);
        assertEquals(newElapsedRealtime, updatedPosition.getLastUpdatedTime());
    }

    @Test
    public void testCreate_WithDeepSleep() {
        // Initial state
        long initialPosition = 10000;
        long initialUptime = 50000;
        long initialElapsedRealtime = 60000;

        ShadowSystemClock.setUptimeMillis(initialUptime);
        ShadowSystemClock.setElapsedRealtime(initialElapsedRealtime);

        // Simulate C++ sending uptimeMillis (initialUptime)
        MediaPosition mediaPosition =
                MediaPosition.create(100000, initialPosition, 1.0f, initialUptime);
        assertEquals(initialElapsedRealtime, mediaPosition.getLastUpdatedTime());

        // Simulate deep sleep for 10 seconds:
        // uptimeMillis remains the same, but elapsedRealtime advances.
        long sleepDuration = 10000;
        long afterSleepUptime = initialUptime; // Paused
        long afterSleepElapsedRealtime = initialElapsedRealtime + sleepDuration;

        // Advance system clocks to post-sleep state
        ShadowSystemClock.setUptimeMillis(afterSleepUptime);
        ShadowSystemClock.setElapsedRealtime(afterSleepElapsedRealtime);

        // Simulate a new update arriving from C++ after sleep.
        // C++ clock (TimeTicks) paused during sleep, so it has only advanced by active time (e.g.
        // 5s).
        long activeTimeAfterSleep = 5000;
        long newUptimeFromCpp = afterSleepUptime + activeTimeAfterSleep;

        // System state also advances by this active time
        ShadowSystemClock.setUptimeMillis(afterSleepUptime + activeTimeAfterSleep);
        ShadowSystemClock.setElapsedRealtime(afterSleepElapsedRealtime + activeTimeAfterSleep);

        MediaPosition updatedMediaPosition =
                MediaPosition.create(
                        100000, initialPosition + activeTimeAfterSleep, 1.0f, newUptimeFromCpp);

        // The expected LastUpdatedTime should match the current system elapsedRealtime.
        long expectedElapsedRealtime = afterSleepElapsedRealtime + activeTimeAfterSleep;
        assertEquals(expectedElapsedRealtime, updatedMediaPosition.getLastUpdatedTime());
    }

    @Test
    public void testCreate_WithLatency() {
        long initialPosition = 10000;
        long initialUptime = 50000;
        long initialElapsedRealtime = 60000;

        // System has advanced slightly beyond the time of the update (simulating IPC latency)
        long latency = 50;
        ShadowSystemClock.setUptimeMillis(initialUptime + latency);
        ShadowSystemClock.setElapsedRealtime(initialElapsedRealtime + latency);

        // C++ sends the time of the update (which was 'latency' ms ago)
        MediaPosition mediaPosition =
                MediaPosition.create(100000, initialPosition, 1.0f, initialUptime);

        // The calculated lastUpdatedTime should still refer to the moment the update happened (in
        // elapsedRealtime).
        assertEquals(initialElapsedRealtime, mediaPosition.getLastUpdatedTime());
    }
}
