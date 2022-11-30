// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.annotations.JNINamespace;

/**
 * Collection of test-only WebContents utilities.
 */
@JNINamespace("content")
public class RenderProcessHostUtils {
    private RenderProcessHostUtils() {}

    public static int getCurrentRenderProcessCount() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return nativeGetCurrentRenderProcessCount(); });
    }

    private static native int nativeGetCurrentRenderProcessCount();
}
