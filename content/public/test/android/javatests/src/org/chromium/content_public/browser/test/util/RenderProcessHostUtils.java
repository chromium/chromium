// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ChildBindingState;
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

    public static int getSpareRenderProcessHostCount() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return RenderProcessHostUtilsJni.get().getSpareRenderProcessHostCount();
                });
    }

    public static @ChildBindingState int getSpareRenderBindingState() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return RenderProcessHostUtilsJni.get().getSpareRenderBindingState();
                });
    }

    public static boolean isSpareRenderReady() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return RenderProcessHostUtilsJni.get().isSpareRenderReady();
                });
    }

    @NativeMethods
    interface Natives {
        int getCurrentRenderProcessCount();

        int getSpareRenderProcessHostCount();

        @ChildBindingState
        int getSpareRenderBindingState();

        boolean isSpareRenderReady();
    }
}
