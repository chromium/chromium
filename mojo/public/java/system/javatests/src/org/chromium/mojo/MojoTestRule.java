// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo;

import androidx.annotation.IntDef;

import org.junit.rules.ExternalResource;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class to test mojo. Setup the environment.
 */
@JNINamespace("mojo::android")
public class MojoTestRule extends ExternalResource {
    @IntDef({MojoCore.SKIP_INITIALIZATION, MojoCore.INITIALIZE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MojoCore {
        int SKIP_INITIALIZATION = 0;
        int INITIALIZE = 1;
    }

    private static boolean sIsCoreInitialized;
    private final boolean mShouldInitCore;
    private long mTestEnvironmentPointer;

    public MojoTestRule() {
        this(MojoCore.SKIP_INITIALIZATION);
    }

    public MojoTestRule(@MojoCore int shouldInitMojoCore) {
        mShouldInitCore = shouldInitMojoCore == MojoCore.INITIALIZE;
    }

    @Override
    protected void before() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        if (mShouldInitCore && !sIsCoreInitialized) {
            nativeInitCore();
            sIsCoreInitialized = true;
        }
        nativeInit();
        mTestEnvironmentPointer = nativeSetupTestEnvironment();
    }

    @Override
    protected void after() {
        nativeTearDownTestEnvironment(mTestEnvironmentPointer);
    }

    /**
     * Runs the run loop for the given time.
     */
    public void runLoop(long timeoutMS) {
        nativeRunLoop(timeoutMS);
    }

    /**
     * Runs the run loop until no handle or task are immediately available.
     */
    public void runLoopUntilIdle() {
        nativeRunLoop(0);
    }

    private static native void nativeInitCore();

    private native void nativeInit();

    private native long nativeSetupTestEnvironment();

    private native void nativeTearDownTestEnvironment(long testEnvironment);

    private native void nativeRunLoop(long timeoutMS);
}
