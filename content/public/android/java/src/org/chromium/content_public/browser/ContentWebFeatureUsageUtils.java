// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.blink.mojom.WebFeature;

/** A Java API for calling ContentBrowserClient::LogWebFeatureForCurrentPage(). */
@JNINamespace("content")
public class ContentWebFeatureUsageUtils {
    public static void logWebFeatureForCurrentPage(
            WebContents webContents, @WebFeature.EnumType int webFeature) {
        ContentWebFeatureUsageUtilsJni.get().logWebFeatureForCurrentPage(webContents, webFeature);
    }

    // If there's a need for a logWebDXFeatureForCurrentPage() in Java, there's
    // code for that included in
    // https://chromium-review.googlesource.com/c/chromium/src/+/5640502/11.

    @NativeMethods
    public interface Natives {
        void logWebFeatureForCurrentPage(
                WebContents webContents, @WebFeature.EnumType int webFeature);
    }
}
