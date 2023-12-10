// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Collection of JavaScript utilities. */
public class JavaScriptUtils {
    private static final long EVALUATION_TIMEOUT_SECONDS = 5L;

    /**
     * Executes the given snippet of JavaScript code within the given ContentView.
     * Returns the result of its execution in JSON format.
     */
    public static String executeJavaScriptAndWaitForResult(WebContents webContents, String code)
            throws TimeoutException {
        return executeJavaScriptAndWaitForResult(
                webContents, code, EVALUATION_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Executes the given snippet of JavaScript code within the given WebContents.
     * Does not depend on ContentView and TestCallbackHelperContainer.
     * Returns the result of its execution in JSON format.
     */
    public static String executeJavaScriptAndWaitForResult(
            final WebContents webContents,
            final String code,
            final long timeout,
            final TimeUnit timeoutUnits)
            throws TimeoutException {
        final OnEvaluateJavaScriptResultHelper helper = new OnEvaluateJavaScriptResultHelper();
        // Calling this from the UI thread causes it to time-out: the UI thread being blocked won't
        // have a chance to process the JavaScript eval response).
        Assert.assertFalse(
                "Executing JavaScript should be done from the test thread, " + " not the UI thread",
                ThreadUtils.runningOnUiThread());
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> helper.evaluateJavaScriptForTests(webContents, code));
        helper.waitUntilHasValue(timeout, timeoutUnits);
        Assert.assertTrue("Failed to retrieve JavaScript evaluation results.", helper.hasValue());
        return helper.getJsonResultAndClear();
    }

    /**
     * Executes the given snippet of JavaScript code within the given ContentView, acting as if a
     * user gesture is present.
     * Returns the result of its execution in JSON format.
     */
    public static String executeJavaScriptWithUserGestureAndWaitForResult(
            WebContents webContents, String code) throws TimeoutException {
        return executeJavaScriptWithUserGestureAndWaitForResult(
                webContents, code, EVALUATION_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Executes the given snippet of JavaScript code within the given WebContents, acting as if a
     * user gesture is present.
     * Does not depend on ContentView and TestCallbackHelperContainer.
     * Returns the result of its execution in JSON format.
     */
    public static String executeJavaScriptWithUserGestureAndWaitForResult(
            final WebContents webContents,
            final String code,
            final long timeout,
            final TimeUnit timeoutUnits)
            throws TimeoutException {
        final OnEvaluateJavaScriptResultHelper helper = new OnEvaluateJavaScriptResultHelper();
        // Calling this from the UI thread causes it to time-out: the UI thread being blocked won't
        // have a chance to process the JavaScript eval response).
        Assert.assertFalse(
                "Executing JavaScript should be done from the test thread, " + " not the UI thread",
                ThreadUtils.runningOnUiThread());
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> helper.evaluateJavaScriptWithUserGestureForTests(webContents, code));
        helper.waitUntilHasValue(timeout, timeoutUnits);
        Assert.assertTrue("Failed to retrieve JavaScript evaluation results.", helper.hasValue());
        return helper.getJsonResultAndClear();
    }

    /**
     * Executes the given snippet of JavaScript code within the given WebContents and waits for a
     * call to domAutomationController.send(). Returns the result from
     * domAutomationController.send() in JSON format.
     */
    public static String runJavascriptWithAsyncResult(WebContents webContents, String code)
            throws TimeoutException {
        DomAutomationController controller = new DomAutomationController();
        controller.inject(webContents);
        executeJavaScript(webContents, code);
        return controller.waitForResult("No result for `" + code + "`");
    }

    /**
     * Executes the given snippet of JavaScript code within the given WebContents, with a user
     * gesture, and waits for a call to domAutomationController.send(). Returns the result from
     * domAutomationController.send() in JSON format.
     */
    public static String runJavascriptWithUserGestureAndAsyncResult(
            WebContents webContents, String code) throws TimeoutException {
        DomAutomationController controller = new DomAutomationController();
        controller.inject(webContents);
        WebContentsUtils.evaluateJavaScriptWithUserGesture(webContents, code, null);
        return controller.waitForResult("No result for `" + code + "`");
    }

    /** Executes the given snippet of JavaScript code but does not wait for the result. */
    public static void executeJavaScript(final WebContents webContents, final String code) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> webContents.evaluateJavaScriptForTests(code, null));
    }
}
