// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;

/** Collection of test-only WebContents utilities. */
@JNINamespace("content")
public class RenderProcessHostUtils {
    private RenderProcessHostUtils() {}

    public static int getCurrentRenderProcessCount() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return RenderProcessHostUtilsJni.get().getCurrentRenderProcessCount();
                });
    }

    @NativeMethods
    interface Natives {
        int getCurrentRenderProcessCount();
    }
}
