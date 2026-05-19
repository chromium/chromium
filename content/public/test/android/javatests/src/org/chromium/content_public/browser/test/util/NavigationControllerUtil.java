// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

@JNINamespace("content")
public class NavigationControllerUtil {
    public static NavigationEntrySimple[] getNavigationHistorySimple(WebContents webContents) {
        return NavigationControllerUtilJni.get().getNavigationHistorySimple(webContents);
    }

    @NativeMethods
    interface Natives {
        NavigationEntrySimple[] getNavigationHistorySimple(WebContents webContents);
    }
}
