// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.utils;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import androidx.annotation.Nullable;

/**
 * Contains a helper method needed by the AndroidX Fragment library due to bytecode modification we
 * perform at build time.
 */
public class ContextUtils {
    private ContextUtils() {}

    /**
     * Extract the {@link Activity} if the given {@link Context} either is or wraps one.
     *
     * Copied from //base/android/java/src/org/chromium/base/ContextUtils.java
     *
     * @param context The context to check.
     * @return Extracted activity if it exists, otherwise null.
     */
    public static @Nullable Activity activityFromContext(@Nullable Context context) {
        // Only retrieves the base context if the supplied context is a ContextWrapper but not an
        // Activity, because Activity is a subclass of ContextWrapper.
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) return (Activity) context;

            context = ((ContextWrapper) context).getBaseContext();
        }

        return null;
    }
}
