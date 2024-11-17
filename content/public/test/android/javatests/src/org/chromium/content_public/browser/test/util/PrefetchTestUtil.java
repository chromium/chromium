// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

@JNINamespace("content")
public class PrefetchTestUtil {
    public static void waitUntilPrefetchResponseCompleted(GURL url) throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefetchTestUtilJni.get()
                            .waitUntilPrefetchResponseCompleted(
                                    url,
                                    () -> {
                                        callbackHelper.notifyCalled();
                                    });
                });
        callbackHelper.waitForOnly();
    }

    @NativeMethods
    interface Natives {
        void waitUntilPrefetchResponseCompleted(@JniType("GURL") GURL url, Runnable callback);
    }
}
