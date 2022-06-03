// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * This class is used to provide callback hooks for tests and related classes.
 */
public class TestCallbackHelperContainer {
    private TestWebContentsObserver mTestWebContentsObserver;

    public TestCallbackHelperContainer(final WebContents webContents) {
        // TODO(yfriedman): Change callers to be executed on the UI thread. Unfortunately this is
        // super convenient as the caller is nearly always on the test thread which is fine to block
        // and it's cumbersome to keep bouncing to the UI thread.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestWebContentsObserver = new TestWebContentsObserver(webContents); });
    }

    /**
     * CallbackHelper for OnPageCommitVisible.
     */
    public static class OnPageCommitVisibleHelper extends CallbackHelper {
        private String mUrl;
        public void notifyCalled(String url) {
            mUrl = url;
            notifyCalled();
        }
        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }
    }

    /**
     * CallbackHelper for OnPageFinished.
     */
    public static class OnPageFinishedHelper extends CallbackHelper {
        private List<String> mUrlList = Collections.synchronizedList(new ArrayList<>());
        private String mUrl;
        public void notifyCalled(String url) {
            mUrl = url;
            mUrlList.add(url);
            notifyCalled();
        }
        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }
        public List<String> getUrlList() {
            return mUrlList;
        }
    }

    /**
     * CallbackHelper for OnPageStarted.
     */
    public static class OnPageStartedHelper extends CallbackHelper {
        private String mUrl;
        public void notifyCalled(String url) {
            mUrl = url;
            notifyCalled();
        }
        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }
    }

    /**
     * CallbackHelper for OnReceivedError.
     */
    public static class OnReceivedErrorHelper extends CallbackHelper {
        private int mErrorCode;
        private String mDescription;
        private String mFailingUrl;
        public void notifyCalled(int errorCode, String description, String failingUrl) {
            mErrorCode = errorCode;
            mDescription = description;
            mFailingUrl = failingUrl;
            notifyCalled();
        }
        public int getErrorCode() {
            assert getCallCount() > 0;
            return mErrorCode;
        }
        public String getDescription() {
            assert getCallCount() > 0;
            return mDescription;
        }
        public String getFailingUrl() {
            assert getCallCount() > 0;
            return mFailingUrl;
        }
    }

    /**
     * CallbackHelper for OnEvaluateJavaScriptResult.
     * This class wraps the evaluation of JavaScript code allowing test code to
     * synchronously evaluate JavaScript and then test the result.
     */
    public static class OnEvaluateJavaScriptResultHelper extends CallbackHelper {
        private String mJsonResult;

        /**
         * Starts evaluation of a given JavaScript code on a given webContents.
         * @param webContents A WebContents instance to be used.
         * @param code A JavaScript code to be evaluated.
         */
        public void evaluateJavaScriptForTests(WebContents webContents, String code) {
            JavaScriptCallback callback = new JavaScriptCallback() {
                @Override
                public void handleJavaScriptResult(String jsonResult) {
                    notifyCalled(jsonResult);
                }
            };
            mJsonResult = null;
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> webContents.evaluateJavaScriptForTests(code, callback));
        }

        /**
         * Returns true if the evaluation started by evaluateJavaScriptForTests() has completed.
         */
        public boolean hasValue() {
            return mJsonResult != null;
        }

        /**
         * Returns the JSON result of a previously completed JavaScript evaluation and
         * resets the helper to accept new evaluations.
         * @return String JSON result of a previously completed JavaScript evaluation.
         */
        public String getJsonResultAndClear() {
            assert hasValue();
            String result = mJsonResult;
            mJsonResult = null;
            return result;
        }

        /**
         * Waits till the JavaScript evaluation finishes and returns true if a value was returned,
         * false if it timed-out.
         */
        public boolean waitUntilHasValue(long timeout, TimeUnit unit) throws TimeoutException {
            int count = getCallCount();
            // Reads and writes are atomic for reference variables in java, this is thread safe
            if (hasValue()) return true;
            waitForCallback(count, 1, timeout, unit);
            return hasValue();
        }

        public boolean waitUntilHasValue() throws TimeoutException {
            return waitUntilHasValue(CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        }

        public void notifyCalled(String jsonResult) {
            assert !hasValue();
            mJsonResult = jsonResult;
            notifyCalled();
        }
    }

    public OnPageStartedHelper getOnPageStartedHelper() {
        return mTestWebContentsObserver.getOnPageStartedHelper();
    }

    public OnPageFinishedHelper getOnPageFinishedHelper() {
        return mTestWebContentsObserver.getOnPageFinishedHelper();
    }

    public OnReceivedErrorHelper getOnReceivedErrorHelper() {
        return mTestWebContentsObserver.getOnReceivedErrorHelper();
    }

    public CallbackHelper getOnFirstVisuallyNonEmptyPaintHelper() {
        return mTestWebContentsObserver.getOnFirstVisuallyNonEmptyPaintHelper();
    }
}
