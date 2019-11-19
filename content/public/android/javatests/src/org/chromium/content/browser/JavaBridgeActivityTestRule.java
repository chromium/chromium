// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.test.SetUpStatement;
import org.chromium.base.test.SetUpTestRule;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.lang.annotation.Annotation;

/**
 * ActivityTestRule with common functionality for testing the Java Bridge.
 */
public class JavaBridgeActivityTestRule
        extends ContentShellActivityTestRule implements SetUpTestRule<JavaBridgeActivityTestRule> {
    private TestCallbackHelperContainer mTestCallbackHelperContainer;
    private boolean mSetup;

    public static class Controller {
        private static final int RESULT_WAIT_TIME = 5000;

        private boolean mIsResultReady;

        protected synchronized void notifyResultIsReady() {
            mIsResultReady = true;
            notify();
        }

        protected synchronized void waitForResult() {
            while (!mIsResultReady) {
                try {
                    wait(RESULT_WAIT_TIME);
                } catch (Exception e) {
                    continue;
                }
                if (!mIsResultReady) {
                    Assert.fail("Wait timed out");
                }
            }
            mIsResultReady = false;
        }
    }

    public TestCallbackHelperContainer getTestCallBackHelperContainer() {
        return mTestCallbackHelperContainer;
    }

    /**
     * Sets up the ContentView. Intended to be called from setUp().
     */
    public void setUpContentView() {
        // This starts the activity, so must be called on the test thread.
        final ContentShellActivity activity = launchContentShellWithUrl(
                UrlUtils.encodeHtmlDataUri("<html><head></head><body>test</body></html>"));
        waitForActiveShellToBeDoneLoading();

        try {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mTestCallbackHelperContainer =
                            new TestCallbackHelperContainer(activity.getActiveWebContents());
                }
            });
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to set up ContentView: " + Log.getStackTraceString(e));
        }
    }

    public void executeJavaScript(String script) throws Throwable {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // When a JavaScript URL is executed, if the value of the last
                // expression evaluated is not 'undefined', this value is
                // converted to a string and used as the new document for the
                // frame. We don't want this behaviour, so wrap the script in
                // an anonymous function.
                getWebContents().getNavigationController().loadUrl(
                        new LoadUrlParams("javascript:(function() { " + script + " })()"));
            }
        });
    }

    public void injectObjectAndReload(Object object, String name) {
        injectObjectAndReload(object, name, null);
    }

    public void injectObjectAndReload(
            Object object, String name, Class<? extends Annotation> requiredAnnotation) {
        injectObjectsAndReload(object, name, null, null, requiredAnnotation);
    }

    public void injectObjectsAndReload(final Object object1, final String name1,
            final Object object2, final String name2,
            final Class<? extends Annotation> requiredAnnotation) {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        try {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    WebContents webContents = getWebContents();
                    JavascriptInjector injector = JavascriptInjector.fromWebContents(webContents);
                    injector.addPossiblyUnsafeInterface(object1, name1, requiredAnnotation);
                    if (object2 != null && name2 != null) {
                        injector.addPossiblyUnsafeInterface(object2, name2, requiredAnnotation);
                    }
                    webContents.getNavigationController().reload(true);
                }
            });
            onPageFinishedHelper.waitForCallback(currentCallCount);
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to injectObjectsAndReload: " + Log.getStackTraceString(e));
        }
    }

    public void synchronousPageReload() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getWebContents().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }


    @Override
    public Statement apply(Statement base, Description desc) {
        SetUpStatement setUpBase = new SetUpStatement(base, this, mSetup);
        return super.apply(setUpBase, desc);
    }

    @Override
    public JavaBridgeActivityTestRule shouldSetUp(boolean runSetUp) {
        mSetup = runSetUp;
        return this;
    }

    @Override
    public void setUp() {
        setUpContentView();
    }
}
