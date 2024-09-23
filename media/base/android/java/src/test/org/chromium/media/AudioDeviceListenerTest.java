// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;

import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.media.AudioManager;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;

/** Tests for AudioDeviceListener. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AudioDeviceListenerTest {
    @Mock Context mContext;
    @Mock AudioDeviceSelector.Devices mDevices;
    @Mock UsbDevice mUsbDevice;
    @Mock UsbInterface mUsbInterface;
    private AudioDeviceListener mListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mListener = new AudioDeviceListener(mDevices);
        mListener.init(/* hasBluetoothPermission= */ true);

        doReturn(1).when(mUsbDevice).getInterfaceCount();
        doReturn(mUsbInterface).when(mUsbDevice).getInterface(eq(0));
        doReturn(UsbConstants.USB_CLASS_AUDIO).when(mUsbInterface).getInterfaceClass();
        doReturn(UsbConstants.USB_CLASS_COMM).when(mUsbInterface).getInterfaceSubclass();
    }

    @Test
    public void testWiredHeadsetConnectionChange() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.AudioDeviceConnectionStatus.Wired",
                        AudioDeviceListener.ConnectionStatus.CONNECTED);

        Intent intent = new Intent(AudioManager.ACTION_HEADSET_PLUG);
        intent.putExtra("state", 1);
        mListener.getWiredHeadsetReceiverForTesting().onReceive(mContext, intent);

        watcher.assertExpected();
    }

    @Test
    public void testBluetoothConnectionChange() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.AudioDeviceConnectionStatus.Bluetooth",
                        AudioDeviceListener.ConnectionStatus.CONNECTED);

        Intent intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_CONNECTED);
        mListener.getBluetoothHeadsetReceiverForTesting().onReceive(mContext, intent);

        watcher.assertExpected();
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void testBluetoothLeAudioConnectionChange() {
        BluetoothManager btManager =
                (BluetoothManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.BLUETOOTH_SERVICE);

        if (!mListener.isLeAudioSupported(btManager.getAdapter())) {
            return;
        }

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.AudioDeviceConnectionStatus.Bluetooth",
                        AudioDeviceListener.ConnectionStatus.CONNECTED);

        Intent intent =
                new Intent(
                        android.bluetooth.BluetoothLeAudio
                                .ACTION_LE_AUDIO_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_CONNECTED);
        mListener.getBluetoothHeadsetReceiverForTesting().onReceive(mContext, intent);

        watcher.assertExpected();
    }

    @Test
    public void testUsbConnectionChange() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.AudioDeviceConnectionStatus.USB",
                        AudioDeviceListener.ConnectionStatus.CONNECTED);

        Intent intent = new Intent(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        intent.putExtra(UsbManager.EXTRA_DEVICE, mUsbDevice);
        mListener.getUsbReceiverForTesting().onReceive(mContext, intent);

        watcher.assertExpected();
    }
}
