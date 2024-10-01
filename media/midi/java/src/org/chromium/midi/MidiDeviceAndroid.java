// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** A class implementing midi::MidiDeviceAndroid functionality. */
@JNINamespace("midi")
class MidiDeviceAndroid {
    /** The underlying device. */
    private final MidiDevice mDevice;

    /** The input ports in the device. */
    private final MidiInputPortAndroid[] mInputPorts;

    /** The output ports in the device. */
    private final MidiOutputPortAndroid[] mOutputPorts;

    /** True when the device is open. */
    private boolean mIsOpen;

    /**
     * constructor
     * @param device the underlying device
     */
    MidiDeviceAndroid(MidiDevice device) {
        mIsOpen = true;
        mDevice = device;
        // Note we use "input" and "output" in the Web MIDI manner.

        mOutputPorts = new MidiOutputPortAndroid[getInfo().getInputPortCount()];
        for (int i = 0; i < mOutputPorts.length; ++i) {
            mOutputPorts[i] = new MidiOutputPortAndroid(device, i);
        }

        mInputPorts = new MidiInputPortAndroid[getInfo().getOutputPortCount()];
        for (int i = 0; i < mInputPorts.length; ++i) {
            mInputPorts[i] = new MidiInputPortAndroid(device, i);
        }
    }

    /** Returns true when the device is open. */
    boolean isOpen() {
        return mIsOpen;
    }

    /** Closes the device. */
    void close() {
        mIsOpen = false;
        for (MidiInputPortAndroid port : mInputPorts) {
            port.close();
        }
        for (MidiOutputPortAndroid port : mOutputPorts) {
            port.close();
        }
    }

    /** Returns the underlying device. */
    MidiDevice getDevice() {
        return mDevice;
    }

    /** Returns the underlying device information. */
    MidiDeviceInfo getInfo() {
        return mDevice.getInfo();
    }

    /** Returns the manufacturer name. */
    @CalledByNative
    String getManufacturer() {
        return getProperty(MidiDeviceInfo.PROPERTY_MANUFACTURER);
    }

    /** Returns the product name. */
    @CalledByNative
    String getProduct() {
        String product = getProperty(MidiDeviceInfo.PROPERTY_PRODUCT);
        // TODO(crbug.com/40480119): Following code to use PROPERTY_NAME is a
        // workaround for a BLE MIDI device issue that Android does not provide
        // information for PROPERTY_MANUFACTURER, PROPERTY_PRODUCT, and
        // PROPERTY_VERSION. Confirmed on Android M and N.
        // See discussion at http://crbug.com/636455 and http://b/32259464.
        if (product == null || product.isEmpty()) {
            return getProperty(MidiDeviceInfo.PROPERTY_NAME);
        }
        return product;
    }

    /** Returns the version string. */
    @CalledByNative
    String getVersion() {
        return getProperty(MidiDeviceInfo.PROPERTY_VERSION);
    }

    /** Returns the associated input ports. */
    @CalledByNative
    MidiInputPortAndroid[] getInputPorts() {
        return mInputPorts;
    }

    /** Returns the associated output ports. */
    @CalledByNative
    MidiOutputPortAndroid[] getOutputPorts() {
        return mOutputPorts;
    }

    private String getProperty(String name) {
        return mDevice.getInfo().getProperties().getString(name);
    }
}
