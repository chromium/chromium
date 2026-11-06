// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.time_zone_monitor;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.os.HandlerThread;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link TimeZoneMonitor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimeZoneMonitorTest {
    private static final long NATIVE_PTR = 123456789L;
    private static final String TEST_TIME_ZONE = "America/Los_Angeles";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TimeZoneMonitor.Natives mNativeMock;

    private HandlerThread mUiThread;

    @Before
    public void setUp() {
        TimeZoneMonitorJni.setInstanceForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        if (mUiThread != null) {
            mUiThread.quitSafely();
        }
    }

    @Test
    public void testBroadcastDeliveredOnBackgroundUiThread() {
        mUiThread = new HandlerThread("TestUiThread");
        mUiThread.start();
        ThreadUtils.clearUiThreadForTesting();
        ThreadUtils.setUiThread(mUiThread.getLooper());

        AtomicReference<Looper> callbackLooper = new AtomicReference<>();
        doAnswer(
                        invocation -> {
                            callbackLooper.set(Looper.myLooper());
                            return null;
                        })
                .when(mNativeMock)
                .timeZoneChangedFromJava(NATIVE_PTR, TEST_TIME_ZONE);

        TimeZoneMonitor monitor = TimeZoneMonitor.getInstance(NATIVE_PTR);

        Intent intent = new Intent(Intent.ACTION_TIMEZONE_CHANGED);
        intent.putExtra(Intent.EXTRA_TIMEZONE, TEST_TIME_ZONE);
        ContextUtils.getApplicationContext().sendBroadcast(intent);

        // Drain the main looper first; the broadcast must not have been delivered on it.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        verify(mNativeMock, never()).timeZoneChangedFromJava(anyLong(), anyString());

        // Drain the background UI thread looper; the broadcast should be delivered there.
        Shadows.shadowOf(mUiThread.getLooper()).idle();
        verify(mNativeMock).timeZoneChangedFromJava(NATIVE_PTR, TEST_TIME_ZONE);
        assertEquals(mUiThread.getLooper(), callbackLooper.get());

        monitor.stop();
    }

    @Test
    public void testStopUnregistersReceiver() {
        TimeZoneMonitor monitor = TimeZoneMonitor.getInstance(NATIVE_PTR);
        monitor.stop();

        Intent intent = new Intent(Intent.ACTION_TIMEZONE_CHANGED);
        intent.putExtra(Intent.EXTRA_TIMEZONE, TEST_TIME_ZONE);
        ContextUtils.getApplicationContext().sendBroadcast(intent);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mNativeMock, never()).timeZoneChangedFromJava(anyLong(), anyString());
    }
}
