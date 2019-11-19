// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.ParcelUuid;
import android.test.mock.MockContext;
import android.util.SparseArray;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.location.LocationUtils;
import org.chromium.device.bluetooth.test.TestRSSI;
import org.chromium.device.bluetooth.test.TestTxPower;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Fake implementations of android.bluetooth.* classes for testing.
 *
 * Fakes are contained in a single file to simplify code. Only one C++ file may
 * access a Java file via JNI, and all of these classes are accessed by
 * bluetooth_test_android.cc. The alternative would be a C++ .h, .cc file for
 * each of these classes.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.M)
class Fakes {
    private static final String TAG = "Bluetooth";

    // Android uses Integer.MIN_VALUE to signal no Tx Power in advertisement
    // packet.
    // https://developer.android.com/reference/android/bluetooth/le/ScanRecord.html#getTxPowerLevel()
    private static final int NO_TX_POWER = Integer.MIN_VALUE;

    /**
     * Sets the factory for LocationUtils to return an instance whose
     * hasAndroidLocationPermission and isSystemLocationSettingEnabled return
     * values depend on |hasPermission| and |isEnabled| respectively.
     */
    @CalledByNative
    public static void setLocationServicesState(
            final boolean hasPermission, final boolean isEnabled) {
        LocationUtils.setFactory(new LocationUtils.Factory() {
            @Override
            public LocationUtils create() {
                return new LocationUtils() {
                    @Override
                    public boolean hasAndroidLocationPermission() {
                        return hasPermission;
                    }

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
     * runOnUiThread uses nativePostTaskFromJava. This allows java to post tasks to the
     * message loop that the test is using rather than to the Java message loop which
     * is not running during tests.
     */
    @CalledByNative
    public static void initFakeThreadUtilsWrapper(final long nativeBluetoothTestAndroid) {
        Wrappers.ThreadUtilsWrapper.setFactory(new Wrappers.ThreadUtilsWrapper.Factory() {
            @Override
            public Wrappers.ThreadUtilsWrapper create() {
                return new Wrappers.ThreadUtilsWrapper() {
                    @Override
                    public void runOnUiThread(Runnable r) {
                        nativePostTaskFromJava(nativeBluetoothTestAndroid, r);
                    }
                };
            }
        });
    }

    @CalledByNative
    public static void runRunnable(Runnable r) {
        r.run();
    }

    /**
     * Fakes android.bluetooth.BluetoothAdapter.
     */
    static class FakeBluetoothAdapter extends Wrappers.BluetoothAdapterWrapper {
        private final FakeContext mFakeContext;
        private final FakeBluetoothLeScanner mFakeScanner;
        private boolean mPowered = true;
        final long mNativeBluetoothTestAndroid;

        /**
         * Creates a FakeBluetoothAdapter.
         */
        @CalledByNative("FakeBluetoothAdapter")
        public static FakeBluetoothAdapter create(long nativeBluetoothTestAndroid) {
            Log.v(TAG, "FakeBluetoothAdapter created.");
            return new FakeBluetoothAdapter(nativeBluetoothTestAndroid);
        }

        private FakeBluetoothAdapter(long nativeBluetoothTestAndroid) {
            super(null, new FakeContext());
            mNativeBluetoothTestAndroid = nativeBluetoothTestAndroid;
            mFakeContext = (FakeContext) mContext;
            mFakeScanner = new FakeBluetoothLeScanner();
        }

        /**
         * Creates and discovers a new device.
         */
        @CalledByNative("FakeBluetoothAdapter")
        public void simulateLowEnergyDevice(int deviceOrdinal) {
            if (mFakeScanner == null) {
                return;
            }

            switch (deviceOrdinal) {
                case 1: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("00001800-0000-1000-8000-00805f9b34fb"));
                    uuids.add(ParcelUuid.fromString("00001801-0000-1000-8000-00805f9b34fb"));

                    HashMap<ParcelUuid, byte[]> serviceData = new HashMap<>();
                    serviceData.put(ParcelUuid.fromString("0000180d-0000-1000-8000-00805f9b34fb"),
                            new byte[] {1});

                    SparseArray<byte[]> manufacturerData = new SparseArray<>();
                    manufacturerData.put(0x00E0, new byte[] {0x01, 0x02, 0x03, 0x04});

                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(this, "01:00:00:90:1E:BE",
                                                       "FakeBluetoothDevice"),
                                    "FakeBluetoothDevice", TestRSSI.LOWEST, uuids,
                                    TestTxPower.LOWEST, serviceData, manufacturerData));
                    break;
                }
                case 2: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("00001802-0000-1000-8000-00805f9b34fb"));
                    uuids.add(ParcelUuid.fromString("00001803-0000-1000-8000-00805f9b34fb"));

                    HashMap<ParcelUuid, byte[]> serviceData = new HashMap<>();
                    serviceData.put(ParcelUuid.fromString("0000180d-0000-1000-8000-00805f9b34fb"),
                            new byte[] {});
                    serviceData.put(ParcelUuid.fromString("00001802-0000-1000-8000-00805f9b34fb"),
                            new byte[] {0, 2});

                    SparseArray<byte[]> manufacturerData = new SparseArray<>();
                    manufacturerData.put(0x00E0, new byte[] {});

                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(this, "01:00:00:90:1E:BE",
                                                       "FakeBluetoothDevice"),
                                    "Local Device Name", TestRSSI.LOWER, uuids, TestTxPower.LOWER,
                                    serviceData, manufacturerData));
                    break;
                }
                case 3: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(
                                    new FakeBluetoothDevice(this, "01:00:00:90:1E:BE", ""),
                                    "Local Device Name", TestRSSI.LOW, uuids, NO_TX_POWER, null,
                                    null));

                    break;
                }
                case 4: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(
                                    new FakeBluetoothDevice(this, "02:00:00:8B:74:63", ""),
                                    "Local Device Name", TestRSSI.MEDIUM, uuids, NO_TX_POWER, null,
                                    null));

                    break;
                }
                case 5: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(
                                    new FakeBluetoothDevice(this, "01:00:00:90:1E:BE", null),
                                    "Local Device Name", TestRSSI.HIGH, uuids, NO_TX_POWER, null,
                                    null));
                    break;
                }
                case 6: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(
                                    new FakeBluetoothDevice(this, "02:00:00:8B:74:63", null),
                                    "Local Device Name", TestRSSI.LOWEST, uuids, NO_TX_POWER, null,
                                    null));
                    break;
                }
                case 7: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb"));

                    HashMap<ParcelUuid, byte[]> serviceData = new HashMap<>();
                    serviceData.put(ParcelUuid.fromString("f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb"),
                            new byte[] {0, 20});

                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(
                                                       this, "01:00:00:90:1E:BE", "U2F FakeDevice"),
                                    "Local Device Name", TestRSSI.LOWEST, uuids, NO_TX_POWER,
                                    serviceData, null));
                    break;
                }
            }
        }

        @CalledByNative("FakeBluetoothAdapter")
        public void forceIllegalStateException() {
            if (mFakeScanner != null) {
                mFakeScanner.forceIllegalStateException();
            }
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothAdapterWrapper overrides:

        @Override
        public boolean disable() {
            // android.bluetooth.BluetoothAdapter::disable() is an async call, so we simulate this
            // by posting a task to the UI thread.
            nativePostTaskFromJava(mNativeBluetoothTestAndroid, new Runnable() {
                @Override
                public void run() {
                    mPowered = false;
                    nativeOnFakeAdapterStateChanged(mNativeBluetoothTestAndroid, false);
                }
            });
            return true;
        }

        @Override
        public boolean enable() {
            // android.bluetooth.BluetoothAdapter::enable() is an async call, so we simulate this by
            // posting a task to the UI thread.
            nativePostTaskFromJava(mNativeBluetoothTestAndroid, new Runnable() {
                @Override
                public void run() {
                    mPowered = true;
                    nativeOnFakeAdapterStateChanged(mNativeBluetoothTestAndroid, true);
                }
            });
            return true;
        }

        @Override
        public String getAddress() {
            return "A1:B2:C3:D4:E5:F6";
        }

        @Override
        public Wrappers.BluetoothLeScannerWrapper getBluetoothLeScanner() {
            if (isEnabled()) {
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
    }

    /**
     * Fakes android.content.Context by extending MockContext.
     */
    static class FakeContext extends MockContext {
        public FakeContext() {
            super();
        }

        @Override
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            return null;
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {}
    }

    /**
     * Fakes android.bluetooth.le.BluetoothLeScanner.
     */
    static class FakeBluetoothLeScanner extends Wrappers.BluetoothLeScannerWrapper {
        public Wrappers.ScanCallbackWrapper mScanCallback;
        private boolean mThrowException;

        private FakeBluetoothLeScanner() {
            super(null);
        }

        @Override
        public void startScan(List<ScanFilter> filters, int scanSettingsScanMode,
                Wrappers.ScanCallbackWrapper callback) {
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
        public void stopScan(Wrappers.ScanCallbackWrapper callback) {
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

    /**
     * Fakes android.bluetooth.le.ScanResult
     */
    static class FakeScanResult extends Wrappers.ScanResultWrapper {
        private final FakeBluetoothDevice mDevice;
        private final String mLocalName;
        private final int mRssi;
        private final int mTxPower;
        private final ArrayList<ParcelUuid> mUuids;
        private final Map<ParcelUuid, byte[]> mServiceData;
        private final SparseArray<byte[]> mManufacturerData;

        FakeScanResult(FakeBluetoothDevice device, String localName, int rssi,
                ArrayList<ParcelUuid> uuids, int txPower, Map<ParcelUuid, byte[]> serviceData,
                SparseArray<byte[]> manufacturerData) {
            super(null);
            mDevice = device;
            mLocalName = localName;
            mRssi = rssi;
            mUuids = uuids;
            mTxPower = txPower;
            mServiceData = serviceData;
            mManufacturerData = manufacturerData;
        }

        @Override
        public Wrappers.BluetoothDeviceWrapper getDevice() {
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
    }

    /**
     * Fakes android.bluetooth.BluetoothDevice.
     */
    static class FakeBluetoothDevice extends Wrappers.BluetoothDeviceWrapper {
        final FakeBluetoothAdapter mAdapter;
        private String mAddress;
        private String mName;
        final FakeBluetoothGatt mGatt;
        private Wrappers.BluetoothGattCallbackWrapper mGattCallback;

        static FakeBluetoothDevice sRememberedDevice;

        public FakeBluetoothDevice(FakeBluetoothAdapter adapter, String address, String name) {
            super(null);
            mAdapter = adapter;
            mAddress = address;
            mName = name;
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
            fakeDevice.mGattCallback.onConnectionStateChange(status, connected
                            ? android.bluetooth.BluetoothProfile.STATE_CONNECTED
                            : android.bluetooth.BluetoothProfile.STATE_DISCONNECTED);
        }

        // Create a call to onServicesDiscovered on the |chrome_device| using parameter
        // |status|.
        @CalledByNative("FakeBluetoothDevice")
        private static void servicesDiscovered(
                ChromeBluetoothDevice chromeDevice, int status, String uuidsSpaceDelimited) {
            if (chromeDevice == null && sRememberedDevice == null) {
                throw new IllegalArgumentException("rememberDevice wasn't called previously.");
            }

            FakeBluetoothDevice fakeDevice = (chromeDevice == null)
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
                    fakeDevice.mGatt.mServices.add(new FakeBluetoothGattService(
                            fakeDevice, UUID.fromString(uuid), instanceId));
                }
            }

            fakeDevice.mGattCallback.onServicesDiscovered(status);
        }

        // -----------------------------------------------------------------------------------------
        // Wrappers.BluetoothDeviceWrapper overrides:

        @Override
        public Wrappers.BluetoothGattWrapper connectGatt(Context context, boolean autoConnect,
                Wrappers.BluetoothGattCallbackWrapper callback, int transport) {
            if (mGattCallback != null && mGattCallback != callback) {
                throw new IllegalArgumentException(
                        "BluetoothGattWrapper doesn't support calls to connectGatt() with "
                        + "multiple distinct callbacks.");
            }
            nativeOnFakeBluetoothDeviceConnectGattCalled(mAdapter.mNativeBluetoothTestAndroid);
            mGattCallback = callback;
            return mGatt;
        }

        @Override
        public String getAddress() {
            return mAddress;
        }

        @Override
        public int getBluetoothClass_getDeviceClass() {
            return Wrappers.DEVICE_CLASS_UNSPECIFIED;
        }

        @Override
        public int getBondState() {
            return BluetoothDevice.BOND_NONE;
        }

        @Override
        public String getName() {
            return mName;
        }
    }

    /**
     * Fakes android.bluetooth.BluetoothGatt.
     */
    static class FakeBluetoothGatt extends Wrappers.BluetoothGattWrapper {
        final FakeBluetoothDevice mDevice;
        final ArrayList<Wrappers.BluetoothGattServiceWrapper> mServices;
        boolean mReadCharacteristicWillFailSynchronouslyOnce;
        boolean mSetCharacteristicNotificationWillFailSynchronouslyOnce;
        boolean mWriteCharacteristicWillFailSynchronouslyOnce;
        boolean mReadDescriptorWillFailSynchronouslyOnce;
        boolean mWriteDescriptorWillFailSynchronouslyOnce;

        public FakeBluetoothGatt(FakeBluetoothDevice device) {
            super(null, null);
            mDevice = device;
            mServices = new ArrayList<Wrappers.BluetoothGattServiceWrapper>();
        }

        @Override
        public void disconnect() {
            nativeOnFakeBluetoothGattDisconnect(mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }

        @Override
        public void close() {
            nativeOnFakeBluetoothGattClose(mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }

        @Override
        public void discoverServices() {
            nativeOnFakeBluetoothGattDiscoverServices(mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }

        @Override
        public List<Wrappers.BluetoothGattServiceWrapper> getServices() {
            return mServices;
        }

        @Override
        boolean readCharacteristic(Wrappers.BluetoothGattCharacteristicWrapper characteristic) {
            if (mReadCharacteristicWillFailSynchronouslyOnce) {
                mReadCharacteristicWillFailSynchronouslyOnce = false;
                return false;
            }
            nativeOnFakeBluetoothGattReadCharacteristic(
                    mDevice.mAdapter.mNativeBluetoothTestAndroid);
            return true;
        }

        @Override
        boolean setCharacteristicNotification(
                Wrappers.BluetoothGattCharacteristicWrapper characteristic, boolean enable) {
            if (mSetCharacteristicNotificationWillFailSynchronouslyOnce) {
                mSetCharacteristicNotificationWillFailSynchronouslyOnce = false;
                return false;
            }
            nativeOnFakeBluetoothGattSetCharacteristicNotification(
                    mDevice.mAdapter.mNativeBluetoothTestAndroid);
            return true;
        }

        @Override
        boolean writeCharacteristic(Wrappers.BluetoothGattCharacteristicWrapper characteristic) {
            if (mWriteCharacteristicWillFailSynchronouslyOnce) {
                mWriteCharacteristicWillFailSynchronouslyOnce = false;
                return false;
            }
            nativeOnFakeBluetoothGattWriteCharacteristic(
                    mDevice.mAdapter.mNativeBluetoothTestAndroid, characteristic.getValue());
            return true;
        }

        @Override
        boolean readDescriptor(Wrappers.BluetoothGattDescriptorWrapper descriptor) {
            if (mReadDescriptorWillFailSynchronouslyOnce) {
                mReadDescriptorWillFailSynchronouslyOnce = false;
                return false;
            }
            nativeOnFakeBluetoothGattReadDescriptor(mDevice.mAdapter.mNativeBluetoothTestAndroid);
            return true;
        }

        @Override
        boolean writeDescriptor(Wrappers.BluetoothGattDescriptorWrapper descriptor) {
            if (mWriteDescriptorWillFailSynchronouslyOnce) {
                mWriteDescriptorWillFailSynchronouslyOnce = false;
                return false;
            }
            nativeOnFakeBluetoothGattWriteDescriptor(
                    mDevice.mAdapter.mNativeBluetoothTestAndroid, descriptor.getValue());
            return true;
        }
    }

    /**
     * Fakes android.bluetooth.BluetoothGattService.
     */
    static class FakeBluetoothGattService extends Wrappers.BluetoothGattServiceWrapper {
        final FakeBluetoothDevice mDevice;
        final int mInstanceId;
        final UUID mUuid;
        final ArrayList<Wrappers.BluetoothGattCharacteristicWrapper> mCharacteristics;

        public FakeBluetoothGattService(FakeBluetoothDevice device, UUID uuid, int instanceId) {
            super(null, null);
            mDevice = device;
            mUuid = uuid;
            mInstanceId = instanceId;
            mCharacteristics = new ArrayList<Wrappers.BluetoothGattCharacteristicWrapper>();
        }

        // Create a characteristic and add it to this service.
        @CalledByNative("FakeBluetoothGattService")
        private static void addCharacteristic(
                ChromeBluetoothRemoteGattService chromeService, String uuidString, int properties) {
            FakeBluetoothGattService fakeService =
                    (FakeBluetoothGattService) chromeService.mService;
            UUID uuid = UUID.fromString(uuidString);

            int countOfDuplicateUUID = 0;
            for (Wrappers.BluetoothGattCharacteristicWrapper characteristic :
                    fakeService.mCharacteristics) {
                if (characteristic.getUuid().equals(uuid)) {
                    countOfDuplicateUUID++;
                }
            }
            fakeService.mCharacteristics.add(new FakeBluetoothGattCharacteristic(fakeService,
                    /* instanceId */ countOfDuplicateUUID, properties, uuid));
        }

        // -----------------------------------------------------------------------------------------
        // Wrappers.BluetoothGattServiceWrapper overrides:

        @Override
        public List<Wrappers.BluetoothGattCharacteristicWrapper> getCharacteristics() {
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

    /**
     * Fakes android.bluetooth.BluetoothGattCharacteristic.
     */
    static class FakeBluetoothGattCharacteristic
            extends Wrappers.BluetoothGattCharacteristicWrapper {
        final FakeBluetoothGattService mService;
        final int mInstanceId;
        final int mProperties;
        final UUID mUuid;
        byte[] mValue;
        static FakeBluetoothGattCharacteristic sRememberedCharacteristic;
        final ArrayList<Wrappers.BluetoothGattDescriptorWrapper> mDescriptors;

        public FakeBluetoothGattCharacteristic(
                FakeBluetoothGattService service, int instanceId, int properties, UUID uuid) {
            super(null, null);
            mService = service;
            mInstanceId = instanceId;
            mProperties = properties;
            mUuid = uuid;
            mValue = new byte[0];
            mDescriptors = new ArrayList<Wrappers.BluetoothGattDescriptorWrapper>();
        }

        // Simulate a characteristic value notified as changed.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void valueChanged(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic, byte[] value) {
            if (chromeCharacteristic == null && sRememberedCharacteristic == null) {
                throw new IllegalArgumentException(
                        "rememberCharacteristic wasn't called previously.");
            }

            FakeBluetoothGattCharacteristic fakeCharacteristic = (chromeCharacteristic == null)
                    ? sRememberedCharacteristic
                    : (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

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
        private static void valueRead(ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic,
                int status, byte[] value) {
            if (chromeCharacteristic == null && sRememberedCharacteristic == null) {
                throw new IllegalArgumentException(
                        "rememberCharacteristic wasn't called previously.");
            }

            FakeBluetoothGattCharacteristic fakeCharacteristic = (chromeCharacteristic == null)
                    ? sRememberedCharacteristic
                    : (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

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

            FakeBluetoothGattCharacteristic fakeCharacteristic = (chromeCharacteristic == null)
                    ? sRememberedCharacteristic
                    : (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

            fakeCharacteristic.mService.mDevice.mGattCallback.onCharacteristicWrite(
                    fakeCharacteristic, status);
        }

        // Cause subsequent notification of a characteristic to fail synchronously.
        @CalledByNative("FakeBluetoothGattCharacteristic")
        private static void setCharacteristicNotificationWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic) {
            FakeBluetoothGattCharacteristic fakeCharacteristic =
                    (FakeBluetoothGattCharacteristic) chromeCharacteristic.mCharacteristic;

            fakeCharacteristic.mService.mDevice.mGatt
                    .mSetCharacteristicNotificationWillFailSynchronouslyOnce = true;
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

            fakeCharacteristic.mService.mDevice.mGatt
                    .mWriteCharacteristicWillFailSynchronouslyOnce = true;
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
        // Wrappers.BluetoothGattCharacteristicWrapper overrides:

        @Override
        public List<Wrappers.BluetoothGattDescriptorWrapper> getDescriptors() {
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
    }

    /**
     * Fakes android.bluetooth.BluetoothGattDescriptor.
     */
    static class FakeBluetoothGattDescriptor extends Wrappers.BluetoothGattDescriptorWrapper {
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

            FakeBluetoothGattDescriptor fakeDescriptor = (chromeDescriptor == null)
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

            FakeBluetoothGattDescriptor fakeDescriptor = (chromeDescriptor == null)
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

            fakeDescriptor.mCharacteristic.mService.mDevice.mGatt
                    .mReadDescriptorWillFailSynchronouslyOnce = true;
        }

        // Cause subsequent value write of a descriptor to fail synchronously.
        @CalledByNative("FakeBluetoothGattDescriptor")
        private static void setWriteDescriptorWillFailSynchronouslyOnce(
                ChromeBluetoothRemoteGattDescriptor chromeDescriptor) {
            FakeBluetoothGattDescriptor fakeDescriptor =
                    (FakeBluetoothGattDescriptor) chromeDescriptor.mDescriptor;

            fakeDescriptor.mCharacteristic.mService.mDevice.mGatt
                    .mWriteDescriptorWillFailSynchronouslyOnce = true;
        }

        // -----------------------------------------------------------------------------------------
        // Wrappers.BluetoothGattDescriptorWrapper overrides:

        @Override
        public Wrappers.BluetoothGattCharacteristicWrapper getCharacteristic() {
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

    // ---------------------------------------------------------------------------------------------
    // BluetoothTestAndroid C++ methods declared for access from java:

    // Bind to BluetoothTestAndroid::PostTaskFromJava.
    private static native void nativePostTaskFromJava(long nativeBluetoothTestAndroid, Runnable r);

    // Binds to BluetoothTestAndroid::OnFakeAdapterStateChanged.
    private static native void nativeOnFakeAdapterStateChanged(
            long nativeBluetoothTestAndroid, boolean powered);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothDeviceConnectGattCalled.
    private static native void nativeOnFakeBluetoothDeviceConnectGattCalled(
            long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattDisconnect.
    private static native void nativeOnFakeBluetoothGattDisconnect(long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattClose.
    private static native void nativeOnFakeBluetoothGattClose(long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattDiscoverServices.
    private static native void nativeOnFakeBluetoothGattDiscoverServices(
            long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattSetCharacteristicNotification.
    private static native void nativeOnFakeBluetoothGattSetCharacteristicNotification(
            long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattReadCharacteristic.
    private static native void nativeOnFakeBluetoothGattReadCharacteristic(
            long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattWriteCharacteristic.
    private static native void nativeOnFakeBluetoothGattWriteCharacteristic(
            long nativeBluetoothTestAndroid, byte[] value);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattReadDescriptor.
    private static native void nativeOnFakeBluetoothGattReadDescriptor(
            long nativeBluetoothTestAndroid);

    // Binds to BluetoothTestAndroid::OnFakeBluetoothGattWriteDescriptor.
    private static native void nativeOnFakeBluetoothGattWriteDescriptor(
            long nativeBluetoothTestAndroid, byte[] value);
}
