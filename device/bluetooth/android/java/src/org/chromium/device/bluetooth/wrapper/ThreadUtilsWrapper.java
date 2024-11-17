// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import org.chromium.base.ThreadUtils;

/**
 * Wraps base.ThreadUtils.
 * base.ThreadUtils has a set of static method to interact with the
 * UI Thread. To be able to provide a set of test methods, ThreadUtilsWrapper
 * uses the factory pattern.
 */
public class ThreadUtilsWrapper {
    private static Factory sFactory;

    private static ThreadUtilsWrapper sInstance;

    protected ThreadUtilsWrapper() {
    }

    /**
     * Returns the singleton instance of ThreadUtilsWrapper, creating it if needed.
     */
    public static ThreadUtilsWrapper getInstance() {
        if (sInstance == null) {
            if (sFactory == null) {
                sInstance = new ThreadUtilsWrapper();
            } else {
                sInstance = sFactory.create();
            }
        }
        return sInstance;
    }

    public void runOnUiThread(Runnable r) {
        ThreadUtils.runOnUiThread(r);
    }

    /**
     * Instantiate this to explain how to create a ThreadUtilsWrapper instance in
     * ThreadUtilsWrapper.getInstance().
     */
    public interface Factory {
        public ThreadUtilsWrapper create();
    }

    /**
     * Call this to use a different subclass of ThreadUtilsWrapper throughout the
     * program.
     */
    public static void setFactory(Factory factory) {
        sFactory = factory;
        sInstance = null;
    }
}
