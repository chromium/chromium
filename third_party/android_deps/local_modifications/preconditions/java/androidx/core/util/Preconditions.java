// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.core.util;

/**
 * Stub version of Preconditions class which is substituted in via jar_excluded_patterns when
 * dchecks are off.
 */
public final class Preconditions {
    public static void checkArgument(boolean expression) {}

    public static void checkArgument(boolean expression, Object errorMessage) {}

    public static <T extends CharSequence> T checkStringNotEmpty(final T string) {
        return string;
    }

    public static <T extends CharSequence> T checkStringNotEmpty(
            final T string, final Object errorMessage) {
        return string;
    }

    public static <T extends CharSequence> T checkStringNotEmpty(
            final T string, final String messageTemplate, final Object... messageArgs) {
        return string;
    }

    public static <T> T checkNotNull(T reference) {
        return reference;
    }

    public static <T> T checkNotNull(T reference, Object errorMessage) {
        return reference;
    }

    public static void checkState(boolean expression, String message) {}

    public static void checkState(final boolean expression) {}

    public static int checkFlagsArgument(final int requestedFlags, final int allowedFlags) {
        return requestedFlags;
    }

    public static int checkArgumentNonnegative(final int value, String errorMessage) {
        return value;
    }

    public static int checkArgumentNonnegative(final int value) {
        return value;
    }

    public static int checkArgumentInRange(int value, int lower, int upper, String valueName) {
        return value;
    }

    private Preconditions() {}
}
