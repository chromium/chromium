// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import android.content.Context;
import android.view.Display;

import com.google.vr.cardboard.DisplaySynchronizer;
import com.google.vr.ndk.base.GvrApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.ui.display.DisplayAndroidManager;

/**
 * Creates an active GvrContext from a GvrApi created from the Application Context. This GvrContext
 * cannot be used for VR rendering, and should only be used to query pose information and device
 * parameters.
 */
@JNINamespace("device")
public class NonPresentingGvrContext {
    private GvrApi mGvrApi;
    private DisplaySynchronizer mDisplaySynchronizer;
    private boolean mResumed;

    private long mNativeGvrDevice;

    private NonPresentingGvrContext(long nativeGvrDevice) {
        mNativeGvrDevice = nativeGvrDevice;
        Context context = ContextUtils.getApplicationContext();
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mDisplaySynchronizer =
                    new DisplaySynchronizer(context, display) {
                        @Override
                        public void onConfigurationChanged() {
                            super.onConfigurationChanged();
                            onDisplayConfigurationChanged();
                        }
                    };
        }

        // Creating the GvrApi can sometimes create the Daydream config file.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            mGvrApi = new GvrApi(context, mDisplaySynchronizer);
        }
        resume();
    }

    @CalledByNative
    private static NonPresentingGvrContext create(long nativeNonPresentingGvrContext) {
        try {
            return new NonPresentingGvrContext(nativeNonPresentingGvrContext);
        } catch (IllegalStateException | UnsatisfiedLinkError e) {
            return null;
        }
    }

    @CalledByNative
    private long getNativeGvrContext() {
        return mGvrApi.getNativeGvrContext();
    }

    @CalledByNative
    private void pause() {
        if (!mResumed) return;
        mResumed = false;
        mDisplaySynchronizer.onPause();
    }

    @CalledByNative
    private void resume() {
        if (mResumed) return;
        mResumed = true;
        mDisplaySynchronizer.onResume();
    }

    @CalledByNative
    private void shutdown() {
        mDisplaySynchronizer.shutdown();
        mGvrApi.shutdown();
        mNativeGvrDevice = 0;
    }

    public void onDisplayConfigurationChanged() {
        mGvrApi.refreshDisplayMetrics();
    }
}
