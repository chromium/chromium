// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.app.Instrumentation;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.InstrumentationUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Collection of utilities related to the UiThread for navigating
 * through and working with browser forward and back history.
 */
public class HistoryUtils {
    protected static final long WAIT_TIMEOUT_SECONDS = 15L;

    /**
     * Calls {@link NavigationController#canGoBack()} on UI thread.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     * @return result of {@link NavigationController#canGoBack()}
     */
    public static boolean canGoBackOnUiThread(
            Instrumentation instrumentation, final WebContents webContents) throws Throwable {
        return InstrumentationUtils.runOnMainSyncAndGetResult(
                instrumentation,
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return webContents.getNavigationController().canGoBack();
                    }
                });
    }

    /**
     * Calls {@link NavigationController#canGoToOffset(int)} on UI thread.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     * @param offset The number of steps to go on the UI thread, with negative representing going
     *     back.
     * @return result of {@link NavigationController#canGoToOffset(int)}
     */
    public static boolean canGoToOffsetOnUiThread(
            Instrumentation instrumentation, final WebContents webContents, final int offset)
            throws Throwable {
        return InstrumentationUtils.runOnMainSyncAndGetResult(
                instrumentation,
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return webContents.getNavigationController().canGoToOffset(offset);
                    }
                });
    }

    /**
     * Calls {@link NavigationController#canGoForward()} on UI thread.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     * @return result of {@link NavigationController#canGoForward()}
     */
    public static boolean canGoForwardOnUiThread(
            Instrumentation instrumentation, final WebContents webContents) throws Throwable {
        return InstrumentationUtils.runOnMainSyncAndGetResult(
                instrumentation,
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return webContents.getNavigationController().canGoForward();
                    }
                });
    }

    /**
     * Calls {@link NavigationController#clearHistory()} on UI thread.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     */
    public static void clearHistoryOnUiThread(
            Instrumentation instrumentation, final WebContents webContents) {
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        webContents.getNavigationController().clearHistory();
                    }
                });
    }

    /**
     * Calls {@link WebContents#getLastCommittedUrl()} on UI Thread to get the current URL.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     * @return the last committed URL of the provided WebContents.
     */
    public static String getUrlOnUiThread(
            Instrumentation instrumentation, final WebContents webContents) throws Throwable {
        return InstrumentationUtils.runOnMainSyncAndGetResult(
                instrumentation,
                new Callable<String>() {
                    @Override
                    public String call() {
                        return webContents.getLastCommittedUrl().getSpec();
                    }
                });
    }

    /**
     * Goes back on UI thread and waits until onPageFinished is called or until it times out.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     * @param onPageFinishedHelper the CallbackHelper instance associated with the onPageFinished
     *     callback of webContents.
     */
    public static void goBackSync(
            Instrumentation instrumentation,
            final WebContents webContents,
            CallbackHelper onPageFinishedHelper)
            throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        webContents.getNavigationController().goBack();
                    }
                });

        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Goes forward on UI thread and waits until onPageFinished is called or until it times out.
     *
     * @param instrumentation an Instrumentation instance.
     * @param webContents a WebContents instance.
     */
    public static void goForwardSync(
            Instrumentation instrumentation,
            final WebContents webContents,
            CallbackHelper onPageFinishedHelper)
            throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        webContents.getNavigationController().goForward();
                    }
                });

        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }
}
