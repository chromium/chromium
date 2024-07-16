// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.TerminationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.concurrent.TimeoutException;

/** Collection of test-only WebContents utilities. */
@JNINamespace("content")
public class WebContentsUtils {
    /**
     * Reports all frame submissions to the browser process, even those that do not impact Browser
     * UI.
     *
     * @param webContents The WebContents for which to report all frame submissions.
     * @param enabled Whether to report all frame submissions.
     */
    public static void reportAllFrameSubmissions(final WebContents webContents, boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContentsUtilsJni.get().reportAllFrameSubmissions(webContents, enabled);
                });
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
        return WebContentsUtilsJni.get().getFocusedFrame(webContents);
    }

    /**
     * Issues a fake notification about the renderer being killed.
     *
     * @param webContents The WebContents in use.
     */
    public static void simulateRendererKilled(WebContents webContents) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ((WebContentsImpl) webContents).simulateRendererKilledForTesting());
    }

    /**
     * Returns {@link ImeAdapter} instance associated with a given {@link WebContents}.
     *
     * @param webContents The WebContents in use.
     */
    public static ImeAdapter getImeAdapter(WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(() -> ImeAdapter.fromWebContents(webContents));
    }

    /**
     * Returns {@link GestureListenerManager} instance associated with a given {@link WebContents}.
     *
     * @param webContents The WebContents in use.
     */
    public static GestureListenerManager getGestureListenerManager(WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> GestureListenerManager.fromWebContents(webContents));
    }

    /**
     * Returns {@link ViewEventSink} instance associated with a given {@link WebContents}.
     *
     * @param webContents The WebContents in use.
     */
    public static ViewEventSink getViewEventSink(WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(() -> ViewEventSink.from(webContents));
    }

    /**
     * Injects the passed Javascript code in the current page and evaluates it, supplying a fake
     * user gesture. This also differs from {@link WebContents#evaluateJavaScript()} in that it
     * allows for executing script in non-webui frames.
     *
     * @param script The Javascript to execute.
     */
    public static void evaluateJavaScriptWithUserGesture(
            WebContents webContents, String script, @Nullable JavaScriptCallback callback) {
        if (script == null) return;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebContentsUtilsJni.get()
                                .evaluateJavaScriptWithUserGesture(webContents, script, callback));
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

    /**
     * Checks if the given WebContents has a valid {@link ActionMode.Callback} set in place.
     * @return {@code true} if WebContents (its SelectionPopupController) has a valid
     *         action mode callback object.
     */
    public static boolean isActionModeSupported(WebContents webContents) {
        SelectionPopupControllerImpl controller =
                ((SelectionPopupControllerImpl)
                        SelectionPopupController.fromWebContents(webContents));
        return controller.isActionModeSupported();
    }

    /** Cause the renderer process for the given WebContents to crash. */
    public static void crashTabAndWait(WebContents webContents) throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        WebContentsObserver observer =
                new WebContentsObserver() {
                    @Override
                    public void primaryMainFrameRenderProcessGone(
                            @TerminationStatus int terminationStatus) {
                        callbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.addObserver(observer);
                    WebContentsUtilsJni.get().crashTab(webContents);
                });
        callbackHelper.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.removeObserver(observer);
                });
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResult(String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    /**
     * Blocks the current execution until the primary main frame is in a steady state so the caller
     * can issue an `viz::CopyOutputRequest` against it.
     *
     * <p>See also, WaitForCopyableViewInFrame in content_browser_test_utils_internal.h.
     *
     * @param webContents The WebContents whose main frame we wish to wait on.
     */
    public static void waitForCopyableViewInWebContents(final WebContents webContents)
            throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContentsUtilsJni.get()
                            .notifyCopyableViewInWebContents(
                                    webContents,
                                    () -> {
                                        callbackHelper.notifyCalled();
                                    });
                });
        callbackHelper.waitForOnly();
    }

    @NativeMethods
    interface Natives {
        void reportAllFrameSubmissions(WebContents webContents, boolean enabled);

        RenderFrameHost getFocusedFrame(WebContents webContents);

        void evaluateJavaScriptWithUserGesture(
                WebContents webContents, String script, @Nullable JavaScriptCallback callback);

        void crashTab(WebContents webContents);

        void notifyCopyableViewInWebContents(WebContents webContents, Runnable doneCallback);
    }
}
