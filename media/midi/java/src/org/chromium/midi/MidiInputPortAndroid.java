// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.annotation.TargetApi;
import android.media.midi.MidiDevice;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Build;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.io.IOException;

// Note "InputPort" is named in the Web MIDI manner. It corresponds to MidiOutputPort class in the
// Android API.
/**
 * A MidiInputPortAndroid provides data to the associated midi::MidiInputPortAndroid object.
 */
@JNINamespace("midi")
@TargetApi(Build.VERSION_CODES.M)
class MidiInputPortAndroid {
    /**
     * The underlying port.
     */
    private MidiOutputPort mPort;
    /**
     * A pointer to a midi::MidiInputPortAndroid object.
     */
    private long mNativeReceiverPointer;
    /**
     * The device this port belongs to.
     */
    private final MidiDevice mDevice;
    /**
     * The index of the port in the associated device.
     */
    private final int mIndex;

    /**
     * constructor
     * @param device the device this port belongs to.
     * @param index the index of the port in the associated device.
     */
    MidiInputPortAndroid(MidiDevice device, int index) {
        mDevice = device;
        mIndex = index;
    }

    /**
     * Registers this object to the underlying port so as to the C++ function will be called with
     * the given C++ object when data arrives.
     * @param nativeReceiverPointer a pointer to a midi::MidiInputPortAndroid object.
     * @return true if this operation succeeds or the port is already open.
     */
    @CalledByNative
    boolean open(long nativeReceiverPointer) {
        if (mPort != null) {
            return true;
        }
        mPort = mDevice.openOutputPort(mIndex);
        if (mPort == null) {
            return false;
        }
        mNativeReceiverPointer = nativeReceiverPointer;
        mPort.connect(new MidiReceiver() {
            @Override
            public void onSend(byte[] bs, int offset, int count, long timestamp) {
                synchronized (MidiInputPortAndroid.this) {
                    if (mPort == null) {
                        return;
                    }
                    MidiInputPortAndroidJni.get().onData(
                            mNativeReceiverPointer, bs, offset, count, timestamp);
                }
            }
        });
        return true;
    }

    /**
     * Closes the port.
     */
    @CalledByNative
    synchronized void close() {
        if (mPort == null) {
            return;
        }
        try {
            mPort.close();
        } catch (IOException e) {
            // We can do nothing here. Just ignore the error.
        }
        mNativeReceiverPointer = 0;
        mPort = null;
    }

    @NativeMethods
    interface Natives {
        void onData(
                long nativeMidiInputPortAndroid, byte[] bs, int offset, int count, long timestamp);
    }
}
