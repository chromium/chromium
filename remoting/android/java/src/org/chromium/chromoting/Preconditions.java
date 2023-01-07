// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * A set of helper functions to write assertions. These functions are always being executed, and
 * will not ignored in release build.
 */

public final class Preconditions {
    // This class contains only static functions, so it should not be instantiated.
    private Preconditions() {}

    /**
     * Checks whether input |value| is true, and returns its value. Throws
     * {@link IllegalStateException} if |value| is false.
     */
    public static final boolean isTrue(boolean value) {
        if (!value) {
            throw new IllegalStateException();
        }
        return value;
    }

    /**
     * Checks whether input |ref| is not a null reference, and returns its value. Throws
     * {@link NullPointerException} if |ref| is null.
     */
    public static final <T> T notNull(T ref) {
        if (ref == null) {
            throw new NullPointerException();
        }
        return ref;
    }

    /**
     * Checks whether input |ref| is a null reference, and returns its value. Throws
     * {@link IllegalArgumentException} if |ref| is not null.
     */
    public static final <T> T isNull(T ref) {
        if (ref != null) {
            throw new IllegalArgumentException();
        }
        return ref;
    }
}
