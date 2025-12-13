// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ChildBindingState;
import org.chromium.base.Log;
import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This class is used to create a single spare ChildProcessConnection (usually early on during
 * start-up) that can then later be retrieved when a connection to a service is needed.
 */
@NullMarked
public class SpareChildConnection {
    private static final String TAG = "SpareChildConn";

    // The allocator used to create the connection.
    private final ChildConnectionAllocator mConnectionAllocator;

    // The actual spare connection.
    private @Nullable ChildProcessConnection mConnection;

    // True when there is a spare connection and it is bound.
    private boolean mConnectionReady;

    // The callback that should be called when the connection becomes bound. Set when the connection
    // is retrieved.
    private ChildProcessConnection.@Nullable ServiceCallback mConnectionServiceCallback;

    // The requested initial binding state passed in getConnection.
    // If the connection is not yet bound when getConnection is called, we need to update
    // the binding state in the onChildStarted callback.
    @ChildBindingState @Nullable Integer mRequestedBindingState;

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
                        if (mRequestedBindingState != null) {
                            updateInitialBindingState(mRequestedBindingState);
                        }
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
                        Log.w(TAG, "Failed to warm up the spare sandbox service");
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

        mConnection =
                mConnectionAllocator.allocate(
                        context, serviceBundle, serviceCallback, ChildBindingState.VISIBLE);
    }

    /**
     * @return a connection that has been bound or is being bound if one was created with the same
     *     allocator as the one provided, null otherwise.
     */
    public @Nullable ChildProcessConnection getConnection(
            ChildConnectionAllocator allocator,
            final ChildProcessConnection.ServiceCallback serviceCallback,
            @ChildBindingState int requestedBindingState) {
        assert LauncherThread.runningOnLauncherThread();
        if (isEmpty() || mConnectionAllocator != allocator || mConnectionServiceCallback != null) {
            return null;
        }

        mConnectionServiceCallback = serviceCallback;

        assert mConnection != null;
        ChildProcessConnection connection = mConnection;
        if (mConnectionReady) {
            updateInitialBindingState(requestedBindingState);
            // onChildStarted was already run. Call it explicitly on the passed serviceCallback.
            if (serviceCallback != null) {
                // Post a task so the callback happens after the caller has retrieved the
                // connection.
                LauncherThread.post(
                        new Runnable() {
                            @Override
                            public void run() {
                                serviceCallback.onChildStarted();
                            }
                        });
            }
            clearConnection();
        } else {
            mRequestedBindingState = requestedBindingState;
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
    public @Nullable ChildProcessConnection getConnection() {
        return mConnection;
    }

    private void updateInitialBindingState(@ChildBindingState int requestedBindingState) {
        ChildProcessConnection connection = mConnection;
        assert connection != null;
        // The spare connection is created with a visible binding. Adjust if needed.
        if (requestedBindingState != ChildBindingState.VISIBLE) {
            if (requestedBindingState == ChildBindingState.STRONG) {
                connection.addStrongBinding();
            } else if (requestedBindingState == ChildBindingState.NOT_PERCEPTIBLE) {
                connection.addNotPerceptibleBinding();
            }
            // For STRONG, NOT_PERCEPTIBLE, and WAIVED, we remove the original visible binding.
            connection.removeVisibleBinding();
        }
    }
}
