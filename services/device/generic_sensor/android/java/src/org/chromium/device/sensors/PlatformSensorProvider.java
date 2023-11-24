// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.sensors;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.device.mojom.SensorType;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Lifetime is controlled by device::PlatformSensorProviderAndroid. */
@JNINamespace("device")
class PlatformSensorProvider {
    /**
     * SensorManager that is shared among PlatformSensor objects. It is used for Sensor object
     * creation and @see android.hardware.SensorEventListener registration.
     * @see android.hardware.SensorManager
     */
    private SensorManager mSensorManager;

    /** Thread that is handling all sensor events. */
    private HandlerThread mSensorsThread;

    /**
     * Processes messages on #mSensorsThread message queue. Provided to #mSensorManager when
     * sensor should start polling for data.
     */
    private Handler mHandler;

    /** Set of currently active PlatformSensor objects. */
    private final Set<PlatformSensor> mActiveSensors = new HashSet<PlatformSensor>();

    /**
     * Returns shared thread Handler.
     *
     * @return Handler thread handler.
     */
    public Handler getHandler() {
        return mHandler;
    }

    /**
     * Returns shared SensorManager.
     *
     * @return SensorManager sensor manager.
     */
    public SensorManager getSensorManager() {
        return mSensorManager;
    }

    /**
     * Notifies PlatformSensorProvider that sensor started polling for data. Adds sensor to
     * a set of active sensors, creates and starts new thread if needed.
     */
    public void sensorStarted(PlatformSensor sensor) {
        synchronized (mActiveSensors) {
            if (mActiveSensors.isEmpty()) startSensorThread();
            mActiveSensors.add(sensor);
        }
    }

    /**
     * Notifies PlatformSensorProvider that sensor is no longer polling for data. When
     * #mActiveSensors becomes empty thread is stopped.
     */
    public void sensorStopped(PlatformSensor sensor) {
        synchronized (mActiveSensors) {
            mActiveSensors.remove(sensor);
            if (mActiveSensors.isEmpty()) stopSensorThread();
        }
    }

    /** Starts sensor handler thread. */
    protected void startSensorThread() {
        if (mSensorsThread == null) {
            mSensorsThread = new HandlerThread("SensorsHandlerThread");
            mSensorsThread.start();
            mHandler = new Handler(mSensorsThread.getLooper());
        }
    }

    /** Stops sensor handler thread. */
    protected void stopSensorThread() {
        if (mSensorsThread != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                mSensorsThread.quitSafely();
            } else {
                mSensorsThread.quit();
            }
            mSensorsThread = null;
            mHandler = null;
        }
    }

    /** Constructor. */
    protected PlatformSensorProvider(Context context) {
        mSensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
    }

    /**
     * Creates PlatformSensorProvider instance.
     *
     * @return PlatformSensorProvider new PlatformSensorProvider instance.
     */
    protected static PlatformSensorProvider createForTest(Context context) {
        return new PlatformSensorProvider(context);
    }

    /**
     * Creates PlatformSensorProvider instance.
     *
     * @return PlatformSensorProvider new PlatformSensorProvider instance.
     */
    @CalledByNative
    protected static PlatformSensorProvider create() {
        return new PlatformSensorProvider(ContextUtils.getApplicationContext());
    }

    /** Sets |mSensorManager| to null for testing purposes. */
    @CalledByNative
    protected void setSensorManagerToNullForTesting() {
        mSensorManager = null;
    }

    /**
     * Checks if |type| sensor is available.
     *
     * @param type type of a sensor.
     * @return If |type| sensor is available, returns true; otherwise returns false.
     */
    @CalledByNative
    protected boolean hasSensorType(int type) {
        if (mSensorManager == null) return false;

        // Type of the sensor to be constructed. @see android.hardware.Sensor.TYPE_*
        int sensorType;

        switch (type) {
            case SensorType.AMBIENT_LIGHT:
                sensorType = Sensor.TYPE_LIGHT;
                break;
            case SensorType.ACCELEROMETER:
                sensorType = Sensor.TYPE_ACCELEROMETER;
                break;
            case SensorType.LINEAR_ACCELERATION:
                sensorType = Sensor.TYPE_LINEAR_ACCELERATION;
                break;
            case SensorType.GRAVITY:
                sensorType = Sensor.TYPE_GRAVITY;
                break;
            case SensorType.GYROSCOPE:
                sensorType = Sensor.TYPE_GYROSCOPE;
                break;
            case SensorType.MAGNETOMETER:
                sensorType = Sensor.TYPE_MAGNETIC_FIELD;
                break;
            case SensorType.ABSOLUTE_ORIENTATION_QUATERNION:
                sensorType = Sensor.TYPE_ROTATION_VECTOR;
                break;
            case SensorType.RELATIVE_ORIENTATION_QUATERNION:
                sensorType = Sensor.TYPE_GAME_ROTATION_VECTOR;
                break;
            default:
                return false;
        }

        List<Sensor> sensors = mSensorManager.getSensorList(sensorType);
        return !sensors.isEmpty();
    }
}
