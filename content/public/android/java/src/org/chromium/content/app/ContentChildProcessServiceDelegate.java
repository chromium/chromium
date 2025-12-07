// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.SparseArray;
import android.window.InputTransferToken;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.IRelroLibInfo;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.memory.MemoryPressureUma;
import org.chromium.base.process_launcher.ChildProcessServiceDelegate;
import org.chromium.base.process_launcher.IChildProcessArgs;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.ChildProcessCreationParamsImpl;
import org.chromium.content.common.IGpuProcessCallback;
import org.chromium.content.common.InputTransferTokenWrapper;
import org.chromium.content.common.SurfaceWrapper;
import org.chromium.content_public.common.ContentProcessInfo;

import java.util.List;

/**
 * This implementation of {@link ChildProcessServiceDelegate} loads the native library, provides
 * access to view surfaces.
 */
@JNINamespace("content")
@NullMarked
public class ContentChildProcessServiceDelegate implements ChildProcessServiceDelegate {
    private static final String TAG = "ContentCPSDelegate";

    private @Nullable IGpuProcessCallback mGpuCallback;

    private int mCpuCount;
    private long mCpuFeatures;

    private @Nullable SparseArray<String> mFdsIdsToKeys;

    public ContentChildProcessServiceDelegate() {
        KillChildUncaughtExceptionHandler.maybeInstallHandler();
    }

    @Override
    public void onServiceCreated() {
        ContentProcessInfo.setInChildProcess(true);
    }

    @Override
    public void onServiceBound(Intent intent) {
        Bundle extras = assumeNonNull(intent.getExtras());
        LibraryLoader.getInstance().getMediator().takeLoadAddressFromBundle(extras);
        LibraryLoader.getInstance()
                .setLibraryProcessType(
                        ChildProcessCreationParamsImpl.getLibraryProcessType(extras));
    }

    @Override
    public void onConnectionSetup(IChildProcessArgs args, List<IBinder> clientInterfaces) {
        mGpuCallback =
                clientInterfaces != null && !clientInterfaces.isEmpty()
                        ? IGpuProcessCallback.Stub.asInterface(clientInterfaces.get(0))
                        : null;

        mCpuCount = args.cpuCount;
        mCpuFeatures = args.cpuFeatures;
        assert mCpuCount > 0;

        LibraryLoader.getInstance().getMediator().takeSharedRelrosFromAidl(args.relroInfo);
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
        ContentChildProcessServiceDelegateJni.get().retrieveFileDescriptorsIdsToKeys(this);
    }

    @Override
    public void consumeRelroLibInfo(IRelroLibInfo libInfo) {
        // Does not block, but may jank slightly. If the library has not been loaded yet, the bundle
        // will be unpacked and saved for the future. If the library is loaded, the RELRO region
        // will be replaced, which involves mmap(2) of shared memory and memcpy+memcmp of a few MB.
        LibraryLoader.getInstance().getMediator().takeSharedRelrosFromAidl(libInfo);
    }

    @Override
    public SparseArray<String> getFileDescriptorsIdsToKeys() {
        assert mFdsIdsToKeys != null;
        return mFdsIdsToKeys;
    }

    @Override
    public void onBeforeMain() {
        ContentChildProcessServiceDelegateJni.get().initChildProcess(this, mCpuCount, mCpuFeatures);
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
    private @Nullable SurfaceWrapper getViewSurface(int surfaceId) {
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

    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @CalledByNative
    private void forwardInputTransferToken(int surfaceId, InputTransferToken vizInputToken) {
        if (mGpuCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }
        try {
            mGpuCallback.forwardInputTransferToken(
                    surfaceId, new InputTransferTokenWrapper(vizInputToken));
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call forwardInputTransferToken: %s", e);
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
                ContentChildProcessServiceDelegate self, int cpuCount, long cpuFeatures);

        /**
         * Initializes the MemoryPressureListener on the same thread callbacks will be received on.
         */
        void initMemoryPressureListener();

        // Retrieves the FD IDs to keys map and set it by calling setFileDescriptorsIdsToKeys().
        void retrieveFileDescriptorsIdsToKeys(ContentChildProcessServiceDelegate self);
    }
}
