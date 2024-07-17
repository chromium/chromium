// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.RenderFrameHostTestExt;
import org.chromium.url.GURL;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** This class implements FencedFrameUtils */
@JNINamespace("content")
public class FencedFrameUtils {
    private FencedFrameUtils() {
        // no-op constructor; won't be called.
    }

    private static int getCount(final RenderFrameHost frame) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return FencedFrameUtilsJni.get().getCount(frame);
                });
    }

    public static RenderFrameHost getLastFencedFrame(
            final RenderFrameHost frame, final String url) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return FencedFrameUtilsJni.get().getLastFencedFrame(frame, url);
                });
    }

    public static RenderFrameHost createFencedFrame(
            final WebContents webContents, final RenderFrameHost parentFrame, String url)
            throws TimeoutException {
        RenderFrameHostTestExt frameExt =
                ThreadUtils.runOnUiThreadBlocking(() -> new RenderFrameHostTestExt(parentFrame));

        int previousFencedFrameCount = getCount(parentFrame);
        final CountDownLatch latch = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new WebContentsObserver(webContents) {
                        @Override
                        public void didStopLoading(GURL url, boolean isKnownValid) {
                            latch.countDown();
                            webContents.removeObserver(this);
                        }
                    };

                    String script =
                            "((async() => {"
                                    + " const fenced_frame = document.createElement('fencedframe');"
                                    + " fenced_frame.config = new FencedFrameConfig('"
                                    + url
                                    + "');"
                                    + " document.body.appendChild(fenced_frame);"
                                    + "})());";
                    frameExt.executeJavaScript(script, (String r) -> {});
                });

        try {
            Assert.assertTrue(latch.await(CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }

        Assert.assertEquals(previousFencedFrameCount + 1, getCount(parentFrame));
        RenderFrameHost fencedFrame = getLastFencedFrame(parentFrame, url);
        Assert.assertNotNull(fencedFrame);
        return fencedFrame;
    }

    @NativeMethods
    interface Natives {
        int getCount(RenderFrameHost frame);

        RenderFrameHost getLastFencedFrame(RenderFrameHost frame, String url);
    }
}
