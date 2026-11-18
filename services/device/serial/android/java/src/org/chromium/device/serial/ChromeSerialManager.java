// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.serial;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.hardware.serial.SerialManager;
import android.hardware.serial.SerialPort;
import android.hardware.serial.SerialPortListener;
import android.hardware.serial.SerialPortResponse;
import android.os.Build;
import android.os.OutcomeReceiver;

import androidx.annotation.GuardedBy;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Exposes Android Serial API as necessary for C++ device::SerialDeviceEnumeratorAndroid.
 *
 * <p>This class implements a platform interface that does not exist before SDK 37, and so cannot
 * even be loaded on older OS versions. Callers must first check
 * SerialDeviceEnumeratorAndroid::IsSupported().
 */
@JNINamespace("device")
@NullMarked
@RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
public class ChromeSerialManager implements SerialPortListener {
    @JniType("::device::SerialDeviceEnumeratorAndroid")
    interface NativeSerialDeviceEnumeratorAndroid extends JniTypeToken {}

    private static final String TAG = "ChromeSerialManager";

    private final Object mLock = new Object();

    /** Address of C++ object SerialDeviceEnumeratorAndroid. */
    @GuardedBy("mLock")
    private @Nullable JniRawPtr<NativeSerialDeviceEnumeratorAndroid> mNativePointer;

    /** Android SerialManager. */
    private final SerialManager mSerialManager;

    /** Maps port names to Android SerialPort objects used for port opening. */
    private final Map<String, SerialPort> mPorts = Collections.synchronizedMap(new HashMap<>());

    /** Maps port names to FileDescriptorReceiver objects for receiving port opening results. */
    private final Map<String, FileDescriptorReceiver> mReceivers =
            Collections.synchronizedMap(new HashMap<>());

    @VisibleForTesting
    protected ChromeSerialManager(
            JniRawPtr<NativeSerialDeviceEnumeratorAndroid> nativePointer,
            SerialManager serialManager) {
        mNativePointer = nativePointer;
        mSerialManager = serialManager;
    }

    @CalledByNative
    static @Nullable ChromeSerialManager create(
            JniRawPtr<NativeSerialDeviceEnumeratorAndroid> nativePointer) {
        SerialManager serialManager =
                ContextUtils.getApplicationContext().getSystemService(SerialManager.class);
        if (serialManager == null) {
            nativePointer.release();
            return null;
        }
        ChromeSerialManager instance = new ChromeSerialManager(nativePointer, serialManager);
        instance.registerListenerAndEnumeratePorts();
        return instance;
    }

    @VisibleForTesting
    protected void registerListenerAndEnumeratePorts() {
        mSerialManager.registerSerialPortListener(AsyncTask.THREAD_POOL_EXECUTOR, this);

        // Initial enumeration of existing serial ports
        List<SerialPort> ports = mSerialManager.getPorts();
        synchronized (mLock) {
            JniRawPtr<NativeSerialDeviceEnumeratorAndroid> nativePointer =
                    assumeNonNull(mNativePointer);
            for (int i = 0; i < ports.size(); i++) {
                SerialPort port = ports.get(i);
                mPorts.put(port.getName(), port);
                ChromeSerialManagerJni.get()
                        .addPortViaJni(
                                nativePointer,
                                port.getName(),
                                port.getVendorId(),
                                port.getProductId(),
                                /* initialEnumeration= */ true);
            }
        }
    }

    @CalledByNative
    @VisibleForTesting
    protected void close() {
        mSerialManager.unregisterSerialPortListener(this);
        synchronized (mLock) {
            if (mNativePointer != null) {
                mNativePointer.release();
                mNativePointer = null;
            }
        }
    }

    /**
     * Request to open this port.
     *
     * @return Empty string in case of success, the error message otherwise.
     */
    @CalledByNative
    @VisibleForTesting
    protected @JniType("std::string") String openPort(@JniType("std::string") String name) {
        SerialPort port = mPorts.get(name);
        if (port == null) {
            Log.w(TAG, "Port not found: " + name);
            return "Port not found";
        }
        int flags = SerialPort.OPEN_FLAG_READ_WRITE | SerialPort.OPEN_FLAG_NONBLOCK;
        var receiver = new FileDescriptorReceiver(name);
        mReceivers.put(name, receiver);
        port.requestOpen(flags, /* exclusive= */ true, AsyncTask.THREAD_POOL_EXECUTOR, receiver);
        return "";
    }

    @Override
    public void onSerialPortConnected(SerialPort port) {
        mPorts.put(port.getName(), port);
        synchronized (mLock) {
            if (mNativePointer == null) return;
            ChromeSerialManagerJni.get()
                    .addPortViaJni(
                            mNativePointer,
                            port.getName(),
                            port.getVendorId(),
                            port.getProductId(),
                            /* initialEnumeration= */ false);
        }
    }

    @Override
    public void onSerialPortDisconnected(SerialPort port) {
        mPorts.remove(port.getName());
        synchronized (mLock) {
            if (mNativePointer == null) return;
            ChromeSerialManagerJni.get().removePortViaJni(mNativePointer, port.getName());
        }
    }

    private class FileDescriptorReceiver implements OutcomeReceiver<SerialPortResponse, Exception> {
        private final String mPortName;

        private FileDescriptorReceiver(String portName) {
            mPortName = portName;
        }

        @Override
        public void onResult(SerialPortResponse response) {
            mReceivers.remove(mPortName);
            if (!Objects.equals(response.getPort().getName(), mPortName)) {
                Log.e(TAG, "Port mismatch: " + response.getPort().getName() + " != " + mPortName);
                return;
            }
            synchronized (mLock) {
                if (mNativePointer == null) {
                    try {
                        response.getFileDescriptor().close();
                    } catch (IOException e) {
                        Log.w(TAG, "Failed to close file descriptor", e);
                    }
                    return;
                }
                int fd = response.getFileDescriptor().detachFd();
                Log.d(TAG, "Returning file descriptor %d for port %s", fd, mPortName);
                ChromeSerialManagerJni.get().openPathCallbackViaJni(mNativePointer, mPortName, fd);
            }
        }

        @Override
        public void onError(Exception error) {
            mReceivers.remove(mPortName);
            Log.e(TAG, "Failed requestOpen()", error);
            synchronized (mLock) {
                if (mNativePointer == null) return;
                ChromeSerialManagerJni.get()
                        .errorCallbackViaJni(
                                mNativePointer, mPortName, "Error opening port", error.toString());
            }
        }
    }

    @NativeMethods
    interface Natives {
        void openPathCallbackViaJni(
                JniPtr<NativeSerialDeviceEnumeratorAndroid> self,
                @JniType("std::string") String portName,
                int fd);

        void errorCallbackViaJni(
                JniPtr<NativeSerialDeviceEnumeratorAndroid> self,
                @JniType("std::string") String portName,
                @JniType("std::string") String errorName,
                @JniType("std::string") String message);

        void addPortViaJni(
                JniPtr<NativeSerialDeviceEnumeratorAndroid> self,
                @JniType("std::string") String name,
                int vendorId,
                int productId,
                boolean initialEnumeration);

        void removePortViaJni(
                JniPtr<NativeSerialDeviceEnumeratorAndroid> self,
                @JniType("std::string") String name);
    }
}
