// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;

/**
 * This class is used to create a single spare ChildProcessConnection (usually early on during
 * start-up) that can then later be retrieved when a connection to a service is needed.
 */
public class SpareChildConnection {
    private static final String TAG = "SpareChildConn";

    // The allocator used to create the connection.
    private final ChildConnectionAllocator mConnectionAllocator;

    // The actual spare connection.
    private ChildProcessConnection mConnection;

    // True when there is a spare connection and it is bound.
    private boolean mConnectionReady;

    // The callback that should be called when the connection becomes bound. Set when the connection
    // is retrieved.
    private ChildProcessConnection.ServiceCallback mConnectionServiceCallback;

    /** Creates and binds a ChildProcessConnection using the specified parameters. */
    public SpareChildConnection(
            Context context, ChildConnectionAllocator connectionAllocator, Bundle serviceBundle) {
        assert LauncherThread.runningOnLauncherThread();

        mConnectionAllocator = connectionAllocator;

        ChildProcessConnection.ServiceCallback serviceCallback =
                new ChildProcessConnection.ServiceCallback() {
                    @Override
                    public void onChildStarted() {
                        assert LauncherThread.runningOnLauncherThread();
                        mConnectionReady = true;
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildStarted();
                            clearConnection();
                        }
                        // If there is no chained callback, that means the spare connection has not
                        // been used yet. It will be cleared when used.
                    }

                    @Override
                    public void onChildStartFailed(ChildProcessConnection connection) {
                        assert LauncherThread.runningOnLauncherThread();
                        Log.e(TAG, "Failed to warm up the spare sandbox service");
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildStartFailed(connection);
                        }
                        clearConnection();
                    }

                    @Override
                    public void onChildProcessDied(ChildProcessConnection connection) {
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildProcessDied(connection);
                        }
                        if (mConnection != null) {
                            assert connection == mConnection;
                            clearConnection();
                        }
                    }
                };

        mConnection = mConnectionAllocator.allocate(context, serviceBundle, serviceCallback);
    }

    /**
     * @return a connection that has been bound or is being bound if one was created with the same
     * allocator as the one provided, null otherwise.
     */
    public ChildProcessConnection getConnection(ChildConnectionAllocator allocator,
            @NonNull final ChildProcessConnection.ServiceCallback serviceCallback) {
        assert LauncherThread.runningOnLauncherThread();
        if (isEmpty() || mConnectionAllocator != allocator || mConnectionServiceCallback != null) {
            return null;
        }

        mConnectionServiceCallback = serviceCallback;

        ChildProcessConnection connection = mConnection;
        if (mConnectionReady) {
            // onChildStarted was already run. Call it explicitly on the passed serviceCallback.
            if (serviceCallback != null) {
                // Post a task so the callback happens after the caller has retrieved the
                // connection.
                LauncherThread.post(new Runnable() {
                    @Override
                    public void run() {
                        serviceCallback.onChildStarted();
                    }
                });
            }
            clearConnection();
        }
        return connection;
    }

    /** Returns true if no connection is available (so getConnection will always return null), */
    public boolean isEmpty() {
        // Note that if the connection was retrieved but was not yet ready mConnection is non null
        // but that connection is already used and will be cleared when it becomes ready. In that
        // case mConnectionServiceCallback is non null.
        return mConnection == null || mConnectionServiceCallback != null;
    }

    private void clearConnection() {
        assert LauncherThread.runningOnLauncherThread();
        mConnection = null;
        mConnectionReady = false;
    }

    @VisibleForTesting
    public ChildProcessConnection getConnection() {
        return mConnection;
    }
}
