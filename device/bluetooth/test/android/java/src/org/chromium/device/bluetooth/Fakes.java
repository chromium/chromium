// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.ParcelUuid;
import android.test.mock.MockContext;
import android.util.ArraySet;
import android.util.SparseArray;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.components.location.LocationUtils;
import org.chromium.device.bluetooth.test.TestRSSI;
import org.chromium.device.bluetooth.test.TestTxPower;
import org.chromium.device.bluetooth.wrapper.BluetoothAdapterWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothDeviceWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothGattCallbackWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothGattCharacteristicWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothGattDescriptorWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothGattServiceWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothGattWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothLeScannerWrapper;
import org.chromium.device.bluetooth.wrapper.BluetoothSocketWrapper;
import org.chromium.device.bluetooth.wrapper.DeviceBondStateReceiverWrapper;
import org.chromium.device.bluetooth.wrapper.DeviceConnectStateReceiverWrapper;
import org.chromium.device.bluetooth.wrapper.ScanCallbackWrapper;
import org.chromium.device.bluetooth.wrapper.ScanResultWrapper;
import org.chromium.device.bluetooth.wrapper.ThreadUtilsWrapper;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

/**
 * Fake implementations of android.bluetooth.* classes for testing.
 *
 * <p>Fakes are contained in a single file to simplify code. Only one C++ file may access a Java
 * file via JNI, and all of these classes are accessed by bluetooth_test_android.cc. The alternative
 * would be a C++ .h, .cc file for each of these classes.
 */
@JNINamespace("device")
class Fakes {
    private static final String TAG = "Bluetooth";

    // Android uses Integer.MIN_VALUE to signal no Tx Power in advertisement
    // packet.
    // https://developer.android.com/reference/android/bluetooth/le/ScanRecord.html#getTxPowerLevel()
    private static final int NO_TX_POWER = Integer.MIN_VALUE;

    /**
     * Sets the factory for LocationUtils to return an instance whose
     * isSystemLocationSettingEnabled method returns |isEnabled|.
     */
    @CalledByNative
    public static void setLocationServicesState(final boolean isEnabled) {
        LocationUtils.setFactory(
                new LocationUtils.Factory() {
                    @Override
                    public LocationUtils create() {
                        return new LocationUtils() {
                            @Override
                            public boolean isSystemLocationSettingEnabled() {
                                return isEnabled;
                            }
                        };
                    }
                });
    }

    /**
     * Sets the factory for ThreadUtilsWrapper to always post a task to the UI thread
     * rather than running the task immediately. This simulates events arriving on a separate
     * thread on Android.
     * runOnUiThread uses FakesJni.get().postTaskFromJava. This allows java to post tasks to the
     * message loop that the test is using rather than to the Java message loop which
     * is not running during tests.
     */
    @CalledByNative
    public static void initFakeThreadUtilsWrapper(final long nativeBluetoothTestAndroid) {
        ThreadUtilsWrapper.setFactory(
                new ThreadUtilsWrapper.Factory() {
                    @Override
                    public ThreadUtilsWrapper create() {
                        return new ThreadUtilsWrapper() {
                            @Override
                            public void runOnUiThread(Runnable r) {
                                FakesJni.get().postTaskFromJava(nativeBluetoothTestAndroid, r);
                            }

                            @Override
                            public void postOnUiThread(Runnable r) {
                                FakesJni.get().postTaskFromJava(nativeBluetoothTestAndroid, r);
                            }

                            @Override
                            public void postOnUiThreadDelayed(Runnable r, long delayMillis) {
                                FakesJni.get()
                                        .postDelayedTaskFromJava(
                                                nativeBluetoothTestAndroid, r, delayMillis);
                            }
                        };
                    }
                });
    }

    @CalledByNative
    public static void runRunnable(Runnable r) {
        r.run();
    }

    /** Fakes android.bluetooth.BluetoothAdapter. */
    static class FakeBluetoothAdapter extends BluetoothAdapterWrapper {
        private final FakeContext mFakeContext;
        private final FakeBluetoothLeScanner mFakeScanner;
        private boolean mPowered = true;
        private int mEnabledDeviceTransport = BluetoothDevice.DEVICE_TYPE_DUAL;
        private final ArraySet<BluetoothDeviceWrapper> mFakePairedDevices = new ArraySet();
        private DeviceBondStateReceiverWrapper.Callback mDeviceBondStateCallback;
        DeviceConnectStateReceiverWrapper.Callback mDeviceConnectStateCallback;
        final long mNativeBluetoothTestAndroid;

        /** Creates a FakeBluetoothAdapter. */
        @CalledByNative("FakeBluetoothAdapter")
        public static FakeBluetoothAdapter create(long nativeBluetoothTestAndroid) {
            Log.v(TAG, "FakeBluetoothAdapter created.");
            return new FakeBluetoothAdapter(nativeBluetoothTestAndroid);
        }

        private FakeBluetoothAdapter(long nativeBluetoothTestAndroid) {
            super(
                    null,
                    new FakeContext(),
                    /* hasBluetoothFeature= */ true,
                    /* hasLowEnergyFeature= */ true);
            mNativeBluetoothTestAndroid = nativeBluetoothTestAndroid;
            mFakeContext = (FakeContext) mContext;
            mFakeScanner = new FakeBluetoothLeScanner();
        }

        @CalledByNative("FakeBluetoothAdapter")
        public void setFakePermission(boolean enabled) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                mFakeContext.setBluetoothPermission(enabled);
            } else {
                mFakeContext.setLocationPermission(enabled);
            }
        }

        @CalledByNative("FakeBluetoothAdapter")
        public void setEnabledTransport(int enabledTransport) {
            mEnabledDeviceTransport = enabledTransport;
        }

        /** Creates and discovers a new device. */
        @CalledByNative("FakeBluetoothAdapter")
        public void simulateLowEnergyDevice(int deviceOrdinal) {
            if (mFakeScanner == null) {
                return;
            }

            switch (deviceOrdinal) {
                case 1:
                    {
                        ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                        uuids.add(ParcelUuid.fromString("00001800-0000-1000-8000-00805f9b34fb"));
                        uuids.add(ParcelUuid.fromString("00001801-0000-1000-8000-00805f9b34fb"));

                        HashMap<ParcelUuid, byte[]> serviceData = new HashMap<>();
                        serviceData.put(
                                ParcelUuid.fromString("0000180d-0000-1000-8000-00805f9b34fb"),
                                new byte[] {1});

                        SparseArray<byte[]> manufacturerData = new SparseArray<>();
                        manufacturerData.put(0x00E0, new byte[] {0x01, 0x02, 0x03, 0x04});

                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "01:00:00:90:1E:BE",
                                                "FakeBluetoothDevice",
                                                BluetoothDevice.DEVICE_TYPE_LE,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "FakeBluetoothDevice",
                                        TestRSSI.LOWEST,
                                        4,
                                        uuids,
                                        TestTxPower.LOWEST,
                                        serviceData,
                                        manufacturerData));
                        break;
                    }
                case 2:
                    {
                        ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                        uuids.add(ParcelUuid.fromString("00001802-0000-1000-8000-00805f9b34fb"));
                        uuids.add(ParcelUuid.fromString("00001803-0000-1000-8000-00805f9b34fb"));

                        HashMap<ParcelUuid, byte[]> serviceData = new HashMap<>();
                        serviceData.put(
                                ParcelUuid.fromString("0000180d-0000-1000-8000-00805f9b34fb"),
                                new byte[] {});
                        serviceData.put(
                                ParcelUuid.fromString("00001802-0000-1000-8000-00805f9b34fb"),
                                new byte[] {0, 2});

                        SparseArray<byte[]> manufacturerData = new SparseArray<>();
                        manufacturerData.put(0x00E0, new byte[] {});

                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "01:00:00:90:1E:BE",
                                                "FakeBluetoothDevice",
                                                BluetoothDevice.DEVICE_TYPE_LE,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "Local Device Name",
                                        TestRSSI.LOWER,
                                        5,
                                        uuids,
                                        TestTxPower.LOWER,
                                        serviceData,
                                        manufacturerData));
                        break;
                    }
                case 3:
                    {
                        ArrayList<ParcelUuid> uuids = null;
                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "01:00:00:90:1E:BE",
                                                "",
                                                BluetoothDevice.DEVICE_TYPE_LE,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "Local Device Name",
                                        TestRSSI.LOW,
                                        -1,
                                        uuids,
                                        NO_TX_POWER,
                                        null,
                                        null));

                        break;
                    }
                case 4:
                    {
                        ArrayList<ParcelUuid> uuids = null;
                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "02:00:00:8B:74:63",
                                                "",
                                                BluetoothDevice.DEVICE_TYPE_LE,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "Local Device Name",
                                        TestRSSI.MEDIUM,
                                        -1,
                                        uuids,
                                        NO_TX_POWER,
                                        null,
                                        null));

                        break;
                    }
                case 5:
                    {
                        ArrayList<ParcelUuid> uuids = null;
                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "01:00:00:90:1E:BE",
                                                null,
                                                BluetoothDevice.DEVICE_TYPE_LE,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "Local Device Name",
                                        TestRSSI.HIGH,
                                        -1,
                                        uuids,
                                        NO_TX_POWER,
                                        null,
                                        null));
                        break;
                    }
                case 6:
                    {
                        ArrayList<ParcelUuid> uuids = null;
                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "02:00:00:8B:74:63",
                                                null,
                                                BluetoothDevice.DEVICE_TYPE_DUAL,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "Local Device Name",
                                        TestRSSI.LOWEST,
                                        -1,
                                        uuids,
                                        NO_TX_POWER,
                                        null,
                                        null));
                        break;
                    }
                case 7:
                    {
                        ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                        uuids.add(ParcelUuid.fromString("f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb"));

                        HashMap<ParcelUuid, byte[]> serviceData = new HashMap<>();
                        serviceData.put(
                                ParcelUuid.fromString("f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb"),
                                new byte[] {0, 20});

                        mFakeScanner.mScanCallback.onScanResult(
                                ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                                new FakeScanResult(
                                        new FakeBluetoothDevice(
                                                this,
                                                "01:00:00:90:1E:BE",
                                                "U2F FakeDevice",
                                                BluetoothDevice.DEVICE_TYPE_LE,
                                                /* uuid= */ null,
                                                BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED),
                                        "Local Device Name",
                                        TestRSSI.LOWEST,
                                        -1,
                                        uuids,
                                        NO_TX_POWER,
                                        serviceData,
                                        null));
                        break;
                    }
            }
        }

        @CalledByNative("FakeBluetoothAdapter")
        public @JniType("std::string") String simulatePairedClassicDevice(
                int deviceOrdinal, boolean notifyCallback) {
            final FakeBluetoothDevice device;
            switch (deviceOrdinal) {
                case 0:
                    device =
                            new FakeBluetoothDevice(
                                    this,
                                    "03:00:00:17:C0:57",
                                    "FakeBluetoothDevice",
                                    BluetoothDevice.DEVICE_TYPE_CLASSIC,
                                    /* uuid= */ null,
                                    BluetoothDeviceWrapper.DEVICE_CLASS_UNSPECIFIED);
                    break;
                case 1:
                    device =
                            new FakeBluetoothDevice(
                                    this,
                                    "01:00:00:90:1E:BE",
                                    "Fake classic device 1",
                                    BluetoothDevice.DEVICE_TYPE_CLASSIC,
                                    "00001101-0000-1000-8000-00805F9B34FB",
                                    /* bluetoothClass= (desktop) */ 0x104);
                    break;
                case 2:
                    device =
                            new FakeBluetoothDevice(
                                    this,
                                    "02:00:00:8B:74:63",
                                    "Fake classic device 2",
                                    BluetoothDevice.DEVICE_TYPE_CLASSIC,
                                    "00001101-0000-1000-8000-00805F9B34FB",
                                    /* bluetoothClass= (cellular phone) */ 0x204);
                    break;
                default:
                    throw new IllegalArgumentException();
            }

            mFakePairedDevices.add(device);
            // When a device becomes paired, it needs to be connected first. The connection state
            // broadcast comes before the bond broadcast.
            if (notifyCallback && mDeviceConnectStateCallback != null) {
                mDeviceConnectStateCallback.onDeviceConnectStateChanged(
                        device, BluetoothDevice.TRANSPORT_BREDR, true);
            }
            if (notifyCallback && mDeviceBondStateCallback != null) {
                mDeviceBondStateCallback.onDeviceBondStateChanged(
                        device, BluetoothDevice.BOND_BONDED);
            }
            return device.getAddress();
        }

        @CalledByNative("FakeBluetoothAdapter")
        public void unpairDevice(@JniType("std::string") String address) {
            FakeBluetoothDevice removedDevice = null;
            Iterator pairedDeviceIterator = mFakePairedDevices.iterator();
            while (pairedDeviceIterator.hasNext()) {
                BluetoothDeviceWrapper device =
                        (BluetoothDeviceWrapper) pairedDeviceIterator.next();
                if (device.getAddress().equals(address)) {
                    pairedDeviceIterator.remove();
                    removedDevice = (FakeBluetoothDevice) device;
                    break;
                }
            }
            if (removedDevice != null && mDeviceBondStateCallback != null) {
                mDeviceBondStateCallback.onDeviceBondStateChanged(
                        removedDevice, BluetoothDevice.BOND_NONE);
            }
        }

        @CalledByNative("FakeBluetoothAdapter")
        public void forceIllegalStateException() {
            if (mFakeScanner != null) {
                mFakeScanner.forceIllegalStateException();
            }
        }

        @CalledByNative("FakeBluetoothAdapter")
        public void failCurrentLeScan(int errorCode) {
            mFakeScanner.mScanCallback.onScanFailed(errorCode);
            mFakeScanner.mScanCallback = null;
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothAdapterWrapper overrides:

        @Override
        public boolean disable() {
            // android.bluetooth.BluetoothAdapter::disable() is an async call, so we simulate this
            // by posting a task to the UI thread.
            FakesJni.get()
                    .postTaskFromJava(
                            mNativeBluetoothTestAndroid,
                            new Runnable() {
                                @Override
                                public void run() {
                                    mPowered = false;
                                    FakesJni.get()
                                            .onFakeAdapterStateChanged(
                                                    mNativeBluetoothTestAndroid, false);
                                }
                            });
            return true;
        }

        @Override
        public boolean enable() {
            // android.bluetooth.BluetoothAdapter::enable() is an async call, so we simulate this by
            // posting a task to the UI thread.
            FakesJni.get()
                    .postTaskFromJava(
                            mNativeBluetoothTestAndroid,
                            new Runnable() {
                                @Override
                                public void run() {
                                    mPowered = true;
                                    FakesJni.get()
                                            .onFakeAdapterStateChanged(
                                                    mNativeBluetoothTestAndroid, true);
                                }
                            });
            return true;
        }

        @Override
        public String getAddress() {
            return "A1:B2:C3:D4:E5:F6";
        }

        @Override
        public BluetoothLeScannerWrapper getBluetoothLeScanner() {
            final boolean isLeEnabled =
                    (mEnabledDeviceTransport & BluetoothDevice.DEVICE_TYPE_LE)
                            == BluetoothDevice.DEVICE_TYPE_LE;
            if (isEnabled() && isLeEnabled) {
                return mFakeScanner;
            }
            return null;
        }

        @Override
        public String getName() {
            return "FakeBluetoothAdapter";
        }

        @Override
        public int getScanMode() {
            return android.bluetooth.BluetoothAdapter.SCAN_MODE_NONE;
        }

        @Override
        public boolean isEnabled() {
            return mPowered;
        }

        @Override
        public boolean isDiscovering() {
            return false;
        }

        @Override
        public boolean hasBluetoothFeature() {
            return (mEnabledDeviceTransport & BluetoothDevice.DEVICE_TYPE_CLASSIC)
                    == BluetoothDevice.DEVICE_TYPE_CLASSIC;
        }

        @Override
        public Set<BluetoothDeviceWrapper> getBondedDevices() {
            if (!isEnabled() || !hasBluetoothFeature()) {
                return null;
            }

            return mFakePairedDevices;
        }

        @Override
        public DeviceBondStateReceiverWrapper createDeviceBondStateReceiver(
                DeviceBondStateReceiverWrapper.Callback callback) {
            mDeviceBondStateCallback = callback;
            return super.createDeviceBondStateReceiver(callback);
        }

        @Override
        public DeviceConnectStateReceiverWrapper createDeviceConnectStateReceiver(
                DeviceConnectStateReceiverWrapper.Callback callback) {
            mDeviceConnectStateCallback = callback;
            return super.createDeviceConnectStateReceiver(callback);
        }
    }

    /** Fakes android.content.Context by extending MockContext. */
    static class FakeContext extends MockContext {
        private int mLocationPermission;
        private int mBluetoothPermission;

        public FakeContext() {
            super();
            mLocationPermission = PackageManager.PERMISSION_GRANTED;
            mBluetoothPermission = PackageManager.PERMISSION_GRANTED;
        }

        public void setLocationPermission(boolean enabled) {
            mLocationPermission = (enabled ? PackageManager.PERMISSION_GRANTED
                                           : PackageManager.PERMISSION_DENIED);
        }

        public void setBluetoothPermission(boolean enabled) {
            mBluetoothPermission = (enabled ? PackageManager.PERMISSION_GRANTED
                                            : PackageManager.PERMISSION_DENIED);
        }

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String permission,
                Handler scheduler) {
            return null;
        }

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String permission,
                Handler scheduler,
                int flags) {
            return null;
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {}

        @Override
        public int checkCallingOrSelfPermission(String permission) {
            final boolean isBluetoothPermissionSOrAbove =
                permission.equals(Manifest.permission.BLUETOOTH_SCAN)
                    || permission.equals(Manifest.permission.BLUETOOTH_CONNECT);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && isBluetoothPermissionSOrAbove) {
                return mBluetoothPermission;
            }
            if (permission.equals(Manifest.permission.ACCESS_FINE_LOCATION)
                    || permission.equals(Manifest.permission.ACCESS_COARSE_LOCATION)) {
                return mLocationPermission;
            }
            return PackageManager.PERMISSION_DENIED;
        }
    }

    /** Fakes android.bluetooth.le.BluetoothLeScanner. */
    static class FakeBluetoothLeScanner extends BluetoothLeScannerWrapper {
        public ScanCallbackWrapper mScanCallback;
        private boolean mThrowException;

        private FakeBluetoothLeScanner() {
            super(null);
        }

        @Override
        public void startScan(
                List<ScanFilter> filters,
                int scanSettingsScanMode,
                ScanCallbackWrapper callback) {
            if (mScanCallback != null) {
                throw new IllegalArgumentException(
                        "FakeBluetoothLeScanner does not support multiple scans.");
            }
            if (mThrowException) {
                throw new IllegalStateException("Adapter is off.");
            }
            mScanCallback = callback;
        }

        @Override
        public void stopScan(ScanCallbackWrapper callback) {
            if (mScanCallback != callback) {
                throw new IllegalArgumentException("No scan in progress.");
            }
            if (mThrowException) {
                throw new IllegalStateException("Adapter is off.");
            }
            mScanCallback = null;
        }

        void forceIllegalStateException() {
            mThrowException = true;
        }
    }

    /** Fakes android.bluetooth.le.ScanResult */
    static class FakeScanResult implements ScanResultWrapper {
        private final FakeBluetoothDevice mDevice;
        private final String mLocalName;
        private final int mRssi;
        private final int mTxPower;
        private final int mAdvertisementFlags;
        private final ArrayList<ParcelUuid> mUuids;
        private final Map<ParcelUuid, byte[]> mServiceData;
        private final SparseArray<byte[]> mManufacturerData;

        FakeScanResult(
                FakeBluetoothDevice device,
                String localName,
                int rssi,
                int advertisementFlags,
                ArrayList<ParcelUuid> uuids,
                int txPower,
                Map<ParcelUuid, byte[]> serviceData,
                SparseArray<byte[]> manufacturerData) {
            mDevice = device;
            mLocalName = localName;
            mRssi = rssi;
            mAdvertisementFlags = advertisementFlags;
            mUuids = uuids;
            mTxPower = txPower;
            mServiceData = serviceData;
            mManufacturerData = manufacturerData;
        }

        @Override
        public BluetoothDeviceWrapper getDevice() {
            return mDevice;
        }

        @Override
        public int getRssi() {
            return mRssi;
        }

        @Override
        public List<ParcelUuid> getScanRecord_getServiceUuids() {
            return mUuids;
        }

        @Override
        public int getScanRecord_getTxPowerLevel() {
            return mTxPower;
        }

        @Override
        public Map<ParcelUuid, byte[]> getScanRecord_getServiceData() {
            return mServiceData;
        }

        @Override
        public SparseArray<byte[]> getScanRecord_getManufacturerSpecificData() {
            return mManufacturerData;
        }

        @Override
        public String getScanRecord_getDeviceName() {
            return mLocalName;
        }

        @Override
        public int getScanRecord_getAdvertiseFlags() {
            return mAdvertisementFlags;
        }
    }

    /** Fakes android.bluetooth.BluetoothDevice. */
    static class FakeBluetoothDevice extends BluetoothDeviceWrapper {
        final FakeBluetoothAdapter mAdapter;
        private final String mAddress;
        private final String mName;
        private final int mType;
        private final String mUuid;
        private final int mBluetoothClass;
        final FakeBluetoothGatt mGatt;
        private BluetoothGattCallbackWrapper mGattCallback;
        private String mNextExceptionMessageOnServiceConnection;

        static FakeBluetoothDevice sRememberedDevice;

        public FakeBluetoothDevice(
                FakeBluetoothAdapter adapter,
                String address,
                String name,
                int type,
                String uuid,
                int bluetoothClass) {
            super(null);
            mAdapter = adapter;
            mAddress = address;
            mName = name;
            mType = type;
            mUuid = uuid;
            mBluetoothClass = bluetoothClass;
            mGatt = new FakeBluetoothGatt(this);
        }

        // Implements BluetoothTestAndroid::RememberDeviceForSubsequentAction.
        @CalledByNative("FakeBluetoothDevice")
        private static void rememberDeviceForSubsequentAction(ChromeBluetoothDevice chromeDevice) {
            sRememberedDevice = (FakeBluetoothDevice) chromeDevice.mDevice;
        }

        // Create a call to onConnectionStateChange on the |chrome_device| using parameters
        // |status| & |connected|.
        @CalledByNative("FakeBluetoothDevice")
        private static void connectionStateChange(
                ChromeBluetoothDevice chromeDevice, int status, boolean connected) {
            FakeBluetoothDevice fakeDevice = (FakeBluetoothDevice) chromeDevice.mDevice;
            fakeDevice.mGattCallback.onConnectionStateChange(
                    status,
                    connected
                            ? android.bluetooth.BluetoothProfile.STATE_CONNECTED
                            : android.bluetooth.BluetoothProfile.STATE_DISCONNECTED);
        }

        @CalledByNative("FakeBluetoothDevice")
        private static void aclConnectionStateChange(
                ChromeBluetoothDevice chromeDevice, int transport, boolean connected) {
            FakeBluetoothDevice fakeDevice = (FakeBluetoothDevice) chromeDevice.mDevice;
            if (fakeDevice.mAdapter.mDeviceConnectStateCallback != null) {
                fakeDevice.mAdapter.mDeviceConnectStateCallback.onDeviceConnectStateChanged(
                        fakeDevice, transport, connected);
            }
        }

        // Create a call to onServicesDiscovered on the |chrome_device| using parameter
        // |status|.
        @CalledByNative("FakeBluetoothDevice")
        private static void servicesDiscovered(
                ChromeBluetoothDevice chromeDevice, int status, String uuidsSpaceDelimited) {
            if (chromeDevice == null && sRememberedDevice == null) {
                throw new IllegalArgumentException("rememberDevice wasn't called previously.");
            }

            FakeBluetoothDevice fakeDevice =
                    (chromeDevice == null)
                            ? sRememberedDevice
                            : (FakeBluetoothDevice) chromeDevice.mDevice;

            if (status == android.bluetooth.BluetoothGatt.GATT_SUCCESS) {
                fakeDevice.mGatt.mServices.clear();
                HashMap<String, Integer> uuidsToInstanceIdMap = new HashMap<String, Integer>();
                for (String uuid : uuidsSpaceDelimited.split(" ")) {
                    // String.split() can return empty strings. Ignore them.
                    if (uuid.isEmpty()) continue;
                    Integer previousId = uuidsToInstanceIdMap.get(uuid);
                    int instanceId = (previousId == null) ? 0 : previousId + 1;
                    uuidsToInstanceIdMap.put(uuid, instanceId);
                    fakeDevice.mGatt.mServices.add(
                            new FakeBluetoothGattService(
                                    fakeDevice, UUID.fromString(uuid), instanceId));
                }
            }

            fakeDevice.mGattCallback.onServicesDiscovered(status);
        }

        @CalledByNative("FakeBluetoothDevice")
        private static void failNextServiceConnection(
                ChromeBluetoothDevice chromeDevice, @JniType("std::string") String message) {
            FakeBluetoothDevice device = (FakeBluetoothDevice) chromeDevice.mDevice;
            device.mNextExceptionMessageOnServiceConnection = message;
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothDeviceWrapper overrides:

        @Override
        public BluetoothGattWrapper connectGatt(
                Context context,
                boolean autoConnect,
                BluetoothGattCallbackWrapper callback,
                int transport) {
            if (mGattCallback != null && mGattCallback != callback) {
                throw new IllegalArgumentException(
                        "BluetoothGattWrapper doesn't support calls to connectGatt() with "
                                + "multiple distinct callbacks.");
            }
            FakesJni.get()
                    .onFakeBluetoothDeviceConnectGattCalled(mAdapter.mNativeBluetoothTestAndroid);
            mGattCallback = callback;
            return mGatt;
        }

        @Override
        public String getAddress() {
            return mAddress;
        }

        @Override
        public int getBluetoothClass_getDeviceClass() {
            return mAdapter.isEnabled() ? mBluetoothClass : DEVICE_CLASS_UNSPECIFIED;
        }

        @Override
        public int getBondState() {
            if (mAdapter.mFakePairedDevices.contains(this)) {
                return BluetoothDevice.BOND_BONDED;
            }
            return BluetoothDevice.BOND_NONE;
        }

        @Override
        public String getName() {
            return mAdapter.isEnabled() ? mName : null;
        }

        @Override
        public int getType() {
            return mAdapter.isEnabled() ? mType : BluetoothDevice.DEVICE_TYPE_UNKNOWN;
        }

        @Override
        public ParcelUuid[] getUuids() {
            if (!mAdapter.isEnabled() || mUuid == null) {
                return null;
            }

            return new ParcelUuid[] {
                ParcelUuid.fromString(mUuid) // Serial UUID
            };
        }

        @Override
        public BluetoothSocketWrapper createRfcommSocketToServiceRecord(UUID uuid)
                throws IOException {
            if (mNextExceptionMessageOnServiceConnection != null) {
                throw new IOException(mNextExceptionMessageOnServiceConnection);
            }
            return new FakeBluetoothSocket();
        }

        @Override
        public BluetoothSocketWrapper createInsecureRfcommSocketToServiceRecord(UUID uuid)
                throws IOException {
            if (mNextExceptionMessageOnServiceConnection != null) {
                throw new IOException(mNextExceptionMessageOnServiceConnection);
            }
            return new FakeBluetoothSocket();
        }

        @Override
        public boolean equals(Object o) {
            if (o instanceof FakeBluetoothDevice) {
                return mAddress.equals(((FakeBluetoothDevice) o).mAddress);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return mAddress.hashCode();
        }
    }

    /** Fakes android.bluetooth.BluetoothGatt. */
    static class FakeBluetoothGatt extends BluetoothGattWrapper {
        final FakeBluetoothDevice mDevice;
        final ArrayList<BluetoothGattServiceWrapper> mServices;
        boolean mReadCharacteristicWillFailSynchronouslyOnce;
        boolean mSetCharacteristicNotificationWillFailSynchronouslyOnce;
        boolean mWriteCharacteristicWillFailSynchronouslyOnce;
        boolean mReadDescriptorWillFailSynchronouslyOnce;
        boolean mWriteDescriptorWillFailSynchronouslyOnce;

        public FakeBluetoothGatt(FakeBluetoothDevice device) {
            super(null, null);
            mDevice = device;
            mServices = new ArrayList<>();
        }

        @Override
        public void disconnect() {
            FakesJni.get()
                    .onFakeBluetoothGattDisconnect(mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }

        @Override
        public void close() {
            FakesJni.get().onFakeBluetoothGattClose(mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }

        @Override
        public boolean requestMtu(int mtu) {
            return false;
        }

        @Override
        public void discoverServices() {
            FakesJni.get()
                    .onFakeBluetoothGattDiscoverServices(
                            mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }

        @Override
        public List<BluetoothGattServiceWrapper> getServices() {
            return mServices;
        }

        @Override
        public boolean readCharacteristic(BluetoothGattCharacteristicWrapper characteristic) {
            if (mReadCharacteristicWillFailSynchronouslyOnce) {
                mReadCharacteristicWillFailSynchronouslyOnce = false;
                return false;
            }
            FakesJni.get()
                    .onFakeBluetoothGattReadCharacteristic(
                            mDevice.mAdapter.mNativeBluetoothTestAndroid);
            return true;
        }

        @Override
        public boolean setCharacteristicNotification(
                BluetoothGattCharacteristicWrapper characteristic, boolean enable) {
            if (mSetCharacteristicNotificationWillFailSynchronouslyOnce) {
                mSetCharacteristicNotificationWillFailSynchronouslyOnce = false;
                return false;
            }
            FakesJni.get()
                    .onFakeBluetoothGattSetCharacteristicNotification(
                            mDevice.mAdapter.mNativeBluetoothTestAndroid);
            return true;
        }

        @Override
        public boolean writeCharacteristic(BluetoothGattCharacteristicWrapper characteristic) {
            if (mWriteCharacteristicWillFailSynchronouslyOnce) {
                mWriteCharacteristicWillFailSynchronouslyOnce = false;
                return false;
            }
            FakesJni.get()
                    .onFakeBluetoothGattWriteCharacteristic(
                            mDevice.mAdapter.mNativeBluetoothTestAndroid,
                            characteristic.getValue());
            return true;
        }

        @Override
        public boolean readDescriptor(BluetoothGattDescriptorWrapper descriptor) {
            if (mReadDescriptorWillFailSynchronouslyOnce) {
                mReadDescriptorWillFailSynchronouslyOnce = false;
                return false;
            }
            FakesJni.get()
                    .onFakeBluetoothGattReadDescriptor(
                            mDevice.mAdapter.mNativeBluetoothTestAndroid);
            return true;
        }

        @Override
        public boolean writeDescriptor(BluetoothGattDescriptorWrapper descriptor) {
            if (mWriteDescriptorWillFailSynchronouslyOnce) {
                mWriteDescriptorWillFailSynchronouslyOnce = false;
                return false;
            }
            FakesJni.get()
                    .onFakeBluetoothGattWriteDescriptor(
                            mDevice.mAdapter.mNativeBluetoothTestAndroid, descriptor.getValue());
            return true;
        }
    }

    /** Fakes android.bluetooth.BluetoothGattService. */
    static class FakeBluetoothGattService extends BluetoothGattServiceWrapper {
        final FakeBluetoothDevice mDevice;
        final int mInstanceId;
        final UUID mUuid;
        final ArrayList<BluetoothGattCharacteristicWrapper> mCharacteristics;

        public FakeBluetoothGattService(FakeBluetoothDevice device, UUID uuid, int instanceId) {
            super(null, null);
            mDevice = device;
            mUuid = uuid;
            mInstanceId = instanceId;
            mCharacteristics = new ArrayList<>();
        }

        // Create a characteristic and add it to this service.
        @CalledByNative("FakeBluetoothGattService")
        private static void addCharacteristic(
                ChromeBluetoothRemoteGattService chromeService, String uuidString, int properties) {
            FakeBluetoothGattService fakeService =
                    (FakeBluetoothGattService) chromeService.mService;
            UUID uuid = UUID.fromString(uuidString);

            int countOfDuplicateUUID = 0;
            for (BluetoothGattCharacteristicWrapper characteristic :
                    fakeService.mCharacteristics) {
                if (characteristic.getUuid().equals(uuid)) {
                    countOfDuplicateUUID++;
                }
            }
            fakeService.mCharacteristics.add(
                    new FakeBluetoothGattCharacteristic(
                            fakeService, /* instanceId= */ countOfDuplicateUUID, properties, uuid));
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothGattServiceWrapper overrides:

        @Override
        public List<BluetoothGattCharacteristicWrapper> getCharacteristics() {
            return mCharacteristics;
        }

        @Override
        public int getInstanceId() {
            return mInstanceId;
        }

        @Override
        public UUID getUuid() {
            return mUuid;
        }
    }

    /** Fakes android.bluetooth.BluetoothGattCharacteristic. */
    static class FakeBluetoothGattCharacteristic
            extends BluetoothGattCharacteristicWrapper {
        final FakeBluetoothGattService mService;
        final int mInstanceId;
        final int mProperties;
        final UUID mUuid;
        byte[] mValue;
        int mWriteType;
        static FakeBluetoothGattCharacteristic sRememberedCharacteristic;
        final ArrayList<BluetoothGattDescriptorWrapper> mDescriptors;

        public FakeBluetoothGattCharacteristic(
                FakeBluetoothGattService service, int instanceId, int properties, UUID uuid) {
            super(null, null);
            mService = service;
            mInstanceId = instanceId;
            mProperties = properties;
            mUuid = uuid;
            mValue = new byte[0];
            mDescriptors = new ArrayList<>();
        }

        // Simulate a characteristic value notified as changed.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void valueChanged(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic, byte[] value) {
            if (chromeCharacteristic == null && sRememberedCharacteristic == null) {
                throw new IllegalArgumentException(
                        "rememberCharacteristic wasn't called previously.");
            }

            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (chromeCharacteristic == null)
                            ? sRememberedCharacteristic
                            : (FakeBluetoothGattCharacteristic)
                                    chromeCharacteristic.mCharacteristic;

            fakeCharacteristic.mValue = value;
            fakeCharacteristic.mService.mDevice.mGattCallback.onCharacteristicChanged(
                    fakeCharacteristic);
        }

        // Implements BluetoothTestAndroid::RememberCharacteristicForSubsequentAction.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void rememberCharacteristicForSubsequentAction(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic) {
            sRememberedCharacteristic =
                    (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;
        }

        // Simulate a value being read from a characteristic.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void valueRead(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic,
                int status,
                byte[] value) {
            if (chromeCharacteristic == null && sRememberedCharacteristic == null) {
                throw new IllegalArgumentException(
                        "rememberCharacteristic wasn't called previously.");
            }

            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (chromeCharacteristic == null)
                            ? sRememberedCharacteristic
                            : (FakeBluetoothGattCharacteristic)
                                    chromeCharacteristic.mCharacteristic;

            fakeCharacteristic.mValue = value;
            fakeCharacteristic.mService.mDevice.mGattCallback.onCharacteristicRead(
                    fakeCharacteristic, status);
        }

        // Simulate a value being written to a characteristic.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void valueWrite(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic, int status) {
            if (chromeCharacteristic == null && sRememberedCharacteristic == null) {
                throw new IllegalArgumentException(
                        "rememberCharacteristic wasn't called previously.");
            }

            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (chromeCharacteristic == null)
                            ? sRememberedCharacteristic
                            : (FakeBluetoothGattCharacteristic)
                                    chromeCharacteristic.mCharacteristic;

            fakeCharacteristic.mService.mDevice.mGattCallback.onCharacteristicWrite(
                    fakeCharacteristic, status);
        }

        // Cause subsequent notification of a characteristic to fail synchronously.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void setCharacteristicNotificationWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic) {
            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

            fakeCharacteristic
                            .mService
                            .mDevice
                            .mGatt
                            .mSetCharacteristicNotificationWillFailSynchronouslyOnce =
                    true;
        }

        // Cause subsequent value read of a characteristic to fail synchronously.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void setReadCharacteristicWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic) {
            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

            fakeCharacteristic.mService.mDevice.mGatt.mReadCharacteristicWillFailSynchronouslyOnce =
                    true;
        }

        // Cause subsequent value write of a characteristic to fail synchronously.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void setWriteCharacteristicWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic) {
            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

            fakeCharacteristic
                            .mService
                            .mDevice
                            .mGatt
                            .mWriteCharacteristicWillFailSynchronouslyOnce =
                    true;
        }

        // Create a descriptor and add it to this characteristic.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void addDescriptor(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic, String uuidString) {
            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;
            UUID uuid = UUID.fromString(uuidString);

            fakeCharacteristic.mDescriptors.add(
                    new FakeBluetoothGattDescriptor(fakeCharacteristic, uuid));
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothGattCharacteristicWrapper overrides:

        @Override
        public List<BluetoothGattDescriptorWrapper> getDescriptors() {
            return mDescriptors;
        }

        @Override
        public int getInstanceId() {
            return mInstanceId;
        }

        @Override
        public int getProperties() {
            return mProperties;
        }

        @Override
        public UUID getUuid() {
            return mUuid;
        }

        @Override
        public byte[] getValue() {
            return mValue;
        }

        @Override
        public boolean setValue(byte[] value) {
            mValue = value;
            return true;
        }

        @Override
        public void setWriteType(int writeType) {
            mWriteType = writeType;
        }
    }

    /** Fakes android.bluetooth.BluetoothGattDescriptor. */
    static class FakeBluetoothGattDescriptor extends BluetoothGattDescriptorWrapper {
        final FakeBluetoothGattCharacteristic mCharacteristic;
        final UUID mUuid;
        byte[] mValue;
        static FakeBluetoothGattDescriptor sRememberedDescriptor;

        public FakeBluetoothGattDescriptor(
                FakeBluetoothGattCharacteristic characteristic, UUID uuid) {
            super(null, null);
            mCharacteristic = characteristic;
            mUuid = uuid;
            mValue = new byte[0];
        }

        // Implements BluetoothTestAndroid::RememberDescriptorForSubsequentAction.
        @CalledByNative("FakeBluetoothGattDescriptor")
        private static void rememberDescriptorForSubsequentAction(
                ChromeBluetoothRemoteGattDescriptor chromeDescriptor) {
            sRememberedDescriptor = (FakeBluetoothGattDescriptor) chromeDescriptor.mDescriptor;
        }

        // Simulate a value being read from a descriptor.
        @CalledByNative("FakeBluetoothGattDescriptor")
        private static void valueRead(
                ChromeBluetoothRemoteGattDescriptor chromeDescriptor, int status, byte[] value) {
            if (chromeDescriptor == null && sRememberedDescriptor == null) {
                throw new IllegalArgumentException("rememberDescriptor wasn't called previously.");
            }

            FakeBluetoothGattDescriptor fakeDescriptor =
                    (chromeDescriptor == null)
                            ? sRememberedDescriptor
                            : (FakeBluetoothGattDescriptor) chromeDescriptor.mDescriptor;

            fakeDescriptor.mValue = value;
            fakeDescriptor.mCharacteristic.mService.mDevice.mGattCallback.onDescriptorRead(
                    fakeDescriptor, status);
        }

        // Simulate a value being written to a descriptor.
        @CalledByNative("FakeBluetoothGattDescriptor")
        private static void valueWrite(
                ChromeBluetoothRemoteGattDescriptor chromeDescriptor, int status) {
            if (chromeDescriptor == null && sRememberedDescriptor == null) {
                throw new IllegalArgumentException("rememberDescriptor wasn't called previously.");
            }

            FakeBluetoothGattDescriptor fakeDescriptor =
                    (chromeDescriptor == null)
                            ? sRememberedDescriptor
                            : (FakeBluetoothGattDescriptor) chromeDescriptor.mDescriptor;

            fakeDescriptor.mCharacteristic.mService.mDevice.mGattCallback.onDescriptorWrite(
                    fakeDescriptor, status);
        }

        // Cause subsequent value read of a descriptor to fail synchronously.
        @CalledByNative("FakeBluetoothGattDescriptor")
        private static void setReadDescriptorWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattDescriptor chromeDescriptor) {
            FakeBluetoothGattDescriptor fakeDescriptor =
                    (FakeBluetoothGattDescriptor) chromeDescriptor.mDescriptor;

            fakeDescriptor
                            .mCharacteristic
                            .mService
                            .mDevice
                            .mGatt
                            .mReadDescriptorWillFailSynchronouslyOnce =
                    true;
        }

        // Cause subsequent value write of a descriptor to fail synchronously.
        @CalledByNative("FakeBluetoothGattDescriptor")
        private static void setWriteDescriptorWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattDescriptor chromeDescriptor) {
            FakeBluetoothGattDescriptor fakeDescriptor =
                    (FakeBluetoothGattDescriptor) chromeDescriptor.mDescriptor;

            fakeDescriptor
                            .mCharacteristic
                            .mService
                            .mDevice
                            .mGatt
                            .mWriteDescriptorWillFailSynchronouslyOnce =
                    true;
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothGattDescriptorWrapper overrides:

        @Override
        public BluetoothGattCharacteristicWrapper getCharacteristic() {
            return mCharacteristic;
        }

        @Override
        public UUID getUuid() {
            return mUuid;
        }

        @Override
        public byte[] getValue() {
            return mValue;
        }

        @Override
        public boolean setValue(byte[] value) {
            mValue = value;
            return true;
        }
    }

    static class FakeBluetoothSocket extends BluetoothSocketWrapper {
        private static final int BUFFER_SIZE = 8192;

        private final byte[] mInputByteBuffer = new byte[BUFFER_SIZE];
        private final FakeSocketInputStream mInputStream =
                new FakeSocketInputStream(mInputByteBuffer, this);
        private final FakeSocketOutputStream mOutputStream = new FakeSocketOutputStream(this);

        private volatile String mNextOperationExceptionMessage;

        private volatile boolean mIsConnected;

        FakeBluetoothSocket() {
            super(null);
        }

        // Simulates an IOException.
        @CalledByNative("FakeBluetoothSocket")
        private static void setNextOperationExceptionMessage(
                ChromeBluetoothSocket chromeSocket, @JniType("std::string") String message) {
            FakeBluetoothSocket fakeSocket = (FakeBluetoothSocket) chromeSocket.mSocket;
            fakeSocket.mNextOperationExceptionMessage = message;
        }

        // Simulates received data.
        @CalledByNative("FakeBluetoothSocket")
        private static void setReceivedBytes(ChromeBluetoothSocket chromeSocket, byte[] buffer) {
            assert buffer.length <= BUFFER_SIZE;
            FakeBluetoothSocket fakeSocket = (FakeBluetoothSocket) chromeSocket.mSocket;
            System.arraycopy(buffer, 0, fakeSocket.mInputByteBuffer, 0, buffer.length);
        }

        // Obtains sent data.
        @CalledByNative("FakeBluetoothSocket")
        private static byte[] getSentBytes(ChromeBluetoothSocket chromeSocket) {
            FakeBluetoothSocket fakeSocket = (FakeBluetoothSocket) chromeSocket.mSocket;
            return fakeSocket.mOutputStream.toByteArray();
        }

        private void throwIfFailNextOperation() throws IOException {
            if (mNextOperationExceptionMessage == null) {
                return;
            }
            String exceptionMessage = mNextOperationExceptionMessage;
            mNextOperationExceptionMessage = null;
            throw new IOException(exceptionMessage);
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothSocketWrapper overrides:

        @Override
        public void connect() throws IOException {
            throwIfFailNextOperation();

            mIsConnected = true;
        }

        @Override
        public boolean isConnected() {
            return mIsConnected;
        }

        @Override
        public InputStream getInputStream() {
            return mInputStream;
        }

        @Override
        public OutputStream getOutputStream() {
            return mOutputStream;
        }

        @Override
        public void close() throws IOException {
            mIsConnected = false;
            throwIfFailNextOperation();
        }
    }

    private static class FakeSocketInputStream extends InputStream {
        private final FakeBluetoothSocket mSocket;
        private final ByteArrayInputStream mInputStream;

        FakeSocketInputStream(byte[] buffer, FakeBluetoothSocket socket) {
            mInputStream = new ByteArrayInputStream(buffer);
            mSocket = socket;
        }

        @Override
        public int read() throws IOException {
            mSocket.throwIfFailNextOperation();
            return mInputStream.read();
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            mSocket.throwIfFailNextOperation();
            return mInputStream.read(b, off, len);
        }
    }

    private static class FakeSocketOutputStream extends OutputStream {
        private final FakeBluetoothSocket mSocket;
        private final ByteArrayOutputStream mOutputStream;

        private FakeSocketOutputStream(FakeBluetoothSocket socket) {
            mSocket = socket;
            mOutputStream = new ByteArrayOutputStream();
        }

        @Override
        public void write(int b) throws IOException {
            mSocket.throwIfFailNextOperation();
            mOutputStream.write(b);
        }

        @Override
        public void write(byte[] b, int off, int len) throws IOException {
            mSocket.throwIfFailNextOperation();
            mOutputStream.write(b, off, len);
        }

        @Override
        public void flush() throws IOException {
            mSocket.throwIfFailNextOperation();
            mOutputStream.flush();
        }

        private byte[] toByteArray() {
            return mOutputStream.toByteArray();
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothTestAndroid C++ methods declared for access from java:
    @NativeMethods
    interface Natives {

        // Bind to BluetoothTestAndroid::PostTaskFromJava.
        void postTaskFromJava(long nativeBluetoothTestAndroid, Runnable r);

        // Bind to BluetoothTestAndroid::PostDelayedTaskFromJava.
        void postDelayedTaskFromJava(long nativeBluetoothTestAndroid, Runnable r, long delayMillis);

        // Binds to BluetoothTestAndroid::OnFakeAdapterStateChanged.
        void onFakeAdapterStateChanged(long nativeBluetoothTestAndroid, boolean powered);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothDeviceConnectGattCalled.
        void onFakeBluetoothDeviceConnectGattCalled(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattDisconnect.
        void onFakeBluetoothGattDisconnect(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattClose.
        void onFakeBluetoothGattClose(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattDiscoverServices.
        void onFakeBluetoothGattDiscoverServices(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattSetCharacteristicNotification.
        void onFakeBluetoothGattSetCharacteristicNotification(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattReadCharacteristic.
        void onFakeBluetoothGattReadCharacteristic(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattWriteCharacteristic.
        void onFakeBluetoothGattWriteCharacteristic(long nativeBluetoothTestAndroid, byte[] value);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattReadDescriptor.
        void onFakeBluetoothGattReadDescriptor(long nativeBluetoothTestAndroid);

        // Binds to BluetoothTestAndroid::OnFakeBluetoothGattWriteDescriptor.
        void onFakeBluetoothGattWriteDescriptor(long nativeBluetoothTestAndroid, byte[] value);
    }
}
