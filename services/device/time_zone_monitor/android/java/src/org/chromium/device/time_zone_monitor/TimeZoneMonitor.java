// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.time_zone_monitor;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/** Android implementation details for device::TimeZoneMonitorAndroid. */
@JNINamespace("device")
class TimeZoneMonitor {
    private static final String TAG = "TimeZoneMonitor";

    private final IntentFilter mFilter = new IntentFilter(Intent.ACTION_TIMEZONE_CHANGED);
    private final BroadcastReceiver mBroadcastReceiver =
            new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (!intent.getAction().equals(Intent.ACTION_TIMEZONE_CHANGED)) {
                        Log.e(TAG, "unexpected intent");
                        return;
                    }

                    TimeZoneMonitorJni.get()
                            .timeZoneChangedFromJava(mNativePtr, TimeZoneMonitor.this);
                }
            };

    private long mNativePtr;

    /**
     * Start listening for intents.
     * @param nativePtr The native device::TimeZoneMonitorAndroid to notify of time zone changes.
     */
    private TimeZoneMonitor(long nativePtr) {
        mNativePtr = nativePtr;
        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), mBroadcastReceiver, mFilter);
    }

    @CalledByNative
    static TimeZoneMonitor getInstance(long nativePtr) {
        return new TimeZoneMonitor(nativePtr);
    }

    /** Stop listening for intents. */
    @CalledByNative
    void stop() {
        ContextUtils.getApplicationContext().unregisterReceiver(mBroadcastReceiver);
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Native JNI call to device::TimeZoneMonitorAndroid::TimeZoneChanged. See
         * device/time_zone_monitor/time_zone_monitor_android.cc.
         */
        void timeZoneChangedFromJava(long nativeTimeZoneMonitorAndroid, TimeZoneMonitor caller);
    }
}
