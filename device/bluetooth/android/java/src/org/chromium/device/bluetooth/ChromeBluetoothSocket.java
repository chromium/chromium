// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.device.bluetooth.wrapper.BluetoothSocketWrapper;
import org.chromium.device.bluetooth.wrapper.ThreadUtilsWrapper;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Exposes necessary android.bluetooth.BluetoothSocket as necessary for C++
 * device::BluetoothSocketAndroid. All methods except the constructor need to be called in an IO
 * thread.
 *
 * <p>Lifetime is controlled by device::BluetoothSocketAndroid.
 */
@NullMarked
public class ChromeBluetoothSocket {
    private static final String TAG = "Bluetooth";

    @VisibleForTesting final BluetoothSocketWrapper mSocket;

    private final InputStream mInputStream;
    private final OutputStream mOutputStream;

    private ChromeBluetoothSocket(BluetoothSocketWrapper socket) {
        mSocket = socket;
        try {
            // Javadoc of BluetoothSocket says we can safely obtain streams before the socket is
            // connected.
            mInputStream = new BufferedInputStream(socket.getInputStream());
            mOutputStream = new BufferedOutputStream(socket.getOutputStream());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothSocketAndroid methods implemented in java:

    /**
     * Connects the socket to the remote service.
     *
     * @return an {@link Outcome} without an exception if successful or with an exception if failed.
     */
    @CalledByNative
    private Outcome<Void> connect() {
        try {
            mSocket.connect();
            return new Outcome<>((Void) null);
        } catch (IOException e) {
            return new Outcome<>(e);
        }
    }

    /**
     * Gets if this Bluetooth socket is connected.
     *
     * @return {@code true} if connected, and {@code false} if disconnected.
     */
    @CalledByNative
    private boolean isConnected() {
        return mSocket.isConnected();
    }

    /**
     * Sends data through this Bluetooth socket.
     *
     * @param data the buffer that holds the data
     * @param offset the starting point of the data
     * @param length the length of the data
     * @return {@link Outcome} with number of bytes sent or an exception.
     */
    @CalledByNative
    private Outcome<Integer> send(byte[] data, int offset, int length) {
        ThreadUtilsWrapper.getInstance().assertOnBackgroundThread();
        try {
            mOutputStream.write(data, offset, length);
            mOutputStream.flush();
            return new Outcome<>(length);
        } catch (IOException e) {
            return new Outcome<>(e);
        }
    }

    /**
     * Reads data from this Bluetooth socket.
     *
     * @param buffer the buffer to receive the data
     * @param offset the starting position in the buffer at which the data is written
     * @param length the maximum number of bytes to read
     * @return number of bytes read if successful, or {@code -1} if failed.
     */
    @CalledByNative
    private Outcome<Integer> receive(byte[] buffer, int offset, int length) {
        ThreadUtilsWrapper.getInstance().assertOnBackgroundThread();
        try {
            return new Outcome<>(mInputStream.read(buffer, offset, length));
        } catch (IOException e) {
            return new Outcome<>(e);
        }
    }

    /** Closes this Bluetooth socket. */
    @CalledByNative
    private void close() {
        ThreadUtilsWrapper.getInstance().assertOnBackgroundThread();
        try {
            mInputStream.close();
        } catch (IOException e) {
            Log.e(TAG, "Failed to close Bluetooth socket input stream.", e);
        }
        try {
            mOutputStream.close();
        } catch (IOException e) {
            Log.e(TAG, "Failed to close Bluetooth socket output stream.", e);
        }
        try {
            mSocket.close();
        } catch (IOException e) {
            Log.e(TAG, "Failed to close Bluetooth socket.", e);
        }
    }

    /**
     * Creates a {@link ChromeBluetoothSocket}.
     *
     * @param socket the underlying {@link BluetoothSocketWrapper}.
     * @return the newly created {@link ChromeBluetoothSocket}
     */
    @CalledByNative
    private static ChromeBluetoothSocket create(BluetoothSocketWrapper socket) {
        return new ChromeBluetoothSocket(socket);
    }
}
