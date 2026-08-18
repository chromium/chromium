// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import android.widget.TextView;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.build.annotations.NullMarked;

/**
 * Custom shadow for {@link TextView} that stubs internal background tasks (like text services
 * locale updates) that would otherwise access attached Views from background threads during unit
 * tests.
 */
@NullMarked
@Implements(value = TextView.class)
public class ShadowTextView extends org.robolectric.shadows.ShadowTextView {
    @Implementation
    protected void updateTextServicesLocaleAsync() {
        // No-op to prevent Android framework background threads from accessing attached TextViews.
    }

    @Implementation
    protected void updateTextServicesLocaleLocked() {
        // No-op to prevent background thread access.
    }
}
