// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A singleton class to record metrics about how Chrome is launched, either as full browser or
 * minimal browser.
 */
public class ServicificationStartupUma {
    // This enum is used to back UMA histograms, and should therefore be treated as append-only.
    @IntDef({
        ServicificationStartup.CHROME_COLD,
        ServicificationStartup.CHROME_HALF_WARM,
        ServicificationStartup.MINIMAL_BROWSER_COLD,
        ServicificationStartup.MINIMAL_BROWSER_WARM
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ServicificationStartup {
        // Cold start of Chrome as a full browser.
        int CHROME_COLD = 0;
        // Half warm start of Chrome.
        int CHROME_HALF_WARM = 1;
        // Cold start of only the minimal browser.
        int MINIMAL_BROWSER_COLD = 2;
        // Warm start of only the minimal browser when already running.
        int MINIMAL_BROWSER_WARM = 3;

        int NUM_ENTRIES = 4;
    }

    // Caches the pending commits before the native is initialized.
    private int[] mPendingCommits = new int[ServicificationStartup.NUM_ENTRIES];
    private boolean mIsNativeInitialized;

    private static final ServicificationStartupUma sInstance = new ServicificationStartupUma();

    /** Returns the singleton instance. */
    public static ServicificationStartupUma getInstance() {
        return sInstance;
    }

    /** Returns the startup mode. */
    public static int getStartupMode(
            boolean isFullBrowserStarted,
            boolean isMinimalBrowserStarted,
            boolean startMinimalBrowser) {
        if (isFullBrowserStarted) {
            return -1;
        }

        if (isMinimalBrowserStarted) {
            if (startMinimalBrowser) {
                return ServicificationStartup.MINIMAL_BROWSER_WARM;
            }
            return ServicificationStartup.CHROME_HALF_WARM;
        }

        if (startMinimalBrowser) {
            return ServicificationStartup.MINIMAL_BROWSER_COLD;
        }
        return ServicificationStartup.CHROME_COLD;
    }

    /**
     * Records the metrics of the given startup mode. If native hasn't initialized, caches the
     * request to pending commit array. Note: warm startup of Chrome doesn't record in this metrics,
     * since it doesn't go through the native initialization and has been recorded in other startup
     * metrics.
     */
    public void record(@ServicificationStartup int startupMode) {
        if (startupMode < 0) return;

        if (mIsNativeInitialized) {
            recordStartupMode(startupMode);
            return;
        }
        mPendingCommits[startupMode]++;
    }

    /**
     * Called when the native initialization is complete. Commit any pending request in the pending
     * commit array.
     */
    public void commit() {
        mIsNativeInitialized = true;

        for (int i = 0; i < ServicificationStartup.NUM_ENTRIES; i++) {
            if (mPendingCommits[i] > 0) {
                for (int count = 0; count < mPendingCommits[i]; count++) {
                    recordStartupMode(i);
                }
                mPendingCommits[i] = 0;
            }
        }
    }

    private ServicificationStartupUma() {
        mIsNativeInitialized = false;
    }

    private void recordStartupMode(@ServicificationStartup int startupMode) {
        RecordHistogram.recordEnumeratedHistogram(
                "Servicification.Startup2", startupMode, ServicificationStartup.NUM_ENTRIES);
    }
}
