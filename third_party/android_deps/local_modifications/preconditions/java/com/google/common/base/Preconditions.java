// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.common.base;

/**
 * Stub version of Preconditions class which is substituted in via jar_excluded_patterns when
 * dchecks are off.
 */
public final class Preconditions {
    private Preconditions() {}

    public static void checkArgument(boolean expression) {}

    public static void checkArgument(boolean expression, Object errorMessage) {}

    public static void checkArgument(
            boolean expression, String errorMessageTemplate, Object... errorMessageArgs) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, char p1) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, int p1) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, long p1) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, Object p1) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, char p1, char p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, char p1, int p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, char p1, long p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, char p1, Object p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, int p1, char p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, int p1, int p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, int p1, long p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, int p1, Object p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, long p1, char p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, long p1, int p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, long p1, long p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, long p1, Object p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, Object p1, char p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, Object p1, int p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, Object p1, long p2) {}

    public static void checkArgument(boolean b, String errorMessageTemplate, Object p1, Object p2) {
    }

    public static void checkArgument(
            boolean b, String errorMessageTemplate, Object p1, Object p2, Object p3) {}

    public static void checkArgument(
            boolean b, String errorMessageTemplate, Object p1, Object p2, Object p3, Object p4) {}

    public static void checkState(boolean expression) {}

    public static void checkState(boolean expression, Object errorMessage) {}

    public static void checkState(
            boolean expression, String errorMessageTemplate, Object... errorMessageArgs) {}

    public static void checkState(boolean b, String errorMessageTemplate, char p1) {}

    public static void checkState(boolean b, String errorMessageTemplate, int p1) {}

    public static void checkState(boolean b, String errorMessageTemplate, long p1) {}

    public static void checkState(boolean b, String errorMessageTemplate, Object p1) {}

    public static void checkState(boolean b, String errorMessageTemplate, char p1, char p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, char p1, int p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, char p1, long p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, char p1, Object p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, int p1, char p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, int p1, int p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, int p1, long p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, int p1, Object p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, long p1, char p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, long p1, int p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, long p1, long p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, long p1, Object p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, Object p1, char p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, Object p1, int p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, Object p1, long p2) {}

    public static void checkState(boolean b, String errorMessageTemplate, Object p1, Object p2) {}

    public static void checkState(
            boolean b, String errorMessageTemplate, Object p1, Object p2, Object p3) {}

    public static void checkState(
            boolean b, String errorMessageTemplate, Object p1, Object p2, Object p3, Object p4) {}

    public static <T extends Object> T checkNotNull(T reference) {
        return reference;
    }

    public static <T extends Object> T checkNotNull(T reference, Object errorMessage) {
        return reference;
    }

    public static <T extends Object> T checkNotNull(
            T reference, String errorMessageTemplate, Object... errorMessageArgs) {
        return reference;
    }

    public static <T extends Object> T checkNotNull(T obj, String errorMessageTemplate, char p1) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(T obj, String errorMessageTemplate, int p1) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(T obj, String errorMessageTemplate, long p1) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(T obj, String errorMessageTemplate, Object p1) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, char p1, char p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, char p1, int p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, char p1, long p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, char p1, Object p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, int p1, char p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, int p1, int p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, int p1, long p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, int p1, Object p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, long p1, char p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, long p1, int p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, long p1, long p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, long p1, Object p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, Object p1, char p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, Object p1, int p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, Object p1, long p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, Object p1, Object p2) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, Object p1, Object p2, Object p3) {
        return obj;
    }

    public static <T extends Object> T checkNotNull(
            T obj, String errorMessageTemplate, Object p1, Object p2, Object p3, Object p4) {
        return obj;
    }

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
}
