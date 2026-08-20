// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.hid;

import android.app.AppOpsManager;
import android.app.AppOpsManager.OnOpChangedListener;
import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.hid.HidDevice;
import org.chromium.base.hid.HidDeviceListener;
import org.chromium.base.hid.HidManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * Exposes org.chromium.base.hid.HidManager as necessary for C++ device::HidServiceAndroid.
 *
 * <p>Lifetime is controlled by device::HidServiceAndroid.
 */
@JNINamespace("device")
@RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
@NullMarked
final class ChromeHidManager {
    private static final String TAG = "Hid";

    private long mNativePointer;
    private final HidManager mHidManager;

    private final SequencedTaskRunner mExecutor =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
    private @Nullable OnOpChangedListener mOpListener;
    private @Nullable HidDeviceListener mListener;

    private ChromeHidManager(long nativePointer, HidManager hidManager) {
        mNativePointer = nativePointer;
        mHidManager = hidManager;

        Context context = ContextUtils.getApplicationContext();
        AppOpsManager appOps = (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
        if (appOps != null) {
            String op = AppOpsManager.permissionToOp("android.permission.ACCESS_HID");
            if (op != null) {
                mOpListener =
                        new OnOpChangedListener() {
                            @Override
                            public void onOpChanged(String op, String packageName) {
                                // OnOpChangedListener callbacks may be dispatched on an arbitrary
                                // binder thread from the system service; hop to the UI thread to
                                // update state safely.
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT, () -> updatePermissionState());
                            }
                        };
                appOps.startWatchingMode(op, context.getPackageName(), mOpListener);
            }
        }

        updatePermissionState();
        Log.v(TAG, "ChromeHidManager created.");
    }

    @CalledByNative
    private static @Nullable ChromeHidManager create(long nativePointer) {
        AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            return null;
        }
        HidManager hidManager = delegate.getHidManager();
        if (hidManager == null) {
            return null;
        }
        return new ChromeHidManager(nativePointer, hidManager);
    }

    private void updatePermissionState() {
        if (mNativePointer == 0) return;

        boolean hasPermission = mHidManager.canEnumerateDevices();
        if (hasPermission && mListener == null) {
            mListener =
                    new HidDeviceListener() {
                        @Override
                        public void onHidDeviceAdded(HidDevice device) {
                            if (mNativePointer == 0) return;
                            handleDeviceAdded(
                                    device,
                                    device.getVendorId(),
                                    device.getProductId(),
                                    device.getName(),
                                    device.getTransport(),
                                    device.getReportDescriptor());
                        }

                        @Override
                        public void onHidDeviceRemoved(HidDevice device) {
                            handleDeviceRemoved(device);
                        }
                    };
            mHidManager.registerListener(mListener, PostTask.getUiUserVisibleExecutor());
            enumerateDevices();
        } else if (!hasPermission && mListener != null) {
            try {
                mHidManager.unregisterListener(mListener);
            } catch (SecurityException | IllegalStateException e) {
                Log.w(TAG, "Failed to unregister HID listener after permission revocation", e);
            }
            mListener = null;
            ChromeHidManagerJni.get().onAllDevicesRemoved(mNativePointer);
        }
    }

    private void handleDeviceAdded(
            HidDevice device,
            int vendorId,
            int productId,
            @Nullable String productName,
            int transportType,
            byte @Nullable [] reportDesc) {
        if (mNativePointer == 0) return;
        ChromeHidManagerJni.get()
                .onDeviceAdded(
                        mNativePointer,
                        device.hashCode(),
                        vendorId,
                        productId,
                        productName,
                        device.getUniqueId(),
                        device.getPhysicalAddress(),
                        transportType,
                        reportDesc);
    }

    private void handleDeviceRemoved(HidDevice device) {
        if (mNativePointer == 0) return;
        ChromeHidManagerJni.get().onDeviceRemoved(mNativePointer, device.hashCode());
    }

    @CalledByNative
    private void enumerateDevices() {
        if (mNativePointer == 0) return;

        if (!mHidManager.canEnumerateDevices()) {
            Log.w(TAG, "Cannot enumerate HID devices: permission denied.");
            ChromeHidManagerJni.get().onEnumerationComplete(mNativePointer);
            return;
        }

        mExecutor.execute(
                () -> {
                    List<HidDevice> devices = null;
                    try {
                        // May throw SecurityException if the user revokes HID Special App Access
                        // in Android Settings concurrently while enumeration is in progress.
                        devices = mHidManager.getDevices();
                    } catch (SecurityException | IllegalStateException e) {
                        Log.e(TAG, "Failed to retrieve HID devices", e);
                    }
                    final List<HidDevice> finalDevices = devices;
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (mNativePointer != 0) {
                                    if (finalDevices != null) {
                                        for (HidDevice device : finalDevices) {
                                            handleDeviceAdded(
                                                    device,
                                                    device.getVendorId(),
                                                    device.getProductId(),
                                                    device.getName(),
                                                    device.getTransport(),
                                                    device.getReportDescriptor());
                                        }
                                    }
                                    ChromeHidManagerJni.get().onEnumerationComplete(mNativePointer);
                                }
                            });
                });
    }

    @CalledByNative
    private void shutdown() {
        mNativePointer = 0;
        if (mListener != null) {
            try {
                mHidManager.unregisterListener(mListener);
            } catch (SecurityException | IllegalStateException e) {
                Log.w(TAG, "Failed to unregister HID listener during shutdown", e);
            }
            mListener = null;
        }
        if (mOpListener != null) {
            Context context = ContextUtils.getApplicationContext();
            AppOpsManager appOps =
                    (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
            if (appOps != null) {
                appOps.stopWatchingMode(mOpListener);
            }
            mOpListener = null;
        }
    }

    @NativeMethods
    interface Natives {
        void onDeviceAdded(
                long nativeHidServiceAndroid,
                int deviceId,
                int vendorId,
                int productId,
                @JniType("std::string") @Nullable String productName,
                @JniType("std::string") @Nullable String serialNumber,
                @JniType("std::string") @Nullable String physicalAddress,
                int transportType,
                @JniType("std::vector<uint8_t>") byte @Nullable [] reportDescriptor);

        void onDeviceRemoved(long nativeHidServiceAndroid, int deviceId);

        void onEnumerationComplete(long nativeHidServiceAndroid);

        void onAllDevicesRemoved(long nativeHidServiceAndroid);
    }
}
