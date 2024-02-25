// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.app.Instrumentation;

/** Collection of UI utilities. */
public class UiUtils {
    /**
     * Waits for the UI thread to settle down and then waits for another second.
     * <p>
     * Avoid this method like the plague. It's a fantastically evil source of flakiness in tests.
     * Instead, you should either:
     *  - Use an observer interface if possible (preferred), or
     *  - Use CriteriaHelper to poll for the desired condition becoming true
     *
     * @param instrumentation Instrumentation object used by the test.
     */
    public static void settleDownUI(Instrumentation instrumentation) throws InterruptedException {
        instrumentation.waitForIdleSync();
        Thread.sleep(1000);
    }
}
