// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.api_wrapper;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A delegation interface for accessibility API wrapper library with accessibility API signatures
 * exposed to internal clank to implement. Methods should exactly match the API.
 */
@NullMarked
public interface AccessibilityApiWrapperDelegate {
    /** An example test API demonstrating the usage of the accessibility API wrapper library. */
    public @Nullable CharSequence getMyTestStringApi(AccessibilityNodeInfoCompat node);

    /** An example test API demonstrating the usage of the accessibility API wrapper library. */
    public void setMyTestStringApi(
            AccessibilityNodeInfoCompat node, @Nullable CharSequence myTestStringApi);
}
