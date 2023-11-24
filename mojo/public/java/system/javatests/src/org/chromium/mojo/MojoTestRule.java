// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.junit.rules.ExternalResource;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Base class to test mojo. Setup the environment. */
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
     * Quits the run loop. Unlike the C++ equivalent, may not be called if the run loop is not
     * running.
     */
    public void quitLoop() {
        MojoTestRuleJni.get().quitLoop(mTestEnvironmentPointer);
    }

    /**
     * Runs the run loop for the given time.
     *
     * @param timeoutMS How long to run the run loop for before timing out, in milliseconds. A
     *     negative value will run forever; a value of 0 will run until idle.
     */
    public void runLoop(long timeoutMS) {
        MojoTestRuleJni.get().runLoop(mTestEnvironmentPointer, timeoutMS);
    }

    /** Runs the run loop forever. Should be used in conjunction with quitRunLoop(). */
    public void runLoopForever() {
        runLoop(-1);
    }

    /** Runs the run loop until no handle or task are immediately available. */
    public void runLoopUntilIdle() {
        runLoop(0);
    }

    @NativeMethods
    interface Natives {
        void init();

        long setupTestEnvironment();

        void tearDownTestEnvironment(long testEnvironment);

        void quitLoop(long testEnvironment);

        void runLoop(long testEnvironment, long timeoutMS);

        void initCore();
    }
}
