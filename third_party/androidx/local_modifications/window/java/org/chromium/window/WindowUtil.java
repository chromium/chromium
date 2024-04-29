// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.window;

import android.content.Context;

import androidx.window.extensions.WindowExtensionsProvider;
import androidx.window.extensions.core.util.function.Consumer;
import androidx.window.extensions.layout.WindowLayoutComponent;
import androidx.window.extensions.layout.WindowLayoutInfo;

/**
 * Helpers for using androidx.window.extensions APIs.
 *
 * <p>For documentation, see:
 * https://android.googlesource.com/platform/frameworks/support/+/androidx-main/window/extensions/extensions/src/main/java/androidx/window/extensions/layout/WindowLayoutComponent.java
 */
public class WindowUtil {
    private static final WindowLayoutComponent sWindowLayoutComponent;

    static {
        WindowLayoutComponent value = null;
        try {
            var extensions = WindowExtensionsProvider.getWindowExtensions();
            // Level 2 provides addWindowLayoutInfoListener(Context, Consumer).
            // See comments at https://cs.android.com/search?q=symbol:VENDOR_API_LEVEL_2
            if (extensions.getVendorApiLevel() >= 2) {
                value = extensions.getWindowLayoutComponent();
            }
        } catch (Throwable t) {
        }
        sWindowLayoutComponent = value;
    }

    private WindowUtil() {}

    // Use WindowApiCheck.isAvailable() instead.
    static boolean isAvailable() {
        return sWindowLayoutComponent != null;
    }

    public static void addWindowLayoutInfoListener(
            Context context, Consumer<WindowLayoutInfo> consumer) {
        sWindowLayoutComponent.addWindowLayoutInfoListener(context, consumer);
    }

    public static void removeWindowLayoutInfoListener(Consumer<WindowLayoutInfo> consumer) {
        sWindowLayoutComponent.removeWindowLayoutInfoListener(consumer);
    }
}
