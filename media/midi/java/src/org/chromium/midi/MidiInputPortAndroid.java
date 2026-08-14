// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.media.midi.MidiDevice;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;

// Note "InputPort" is named in the Web MIDI manner. It corresponds to MidiOutputPort class in the
// Android API.
/** A MidiInputPortAndroid provides data to the associated midi::MidiInputPortAndroid object. */
@JNINamespace("midi")
@NullMarked
class MidiInputPortAndroid {
    /** The underlying port. */
    private volatile @Nullable MidiOutputPort mPort;

    /** A pointer to a midi::MidiInputPortAndroid object. */
    private long mNativeReceiverPointer;

    /** The device this port belongs to. */
    private final MidiDevice mDevice;

    /** The index of the port in the associated device. */
    private final int mIndex;

    private static final String TAG = "MidiInputPortAndroid";

    /**
     * constructor
     *
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
        @Nullable MidiOutputPort localPort = null;
        try {
            localPort = mDevice.openOutputPort(mIndex);
            if (localPort != null) {
                synchronized (this) {
                    if (mPort != null) {
                        try {
                            localPort.close();
                        } catch (IOException innerException) {
                            // We can do nothing here. Just ignore the error.
                        }
                        return true;
                    }
                    localPort.connect(
                            new MidiReceiver() {
                                @Override
                                public void onSend(
                                        byte[] bs, int offset, int count, long timestamp) {
                                    synchronized (MidiInputPortAndroid.this) {
                                        if (mPort == null) {
                                            return;
                                        }
                                        MidiInputPortAndroidJni.get()
                                                .onData(
                                                        mNativeReceiverPointer,
                                                        bs,
                                                        offset,
                                                        count,
                                                        timestamp);
                                    }
                                }
                            });
                    mPort = localPort;
                    mNativeReceiverPointer = nativeReceiverPointer;
                }
                return true;
            }
        } catch (SecurityException | IllegalArgumentException exception) {
            Log.w(TAG, "Failed to open or connect port", exception);
            if (localPort != null) {
                try {
                    localPort.close();
                } catch (IOException innerException) {
                    // We can do nothing here. Just ignore the error.
                }
            }
        }
        return false;
    }

    /** Closes the port. */
    @CalledByNative
    void close() {
        MidiOutputPort localPort;

        synchronized (this) {
            if (mPort == null) {
                return;
            }
            localPort = mPort;
            mNativeReceiverPointer = 0;
            mPort = null;
        }

        try {
            localPort.close();
        } catch (IOException e) {
            // We can do nothing here. Just ignore the error.
        }
    }

    @NativeMethods
    interface Natives {
        void onData(
                long nativeMidiInputPortAndroid, byte[] bs, int offset, int count, long timestamp);
    }
}
