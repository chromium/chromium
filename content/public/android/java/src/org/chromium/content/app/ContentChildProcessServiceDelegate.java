// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.SparseArray;
import android.view.Surface;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnguessableToken;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.memory.MemoryPressureUma;
import org.chromium.base.process_launcher.ChildProcessServiceDelegate;
import org.chromium.content.browser.ChildProcessCreationParamsImpl;
import org.chromium.content.browser.ContentChildProcessConstants;
import org.chromium.content.common.IGpuProcessCallback;
import org.chromium.content.common.SurfaceWrapper;
import org.chromium.content_public.common.ContentProcessInfo;

import java.util.List;

/**
 * This implementation of {@link ChildProcessServiceDelegate} loads the native library, provides
 * access to view surfaces.
 */
@JNINamespace("content")
public class ContentChildProcessServiceDelegate implements ChildProcessServiceDelegate {
    private static final String TAG = "ContentCPSDelegate";

    // The binder box passed to us by the browser. May be null.
    private IBinder mBinderBox;

    private IGpuProcessCallback mGpuCallback;

    private int mCpuCount;
    private long mCpuFeatures;

    private SparseArray<String> mFdsIdsToKeys;

    public ContentChildProcessServiceDelegate() {
        KillChildUncaughtExceptionHandler.maybeInstallHandler();
    }

    @Override
    public void onServiceCreated() {
        ContentProcessInfo.setInChildProcess(true);
    }

    @Override
    public void onServiceBound(Intent intent) {
        LibraryLoader.getInstance().getMediator().takeLoadAddressFromBundle(intent.getExtras());
        LibraryLoader.getInstance()
                .setLibraryProcessType(
                        ChildProcessCreationParamsImpl.getLibraryProcessType(intent.getExtras()));
    }

    @Override
    public void onConnectionSetup(
            Bundle connectionBundle, List<IBinder> clientInterfaces, IBinder binderBox) {
        mBinderBox = binderBox;
        mGpuCallback =
                clientInterfaces != null && !clientInterfaces.isEmpty()
                        ? IGpuProcessCallback.Stub.asInterface(clientInterfaces.get(0))
                        : null;

        mCpuCount = connectionBundle.getInt(ContentChildProcessConstants.EXTRA_CPU_COUNT);
        mCpuFeatures = connectionBundle.getLong(ContentChildProcessConstants.EXTRA_CPU_FEATURES);
        assert mCpuCount > 0;

        LibraryLoader.getInstance().getMediator().takeSharedRelrosFromBundle(connectionBundle);
    }

    @Override
    public void preloadNativeLibrary(String packageName) {
        // This function can be called before command line is set. That is fine because
        // preloading explicitly doesn't run any Chromium code, see NativeLibraryPreloader
        // for more info.
        LibraryLoader.getInstance().preloadNowOverridePackageName(packageName);
    }

    @Override
    public void loadNativeLibrary(Context hostContext) {
        if (LibraryLoader.getInstance().isLoadedByZygote()) {
            initializeLibrary();
            return;
        }

        LibraryLoader libraryLoader = LibraryLoader.getInstance();
        libraryLoader.getMediator().initInChildProcess();
        libraryLoader.loadNowOverrideApplicationContext(hostContext);
        initializeLibrary();
    }

    private void initializeLibrary() {
        LibraryLoader.getInstance().initialize();

        // Now that the library is loaded, get the FD map,
        // TODO(jcivelli): can this be done in onBeforeMain? We would have to mode onBeforeMain
        // so it's called before FDs are registered.
        ContentChildProcessServiceDelegateJni.get()
                .retrieveFileDescriptorsIdsToKeys(ContentChildProcessServiceDelegate.this);
    }

    @Override
    public void consumeRelroBundle(Bundle bundle) {
        // Does not block, but may jank slightly. If the library has not been loaded yet, the bundle
        // will be unpacked and saved for the future. If the library is loaded, the RELRO region
        // will be replaced, which involves mmap(2) of shared memory and memcpy+memcmp of a few MB.
        LibraryLoader.getInstance().getMediator().takeSharedRelrosFromBundle(bundle);
    }

    @Override
    public SparseArray<String> getFileDescriptorsIdsToKeys() {
        assert mFdsIdsToKeys != null;
        return mFdsIdsToKeys;
    }

    @Override
    public void onBeforeMain() {
        ContentChildProcessServiceDelegateJni.get()
                .initChildProcess(ContentChildProcessServiceDelegate.this, mCpuCount, mCpuFeatures);
        ThreadUtils.getUiThreadHandler()
                .post(
                        () -> {
                            ContentChildProcessServiceDelegateJni.get()
                                    .initMemoryPressureListener();
                            MemoryPressureUma.initializeForChildService();
                        });
    }

    @Override
    public void runMain() {
        ContentMain.setBindersFromParent(mBinderBox);
        ContentMain.start(false);
    }

    @CalledByNative
    private void setFileDescriptorsIdsToKeys(int[] ids, String[] keys) {
        assert ids.length == keys.length;
        assert mFdsIdsToKeys == null;
        mFdsIdsToKeys = new SparseArray<>();
        for (int i = 0; i < ids.length; ++i) {
            mFdsIdsToKeys.put(ids[i], keys[i]);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void forwardSurfaceForSurfaceRequest(
            @JniType("base::UnguessableToken") UnguessableToken requestToken, Surface surface) {
        if (mGpuCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        try {
            mGpuCallback.forwardSurfaceForSurfaceRequest(requestToken, surface);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call forwardSurfaceForSurfaceRequest: %s", e);
            return;
        } finally {
            surface.release();
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private SurfaceWrapper getViewSurface(int surfaceId) {
        if (mGpuCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return null;
        }

        try {
            SurfaceWrapper wrapper = mGpuCallback.getViewSurface(surfaceId);
            return wrapper;
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call getViewSurface: %s", e);
            return null;
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Initializes the native parts of the service.
         *
         * @param cpuCount The number of CPUs.
         * @param cpuFeatures The CPU features.
         */
        void initChildProcess(
                ContentChildProcessServiceDelegate caller, int cpuCount, long cpuFeatures);

        /**
         * Initializes the MemoryPressureListener on the same thread callbacks will be
         * received on.
         */
        void initMemoryPressureListener();

        // Retrieves the FD IDs to keys map and set it by calling setFileDescriptorsIdsToKeys().
        void retrieveFileDescriptorsIdsToKeys(ContentChildProcessServiceDelegate caller);
    }
}
