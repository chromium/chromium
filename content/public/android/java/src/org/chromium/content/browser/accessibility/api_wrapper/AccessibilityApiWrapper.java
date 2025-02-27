// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.api_wrapper;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Accessibility API wrapper library allows early prototyping against Android frameworks
 * accessibility APIs. This library is only available in default and canary build.
 *
 * <p>This is a thin wrapper class to allow calling into the internal clank implementation. Methods
 * should exactly match the AccessibilityApiWrapperDelegate.
 */
@NullMarked
public class AccessibilityApiWrapper {

    private static final String TAG = "A11yApiWrapper";

    private static final String WARNING_WRAPPER_NOT_AVAILABLE =
            "accessibility API wrapper library is not available";

    /** An example test API demonstrating the usage of the accessibility API wrapper library. */
    public static @Nullable CharSequence getMyTestStringApi(AccessibilityNodeInfoCompat node) {
        AccessibilityApiWrapperDelegate impl =
                ServiceLoaderUtil.maybeCreate(AccessibilityApiWrapperDelegate.class);
        if (impl != null) {
            return impl.getMyTestStringApi(node);
        }

        Log.w(TAG, "getMyTestStringApi: " + WARNING_WRAPPER_NOT_AVAILABLE);
        return "";
    }

    /** An example test API demonstrating the usage of the accessibility API wrapper library. */
    public static void setMyTestStringApi(
            AccessibilityNodeInfoCompat node, @Nullable CharSequence myTestStringApi) {
        AccessibilityApiWrapperDelegate impl =
                ServiceLoaderUtil.maybeCreate(AccessibilityApiWrapperDelegate.class);
        if (impl != null) {
            impl.setMyTestStringApi(node, myTestStringApi);
        } else {
            Log.w(TAG, "setMyTestStringApi: " + WARNING_WRAPPER_NOT_AVAILABLE);
        }
    }
}
