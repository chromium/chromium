// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.media.midi.MidiDevice;
import android.media.midi.MidiInputPort;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;

/** A class implementing midi::MidiOutputPortAndroid functionality. */
// Note "OutputPort" is named in the Web MIDI manner. It corresponds to MidiInputPort class in the
// Android API.
@JNINamespace("midi")
@NullMarked
class MidiOutputPortAndroid {
    /** The underlying port. */
    private volatile @Nullable MidiInputPort mPort;

    /** The device this port belongs to. */
    private final MidiDevice mDevice;

    /** The index of the port in the associated device. */
    private final int mIndex;

    private static final String TAG = "MidiOutPortAndroid";

    /**
     * constructor
     *
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
        try {
            mPort = mDevice.openInputPort(mIndex);
            return mPort != null;
        } catch (SecurityException | IllegalArgumentException e) {
            Log.w(TAG, "Failed to open port", e);
            return false;
        }
    }

    /** Sends the data to the underlying output port. */
    @CalledByNative
    void send(byte[] bs) {
        MidiInputPort localPort = mPort;
        if (localPort == null) {
            return;
        }
        try {
            localPort.send(bs, 0, bs.length);
        } catch (IOException e) {
            // We can do nothing here. Just ignore the error.
            Log.e(TAG, "MidiOutputPortAndroid.send: " + e);
        }
    }

    /** Closes the port. */
    @CalledByNative
    void close() {
        MidiInputPort localPort;

        synchronized (this) {
            if (mPort == null) {
                return;
            }
            localPort = mPort;
            mPort = null;
        }

        try {
            localPort.close();
        } catch (IOException e) {
            // We can do nothing here. Just ignore the error.
        }
    }
}
