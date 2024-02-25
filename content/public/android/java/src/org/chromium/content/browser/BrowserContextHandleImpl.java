// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.content_public.browser.BrowserContextHandle;

/** An interface that provides access to a native BrowserContext. */
@JNINamespace("content")
public class BrowserContextHandleImpl {
    private BrowserContextHandleImpl() {}

    /** @return A pointer to the native BrowserContext that this object wraps. */
    @CalledByNative
    private static long getNativeBrowserContextPointer(BrowserContextHandle handle) {
        return handle.getNativeBrowserContextPointer();
    }
}
