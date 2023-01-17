// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo;

import androidx.annotation.IntDef;

import org.junit.rules.ExternalResource;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
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
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
    }

    @Override
    protected void before() {
        LibraryLoader.getInstance().ensureInitialized();
        if (mShouldInitCore && !sIsCoreInitialized) {
            MojoTestRuleJni.get().initCore();
            sIsCoreInitialized = true;
        }
        MojoTestRuleJni.get().init();
        mTestEnvironmentPointer = MojoTestRuleJni.get().setupTestEnvironment();
    }

    @Override
    protected void after() {
        MojoTestRuleJni.get().tearDownTestEnvironment(mTestEnvironmentPointer);
    }

    /**
     * Runs the run loop for the given time.
     */
    public void runLoop(long timeoutMS) {
        MojoTestRuleJni.get().runLoop(timeoutMS);
    }

    /**
     * Runs the run loop until no handle or task are immediately available.
     */
    public void runLoopUntilIdle() {
        MojoTestRuleJni.get().runLoop(0);
    }

    @NativeMethods
    interface Natives {
        void init();
        long setupTestEnvironment();
        void tearDownTestEnvironment(long testEnvironment);
        void runLoop(long timeoutMS);
        void initCore();
    }
}
