// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content.browser.navigation_transitions.BackForwardTransitionAnimationManagerImplJni;

/** Test-only configuration for the back/forward transition animation manager. */
@NullMarked
public final class BackForwardTransitionAnimationManagerTestUtils {
    private BackForwardTransitionAnimationManagerTestUtils() {}

    /** Sets the minimum physical RAM for transitions, restoring it after the test. */
    public static void setMinRequiredPhysicalRamMbForTesting(int mb) {
        Runnable reset =
                BackForwardTransitionAnimationManagerImplJni.get()
                        .setMinRequiredPhysicalRamMbForTesting(mb);
        ResettersForTesting.register(reset);
    }
}
