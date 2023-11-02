// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.help;

/**
 * Holder for a singleton {@link HelpAndFeedback} instance for the application.
 */
public class HelpSingleton {
    private static HelpAndFeedback sInstance;

    private HelpSingleton() {}

    /** Returns the instance. Called on the UI thread. */
    public static HelpAndFeedback getInstance() {
        if (sInstance == null) {
            // Create a new instance, so that official builds still work before the internal
            // implementation is complete.
            sInstance = new HelpAndFeedbackBasic();
        }
        return sInstance;
    }

    /** Sets the instance. Called during application startup on the UI thread. */
    public static void setInstance(HelpAndFeedback instance) {
        sInstance = instance;
    }
}
