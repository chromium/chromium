// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.sensors;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

import androidx.annotation.GuardedBy;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.device.mojom.ReportingMode;
import org.chromium.device.mojom.SensorType;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.List;

/**
 * Implementation of PlatformSensor that uses Android Sensor Framework. Lifetime is controlled by
 * the device::PlatformSensorAndroid.
 */
@JNINamespace("device")
public class PlatformSensor implements SensorEventListener {
    private static final double MICROSECONDS_IN_SECOND = 1000000;
    private static final double SECONDS_IN_MICROSECOND = 0.000001d;
    private static final double SECONDS_IN_NANOSECOND = 0.000000001d;
    private static final String TAG = "PlatformSensor";

    /**
     * The SENSOR_FREQUENCY_NORMAL is defined as 5Hz which corresponds to a polling delay
     * @see android.hardware.SensorManager.SENSOR_DELAY_NORMAL value that is defined as 200000
     * microseconds.
     */
    private static final double SENSOR_FREQUENCY_NORMAL = 5.0d;

    /** Lock protecting access to mNativePlatformSensorAndroid. */
    private final Object mLock = new Object();

    /** Identifier of device::PlatformSensorAndroid instance. */
    @GuardedBy("mLock")
    private long mNativePlatformSensorAndroid;

    /**
     * Used for fetching sensor reading values and obtaining information about the sensor.
     * @see android.hardware.Sensor
     */
    private final Sensor mSensor;

    /** The minimum delay between two readings in microseconds that is supported by the sensor. */
    private final int mMinDelayUsec;

    /** The number of sensor reading values required from the sensor. */
    private final int mReadingCount;

    /** Frequency that is currently used by the sensor for polling. */
    private double mCurrentPollingFrequency;

    /**
     * Provides shared SensorManager and event processing thread Handler to PlatformSensor objects.
     */
    private final PlatformSensorProvider mProvider;

    /**
     * Creates new PlatformSensor.
     *
     * @param provider object that shares SensorManager and polling thread Handler with sensors.
     * @param type type of the sensor to be constructed. @see android.hardware.Sensor.TYPE_*
     * @param nativePlatformSensorAndroid identifier of device::PlatformSensorAndroid instance.
     */
    @CalledByNative
    public static PlatformSensor create(
            PlatformSensorProvider provider, int type, long nativePlatformSensorAndroid) {
        SensorManager sensorManager = provider.getSensorManager();
        if (sensorManager == null) return null;

        int sensorType;
        int readingCount;
        switch (type) {
            case SensorType.AMBIENT_LIGHT:
                sensorType = Sensor.TYPE_LIGHT;
                readingCount = 1;
                break;
            case SensorType.ACCELEROMETER:
                sensorType = Sensor.TYPE_ACCELEROMETER;
                readingCount = 3;
                break;
            case SensorType.LINEAR_ACCELERATION:
                sensorType = Sensor.TYPE_LINEAR_ACCELERATION;
                readingCount = 3;
                break;
            case SensorType.GRAVITY:
                sensorType = Sensor.TYPE_GRAVITY;
                readingCount = 3;
                break;
            case SensorType.GYROSCOPE:
                sensorType = Sensor.TYPE_GYROSCOPE;
                readingCount = 3;
                break;
            case SensorType.MAGNETOMETER:
                sensorType = Sensor.TYPE_MAGNETIC_FIELD;
                readingCount = 3;
                break;
            case SensorType.ABSOLUTE_ORIENTATION_QUATERNION:
                sensorType = Sensor.TYPE_ROTATION_VECTOR;
                readingCount = 4;
                break;
            case SensorType.RELATIVE_ORIENTATION_QUATERNION:
                sensorType = Sensor.TYPE_GAME_ROTATION_VECTOR;
                readingCount = 4;
                break;
            default:
                return null;
        }

        List<Sensor> sensors = sensorManager.getSensorList(sensorType);
        if (sensors.isEmpty()) return null;
        return new PlatformSensor(
                sensors.get(0), readingCount, provider, nativePlatformSensorAndroid);
    }

    /** Constructor. */
    protected PlatformSensor(
            Sensor sensor,
            int readingCount,
            PlatformSensorProvider provider,
            long nativePlatformSensorAndroid) {
        mReadingCount = readingCount;
        mProvider = provider;
        mSensor = sensor;
        mNativePlatformSensorAndroid = nativePlatformSensorAndroid;
        mMinDelayUsec = mSensor.getMinDelay();
    }

    /**
     * Returns reporting mode supported by the sensor.
     *
     * @return ReportingMode reporting mode.
     */
    @CalledByNative
    protected int getReportingMode() {
        return mSensor.getReportingMode() == Sensor.REPORTING_MODE_CONTINUOUS
                ? ReportingMode.CONTINUOUS
                : ReportingMode.ON_CHANGE;
    }

    /**
     * Returns default configuration supported by the sensor. Currently only frequency is supported.
     *
     * @return double frequency.
     */
    @CalledByNative
    protected double getDefaultConfiguration() {
        return SENSOR_FREQUENCY_NORMAL;
    }

    /**
     * Returns maximum sampling frequency supported by the sensor.
     *
     * @return double frequency in Hz.
     */
    @CalledByNative
    protected double getMaximumSupportedFrequency() {
        if (mMinDelayUsec == 0) return getDefaultConfiguration();
        return 1 / (mMinDelayUsec * SECONDS_IN_MICROSECOND);
    }

    /** Requests sensor to start polling for data. */
    @CalledByNative
    protected void startSensor(double frequency) {
        // If we already polling hw with same frequency, do not restart the sensor.
        if (mCurrentPollingFrequency == frequency) return;

        // Unregister old listener if polling frequency has changed.
        unregisterListener();

        mProvider.sensorStarted(this);
        boolean sensorStarted;
        try {
            sensorStarted =
                    mProvider
                            .getSensorManager()
                            .registerListener(
                                    this,
                                    mSensor,
                                    getSamplingPeriod(frequency),
                                    mProvider.getHandler());
        } catch (RuntimeException e) {
            // This can fail due to internal framework errors. https://crbug.com/884190
            Log.w(TAG, "Failed to register sensor listener.", e);
            sensorStarted = false;
        }

        if (!sensorStarted) {
            stopSensor();
            synchronized (mLock) {
                sensorError();
            }
        } else {
            mCurrentPollingFrequency = frequency;
        }
    }

    private void unregisterListener() {
        // Do not unregister if current polling frequency is 0, not polling for data.
        if (mCurrentPollingFrequency == 0) return;
        mProvider.getSensorManager().unregisterListener(this, mSensor);
    }

    /** Requests sensor to stop polling for data. */
    @CalledByNative
    protected void stopSensor() {
        unregisterListener();
        mProvider.sensorStopped(this);
        mCurrentPollingFrequency = 0;
    }

    /**
     * Checks whether configuration is supported by the sensor. Currently only frequency is
     * supported.
     *
     * @return boolean true if configuration is supported, false otherwise.
     */
    @CalledByNative
    protected boolean checkSensorConfiguration(double frequency) {
        return mMinDelayUsec <= getSamplingPeriod(frequency);
    }

    /**
     * Called from device::PlatformSensorAndroid destructor, so that this instance would be
     * notified not to deliver any updates about new sensor readings or errors.
     */
    @CalledByNative
    protected void sensorDestroyed() {
        synchronized (mLock) {
            mNativePlatformSensorAndroid = 0;
        }
    }

    /** Converts frequency to sampling period in microseconds. */
    private int getSamplingPeriod(double frequency) {
        return (int) ((1 / frequency) * MICROSECONDS_IN_SECOND);
    }

    /** Notifies native device::PlatformSensorAndroid when there is an error. */
    @GuardedBy("mLock")
    protected void sensorError() {
        if (mNativePlatformSensorAndroid != 0) {
            PlatformSensorJni.get()
                    .notifyPlatformSensorError(mNativePlatformSensorAndroid, PlatformSensor.this);
        }
    }

    /** Updates reading at native device::PlatformSensorAndroid. */
    @GuardedBy("mLock")
    protected void updateSensorReading(
            double timestamp, double value1, double value2, double value3, double value4) {
        PlatformSensorJni.get()
                .updatePlatformSensorReading(
                        mNativePlatformSensorAndroid,
                        PlatformSensor.this,
                        timestamp,
                        value1,
                        value2,
                        value3,
                        value4);
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    @Override
    public void onSensorChanged(SensorEvent event) {
        // Acquire mLock to ensure that mNativePlatformSensorAndroid is not reset between this check
        // and when it is used.
        synchronized (mLock) {
            if (mNativePlatformSensorAndroid == 0) {
                Log.w(
                        TAG,
                        "Should not get sensor events after PlatformSensorAndroid is destroyed.");
                return;
            }

            if (event.values.length < mReadingCount) {
                sensorError();
                stopSensor();
                return;
            }

            double timestamp = event.timestamp * SECONDS_IN_NANOSECOND;
            switch (event.values.length) {
                case 1:
                    updateSensorReading(timestamp, event.values[0], 0.0, 0.0, 0.0);
                    break;
                case 2:
                    updateSensorReading(timestamp, event.values[0], event.values[1], 0.0, 0.0);
                    break;
                case 3:
                    updateSensorReading(
                            timestamp, event.values[0], event.values[1], event.values[2], 0.0);
                    break;
                default:
                    updateSensorReading(
                            timestamp,
                            event.values[0],
                            event.values[1],
                            event.values[2],
                            event.values[3]);
            }
        }
    }

    /**
     * A testing method to let device::PlatformSensorAndroid simulates a OnSensorChanged call. The
     * event with length |readingValuesLength| is created and filled with readings as (reading_index
     * + 0.1).
     */
    @CalledByNativeForTesting
    public void simulateSensorEventForTesting(int readingValuesLength) {
        try {
            Constructor<SensorEvent> sensorEventConstructor =
                    SensorEvent.class.getDeclaredConstructor(Integer.TYPE);
            sensorEventConstructor.setAccessible(true);
            SensorEvent event = sensorEventConstructor.newInstance(readingValuesLength);
            event.timestamp = 123L;
            for (int i = 0; i < readingValuesLength; ++i) {
                event.values[i] = (float) (i + 0.1);
            }
            onSensorChanged(event);
        } catch (InvocationTargetException
                | NoSuchMethodException
                | InstantiationException
                | IllegalAccessException e) {
            Log.e(TAG, "Failed to create simulated SensorEvent.", e);
            return;
        }
    }

    @NativeMethods
    interface Natives {
        void notifyPlatformSensorError(long nativePlatformSensorAndroid, PlatformSensor caller);

        void updatePlatformSensorReading(
                long nativePlatformSensorAndroid,
                PlatformSensor caller,
                double timestamp,
                double value1,
                double value2,
                double value3,
                double value4);
    }
}
