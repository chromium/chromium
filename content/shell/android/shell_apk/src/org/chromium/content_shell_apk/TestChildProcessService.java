// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.util.SparseArray;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.base.process_launcher.ChildProcessServiceDelegate;

import java.util.List;

import javax.annotation.concurrent.GuardedBy;

/** Child service started by ChildProcessLauncherTest. */
public class TestChildProcessService extends Service {
    private static final String TAG = "TestProcessService";

    private static final long MAIN_BLOCKING_DURATION_MS = 5000;

    private static class TestChildProcessServiceDelegate implements ChildProcessServiceDelegate {
        private final Object mConnectionSetupLock = new Object();

        @GuardedBy("mConnectionSetupLock")
        private boolean mConnectionSetup;

        private boolean mServiceCreated;
        private Bundle mServiceBundle;
        private String[] mCommandLine;
        private IChildProcessTest mIChildProcessTest;

        @Override
        public void onServiceCreated() {
            mServiceCreated = true;
        }

        @Override
        public void onServiceBound(Intent intent) {
            mServiceBundle = intent.getExtras();
            LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_CHILD);
        }

        @Override
        public void onConnectionSetup(
                Bundle connectionBundle, List<IBinder> clientInterfaces, IBinder binderBox) {
            if (clientInterfaces != null && !clientInterfaces.isEmpty()) {
                mIChildProcessTest = IChildProcessTest.Stub.asInterface(clientInterfaces.get(0));
            }
            if (mIChildProcessTest != null) {
                try {
                    mIChildProcessTest.onConnectionSetup(
                            mServiceCreated, mServiceBundle, connectionBundle);
                } catch (RemoteException re) {
                    Log.e(TAG, "Failed to call IChildProcessTest.onConnectionSetup.", re);
                }
            }
            synchronized (mConnectionSetupLock) {
                mConnectionSetup = true;
                mConnectionSetupLock.notifyAll();
            }
        }

        @Override
        public void preloadNativeLibrary(String packageName) {
            LibraryLoader.getInstance().preloadNow();
        }

        @Override
        public void loadNativeLibrary(Context hostContext) {
            // Store the command line before loading the library to avoid an assert in CommandLine.
            mCommandLine = CommandLine.getJavaSwitchesForTesting();

            LibraryLoader.getInstance().loadNow();
            LibraryLoader.getInstance().ensureInitialized();

            // Loading the library happen on the main thread and onConnectionSetup is called from
            // the client. Wait for onConnectionSetup so mIChildProcessTest is set.
            synchronized (mConnectionSetupLock) {
                while (!mConnectionSetup) {
                    try {
                        mConnectionSetupLock.wait();
                    } catch (InterruptedException e) {
                        // Ignore.
                    }
                }
            }

            if (mIChildProcessTest != null) {
                try {
                    mIChildProcessTest.onLoadNativeLibrary(true);
                } catch (RemoteException re) {
                    Log.e(TAG, "Failed to call IChildProcessTest.onLoadNativeLibrary.", re);
                }
            }
        }

        @Override
        public SparseArray<String> getFileDescriptorsIdsToKeys() {
            return null;
        }

        @Override
        public void onBeforeMain() {
            if (mIChildProcessTest == null) return;
            try {
                mIChildProcessTest.onBeforeMain(mCommandLine);
            } catch (RemoteException re) {
                Log.e(TAG, "Failed to call IChildProcessTest.onBeforeMain.", re);
            }
        }

        @Override
        public void runMain() {
            if (mIChildProcessTest != null) {
                try {
                    mIChildProcessTest.onRunMain();
                } catch (RemoteException re) {
                    Log.e(TAG, "Failed to call IChildProcessTest.onRunMain.", re);
                }
            }
            // Run a message loop to keep the service from exiting.
            Looper.prepare();
            Looper.loop();
        }

        @Override
        public void consumeRelroBundle(Bundle bundle) {}
    }

    private ChildProcessService mService;

    public TestChildProcessService() {}

    @Override
    public void onCreate() {
        super.onCreate();
        mService =
                new ChildProcessService(
                        new TestChildProcessServiceDelegate(), this, getApplicationContext());
        mService.onCreate();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mService.onDestroy();
        mService = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mService.onBind(intent);
    }
}
