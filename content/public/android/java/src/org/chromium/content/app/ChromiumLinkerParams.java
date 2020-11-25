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

    private static final String EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS =
            "org.chromium.content.common.linker_params.base_load_address";

    public ChromiumLinkerParams(long baseLoadAddress) {
        mBaseLoadAddress = baseLoadAddress;
    }

    /**
     * Use this method to recreate a LinkerParams instance from a Bundle.
     *
     * @param bundle A Bundle, its content must have been populated by a previous
     * call to populateBundle().
     * @return params instance or possibly null if params was not put into bundle.
     */
    public static ChromiumLinkerParams create(Bundle bundle) {
        if (!bundle.containsKey(EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS)) return null;
        return new ChromiumLinkerParams(bundle);
    }

    private ChromiumLinkerParams(Bundle bundle) {
        mBaseLoadAddress = bundle.getLong(EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS, 0);
    }

    /**
     * Save data in this LinkerParams instance in a bundle, to be sent to a service process.
     *
     * @param bundle An bundle to be passed to the child service process.
     */
    public void populateBundle(Bundle bundle) {
        bundle.putLong(EXTRA_LINKER_PARAMS_BASE_LOAD_ADDRESS, mBaseLoadAddress);
    }

    // For debugging traces only.
    @Override
    public String toString() {
        return String.format(Locale.US, "LinkerParams(baseLoadAddress:0x%x", mBaseLoadAddress);
    }
}
