// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.content.browser.framehost.RenderFrameHostImpl;
import org.chromium.content_public.browser.RenderFrameHost;

/** The Java wrapper around RenderFrameHost to define test-only operations. */
@JNINamespace("content")
public class RenderFrameHostTestExt {
    private final long mNativeRenderFrameHostTestExt;

    public RenderFrameHostTestExt(RenderFrameHost host) {
        mNativeRenderFrameHostTestExt =
                RenderFrameHostTestExtJni.get().init(((RenderFrameHostImpl) host).getNativePtr());
    }

    /**
     * Runs the given JavaScript in the RenderFrameHost.
     *
     * @param script A String containing the JavaScript to run.
     * @param callback The Callback that will be called with the result of the JavaScript execution
     *        serialized to a String using JSONStringValueSerializer.
     */
    public void executeJavaScript(String script, Callback<String> callback) {
        RenderFrameHostTestExtJni.get()
                .executeJavaScript(mNativeRenderFrameHostTestExt, script, callback, false);
    }

    public void executeJavaScriptWithUserGesture(String script) {
        RenderFrameHostTestExtJni.get()
                .executeJavaScript(mNativeRenderFrameHostTestExt, script, (String r) -> {}, true);
    }

    public void updateVisualState(Callback<Boolean> callback) {
        RenderFrameHostTestExtJni.get().updateVisualState(mNativeRenderFrameHostTestExt, callback);
    }

    public void notifyVirtualKeyboardOverlayRect(int x, int y, int width, int height) {
        RenderFrameHostTestExtJni.get()
                .notifyVirtualKeyboardOverlayRect(
                        mNativeRenderFrameHostTestExt, x, y, width, height);
    }

    @NativeMethods
    interface Natives {
        long init(long renderFrameHostAndroidPtr);

        void executeJavaScript(
                long nativeRenderFrameHostTestExt,
                String script,
                Callback<String> callback,
                boolean withUserGesture);

        void updateVisualState(long nativeRenderFrameHostTestExt, Callback<Boolean> callback);

        void notifyVirtualKeyboardOverlayRect(
                long nativeRenderFrameHostTestExt, int x, int y, int width, int height);
    }
}
