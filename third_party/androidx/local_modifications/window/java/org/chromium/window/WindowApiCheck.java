// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.window;

/**
 * Checks if the androidx.window.extensions APIs exist.
 *
 * <p>Must live in a class that does not contain any types used by the API (or else risk
 * NoClassDefFoundErrors).
 */
public class WindowApiCheck {
    private static final boolean sAvailable;

    static {
        boolean value = false;
        try {
            value = WindowUtil.isAvailable();
        } catch (Exception e) {
            // E.g. NoClassDefFoundError
        }
        sAvailable = value;
    }

    /** Returns whether the required system library is loaded. */
    public static boolean isAvailable() {
        return sAvailable;
    }
}
