// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.ExecutionException;

/**
 * Collection of test-only WebContents utilities.
 */
@JNINamespace("content")
public class WebContentsUtils {
    /**
     * Reports all frame submissions to the browser process, even those that do not impact Browser
     * UI.
     * @param webContents The WebContents for which to report all frame submissions.
     * @param enabled Whether to report all frame submissions.
     */
    public static void reportAllFrameSubmissions(final WebContents webContents, boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { nativeReportAllFrameSubmissions(webContents, enabled); });
    }

    /**
     * @param webContents The WebContents on which the SelectPopup is being shown.
     * @return {@code true} if select popup is being shown.
     */
    public static boolean isSelectPopupVisible(WebContents webContents) {
        return SelectPopup.fromWebContents(webContents).isVisibleForTesting();
    }

    /**
     * Gets the currently focused {@link RenderFrameHost} instance for a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static RenderFrameHost getFocusedFrame(final WebContents webContents) {
        return nativeGetFocusedFrame(webContents);
    }

    /**
     * Issues a fake notification about the renderer being killed.
     *
     * @param webContents The WebContents in use.
     * @param wasOomProtected True if the renderer was protected from the OS out-of-memory killer
     *                        (e.g. renderer for the currently selected tab)
     */
    public static void simulateRendererKilled(WebContents webContents, boolean wasOomProtected) {
        TestThreadUtils.runOnUiThreadBlocking(() ->
            ((WebContentsImpl) webContents).simulateRendererKilledForTesting(wasOomProtected));
    }

    /**
     * Returns {@link ImeAdapter} instance associated with a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static ImeAdapter getImeAdapter(WebContents webContents) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> ImeAdapter.fromWebContents(webContents));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns {@link GestureListenerManager} instance associated with a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static GestureListenerManager getGestureListenerManager(WebContents webContents) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> GestureListenerManager.fromWebContents(webContents));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns {@link ViewEventSink} instance associated with a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static ViewEventSink getViewEventSink(WebContents webContents) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> ViewEventSink.from(webContents));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Injects the passed Javascript code in the current page and evaluates it, supplying a fake
     * user gesture. This also differs from {@link WebContents#evaluateJavaScript()} in that it
     * allows for executing script in non-webui frames.
     *
     * @param script The Javascript to execute.
     */
    public static void evaluateJavaScriptWithUserGesture(WebContents webContents, String script) {
        if (script == null) return;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> nativeEvaluateJavaScriptWithUserGesture(webContents, script));
    }

    /**
     * Create and initialize a new {@link SelectionPopupController} instance for testing.
     *
     * @param webContents {@link WebContents} object.
     * @return {@link SelectionPopupController} object used for the give WebContents.
     *         Creates one if not present.
     */
    public static SelectionPopupController createSelectionPopupController(WebContents webContents) {
        return SelectionPopupControllerImpl.createForTesting(webContents);
    }

    private static native void nativeReportAllFrameSubmissions(
            WebContents webContents, boolean enabled);
    private static native RenderFrameHost nativeGetFocusedFrame(WebContents webContents);
    private static native void nativeEvaluateJavaScriptWithUserGesture(
            WebContents webContents, String script);
}
