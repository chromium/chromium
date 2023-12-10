// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.usb;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;

import java.util.HashMap;

/**
 * Exposes android.hardware.usb.UsbManager as necessary for C++
 * device::UsbServiceAndroid.
 *
 * Lifetime is controlled by device::UsbServiceAndroid.
 */
@JNINamespace("device")
final class ChromeUsbService {
    private static final String TAG = "Usb";
    private static final String ACTION_USB_PERMISSION = "org.chromium.device.ACTION_USB_PERMISSION";

    long mUsbServiceAndroid;
    UsbManager mUsbManager;
    BroadcastReceiver mUsbPermissionReceiver;
    BroadcastReceiver mUsbDeviceChangeReceiver;

    private ChromeUsbService(long usbServiceAndroid) {
        mUsbServiceAndroid = usbServiceAndroid;
        mUsbManager =
                (UsbManager)
                        ContextUtils.getApplicationContext().getSystemService(Context.USB_SERVICE);
        registerForUsbDeviceIntentBroadcasts();
        Log.v(TAG, "ChromeUsbService created.");
    }

    @CalledByNative
    private static ChromeUsbService create(long usbServiceAndroid) {
        return new ChromeUsbService(usbServiceAndroid);
    }

    @CalledByNative
    private Object[] getDevices() {
        HashMap<String, UsbDevice> deviceList = mUsbManager.getDeviceList();
        return deviceList.values().toArray();
    }

    @CalledByNative
    private UsbDeviceConnection openDevice(ChromeUsbDevice wrapper) {
        UsbDevice device = wrapper.getDevice();
        return mUsbManager.openDevice(device);
    }

    @CalledByNative
    private boolean hasDevicePermission(ChromeUsbDevice wrapper) {
        UsbDevice device = wrapper.getDevice();
        return mUsbManager.hasPermission(device);
    }

    @CalledByNative
    private void requestDevicePermission(ChromeUsbDevice wrapper) {
        UsbDevice device = wrapper.getDevice();
        if (mUsbManager.hasPermission(device)) {
            ChromeUsbServiceJni.get()
                    .devicePermissionRequestComplete(
                            mUsbServiceAndroid, ChromeUsbService.this, device.getDeviceId(), true);
        } else {
            Context context = ContextUtils.getApplicationContext();
            Intent intent = new Intent(ACTION_USB_PERMISSION);
            intent.setPackage(context.getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(
                            context, 0, intent, IntentUtils.getPendingIntentMutabilityFlag(true));
            mUsbManager.requestPermission(wrapper.getDevice(), pendingIntent);
        }
    }

    @CalledByNative
    private void close() {
        unregisterForUsbDeviceIntentBroadcasts();
    }

    private void registerForUsbDeviceIntentBroadcasts() {
        mUsbPermissionReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        if (!IntentUtils.isTrustedIntentFromSelf(intent)) return;
                        assert ACTION_USB_PERMISSION.equals(intent.getAction());
                        UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                        ChromeUsbServiceJni.get()
                                .devicePermissionRequestComplete(
                                        mUsbServiceAndroid,
                                        ChromeUsbService.this,
                                        device.getDeviceId(),
                                        intent.getBooleanExtra(
                                                UsbManager.EXTRA_PERMISSION_GRANTED, false));
                    }
                };
        mUsbDeviceChangeReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                        if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(intent.getAction())) {
                            ChromeUsbServiceJni.get()
                                    .deviceAttached(
                                            mUsbServiceAndroid, ChromeUsbService.this, device);
                        } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(
                                intent.getAction())) {
                            ChromeUsbServiceJni.get()
                                    .deviceDetached(
                                            mUsbServiceAndroid,
                                            ChromeUsbService.this,
                                            device.getDeviceId());
                        }
                    }
                };

        Context context = ContextUtils.getApplicationContext();
        IntentFilter permissionFilter = new IntentFilter();
        permissionFilter.addAction(ACTION_USB_PERMISSION);
        ContextUtils.registerNonExportedBroadcastReceiver(
                context, mUsbPermissionReceiver, permissionFilter);
        IntentFilter deviceChangeFilter = new IntentFilter();
        deviceChangeFilter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        deviceChangeFilter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        ContextUtils.registerProtectedBroadcastReceiver(
                context, mUsbDeviceChangeReceiver, deviceChangeFilter);
    }

    private void unregisterForUsbDeviceIntentBroadcasts() {
        ContextUtils.getApplicationContext().unregisterReceiver(mUsbDeviceChangeReceiver);
        mUsbDeviceChangeReceiver = null;
        ContextUtils.getApplicationContext().unregisterReceiver(mUsbPermissionReceiver);
        mUsbPermissionReceiver = null;
    }

    @NativeMethods
    interface Natives {
        void deviceAttached(
                long nativeUsbServiceAndroid, ChromeUsbService caller, UsbDevice device);

        void deviceDetached(long nativeUsbServiceAndroid, ChromeUsbService caller, int deviceId);

        void devicePermissionRequestComplete(
                long nativeUsbServiceAndroid,
                ChromeUsbService caller,
                int deviceId,
                boolean granted);
    }
}
