// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.os.Bundle;

import java.util.Locale;

import javax.annotation.concurrent.Immutable;

/**
 * A class to hold information passed from the browser process to each
 * service one when using the chromium linker. For more information, read the
 * technical notes in Linker.java.
 */
@Immutable
public class ChromiumLinkerParams {
    // Use this base address to load native shared libraries. If 0, ignore other members.
    public final long mBaseLoadAddress;

    // If true, wait for a shared RELRO Bundle just after loading the libraries.
    public final boolean mWaitForSharedRelro;

    // If not empty, name of Linker.TestRunner implementation that needs to be
    // registered in the service process.
    public final String mTestRunnerClassNameForTesting;

    // If mTestRunnerClassNameForTesting is not empty, the Linker implementation
    // to force for testing.
    public final int mLinkerImplementationForTesting;

    private static final String EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS =
            "org.chromium.content.common.linker_params.base_load_address";

    private static final String EXTRA_LINKER_PARAMS_WAIT_FOR_SHARED_RELRO =
            "org.chromium.content.common.linker_params.wait_for_shared_relro";

    private static final String EXTRA_LINKER_PARAMS_TEST_RUNNER_CLASS_NAME =
            "org.chromium.content.common.linker_params.test_runner_class_name";

    private static final String EXTRA_LINKER_PARAMS_LINKER_IMPLEMENTATION =
            "org.chromium.content.common.linker_params.linker_implementation";

    public ChromiumLinkerParams(long baseLoadAddress, boolean waitForSharedRelro) {
        mBaseLoadAddress = baseLoadAddress;
        mWaitForSharedRelro = waitForSharedRelro;
        mTestRunnerClassNameForTesting = null;
        mLinkerImplementationForTesting = 0;
    }

    /**
     * Use this constructor to create a LinkerParams instance for testing.
     */
    public ChromiumLinkerParams(long baseLoadAddress, boolean waitForSharedRelro,
            String testRunnerClassName, int linkerImplementation) {
        mBaseLoadAddress = baseLoadAddress;
        mWaitForSharedRelro = waitForSharedRelro;
        mTestRunnerClassNameForTesting = testRunnerClassName;
        mLinkerImplementationForTesting = linkerImplementation;
    }

    /**
     * Use this method to recreate a LinkerParams instance from a Bundle.
     *
     * @param bundle A Bundle, its content must have been populated by a previous
     * call to populateBundle().
     * @return params instance or possibly null if params was not put into bundle.
     */
    public static ChromiumLinkerParams create(Bundle bundle) {
        if (!bundle.containsKey(EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS)
                || !bundle.containsKey(EXTRA_LINKER_PARAMS_WAIT_FOR_SHARED_RELRO)
                || !bundle.containsKey(EXTRA_LINKER_PARAMS_TEST_RUNNER_CLASS_NAME)
                || !bundle.containsKey(EXTRA_LINKER_PARAMS_LINKER_IMPLEMENTATION)) {
            return null;
        }
        return new ChromiumLinkerParams(bundle);
    }

    private ChromiumLinkerParams(Bundle bundle) {
        mBaseLoadAddress = bundle.getLong(EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS, 0);
        mWaitForSharedRelro = bundle.getBoolean(EXTRA_LINKER_PARAMS_WAIT_FOR_SHARED_RELRO, false);
        mTestRunnerClassNameForTesting =
                bundle.getString(EXTRA_LINKER_PARAMS_TEST_RUNNER_CLASS_NAME);
        mLinkerImplementationForTesting =
                bundle.getInt(EXTRA_LINKER_PARAMS_LINKER_IMPLEMENTATION, 0);
    }

    /**
     * Save data in this LinkerParams instance in a bundle, to be sent to a service process.
     *
     * @param bundle An bundle to be passed to the child service process.
     */
    public void populateBundle(Bundle bundle) {
        bundle.putLong(EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS, mBaseLoadAddress);
        bundle.putBoolean(EXTRA_LINKER_PARAMS_WAIT_FOR_SHARED_RELRO, mWaitForSharedRelro);
        bundle.putString(
                EXTRA_LINKER_PARAMS_TEST_RUNNER_CLASS_NAME, mTestRunnerClassNameForTesting);
        bundle.putInt(EXTRA_LINKER_PARAMS_LINKER_IMPLEMENTATION, mLinkerImplementationForTesting);
    }

    // For debugging traces only.
    @Override
    public String toString() {
        return String.format(Locale.US,
                "LinkerParams(baseLoadAddress:0x%x, waitForSharedRelro:%s, "
                        + "testRunnerClassName:%s, linkerImplementation:%d",
                mBaseLoadAddress, Boolean.toString(mWaitForSharedRelro),
                mTestRunnerClassNameForTesting, mLinkerImplementationForTesting);
    }
}
