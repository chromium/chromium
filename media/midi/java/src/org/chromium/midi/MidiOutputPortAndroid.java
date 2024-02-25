// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.media.midi.MidiDevice;
import android.media.midi.MidiInputPort;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

import java.io.IOException;

/** A class implementing midi::MidiOutputPortAndroid functionality. */
// Note "OutputPort" is named in the Web MIDI manner. It corresponds to MidiInputPort class in the
// Android API.
@JNINamespace("midi")
class MidiOutputPortAndroid {
    /** The underlying port. */
    private MidiInputPort mPort;

    /** The device this port belongs to. */
    private final MidiDevice mDevice;

    /** The index of the port in the associated device. */
    private final int mIndex;

    private static final String TAG = "midi";

    /**
     * constructor
     * @param device The device this port belongs to.
     * @param index The index of the port in the associated device.
     */
    MidiOutputPortAndroid(MidiDevice device, int index) {
        mDevice = device;
        mIndex = index;
    }

    /**
     * Opens this port.
     * @return true when the operation succeeds or the port is already open.
     */
    @CalledByNative
    boolean open() {
        if (mPort != null) {
            return true;
        }
        mPort = mDevice.openInputPort(mIndex);
        return mPort != null;
    }

    /** Sends the data to the underlying output port. */
    @CalledByNative
    void send(byte[] bs) {
        if (mPort == null) {
            return;
        }
        try {
            mPort.send(bs, 0, bs.length);
        } catch (IOException e) {
            // We can do nothing here. Just ignore the error.
            Log.e(TAG, "MidiOutputPortAndroid.send: " + e);
        }
    }

    /** Closes the port. */
    @CalledByNative
    void close() {
        if (mPort == null) {
            return;
        }
        try {
            mPort.close();
        } catch (IOException e) {
            // We can do nothing here. Just ignore the error.
        }
        mPort = null;
    }
}
