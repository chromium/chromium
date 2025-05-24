// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import android.bluetooth.BluetoothSocket;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Wraps android.bluetooth.BluetoothSocket. */
@NullMarked
public class BluetoothSocketWrapper implements AutoCloseable {
    private final BluetoothSocket mSocket;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public BluetoothSocketWrapper(BluetoothSocket socket) {
        mSocket = socket;
    }

    public void connect() throws IOException {
        mSocket.connect();
    }

    public boolean isConnected() {
        return mSocket.isConnected();
    }

    public InputStream getInputStream() throws IOException {
        return mSocket.getInputStream();
    }

    public OutputStream getOutputStream() throws IOException {
        return mSocket.getOutputStream();
    }

    @Override
    public void close() throws IOException {
        mSocket.close();
    }
}
