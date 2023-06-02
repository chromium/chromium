// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.gms.common.internal;

import android.content.ContentValues;
import android.os.Handler;

/**
 * Stub version of Preconditions class which is substituted in via jar_excluded_patterns when
 * dchecks are off.
 */
public final class Preconditions {
    public static <T> T checkNotNull(T reference) {
        return reference;
    }

    public static String checkNotEmpty(String string) {
        return string;
    }

    public static String checkNotEmpty(String string, Object errorMessage) {
        return string;
    }

    public static <T> T checkNotNull(T reference, Object errorMessage) {
        return reference;
    }

    public static int checkNotZero(int value, Object errorMessage) {
        return value;
    }

    public static int checkNotZero(int value) {
        return value;
    }

    public static long checkNotZero(long value, Object errorMessage) {
        return value;
    }

    public static long checkNotZero(long value) {
        return value;
    }

    public static void checkNotNullIfPresent(String field, ContentValues values) {}

    public static void checkState(boolean expression) {}

    public static void checkState(boolean expression, Object errorMessage) {}

    public static void checkState(
            boolean expression, String errorMessage, Object... errorMessageArgs) {}

    public static void checkArgument(boolean expression, Object errorMessage) {}

    public static void checkArgument(
            boolean expression, String errorMessage, Object... errorMessageArgs) {}

    public static void checkArgument(boolean expression) {}

    private Preconditions() {}

    public static void checkMainThread(String errorMessage) {}

    public static void checkNotMainThread() {}

    public static void checkNotGoogleApiHandlerThread() {}

    public static void checkNotMainThread(String errorMessage) {}

    public static void checkHandlerThread(Handler handler) {}

    public static void checkHandlerThread(Handler handler, String errorMessage) {}

    public static int checkElementIndex(int index, int size) {
        return index;
    }

    public static int checkElementIndex(int index, int size, String desc) {
        return index;
    }

    public static int checkPositionIndex(int index, int size) {
        return index;
    }

    public static int checkPositionIndex(int index, int size, String desc) {
        return index;
    }

    public static void checkPositionIndexes(int start, int end, int size) {}

    public static String checkTag(String tag) {
        return tag;
    }
}
