// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.annotation.TargetApi;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.PowerManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.text.TextUtils;
import android.view.ViewGroup;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell.ShellViewAndroidDelegate.OnCursorUpdateHelper;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * ActivityTestRule for ContentShellActivity.
 *
 * Test can use this ActivityTestRule to launch or get ContentShellActivity.
 */
public class ContentShellActivityTestRule extends ActivityTestRule<ContentShellActivity> {
    /** The maximum time the waitForActiveShellToBeDoneLoading method will wait. */
    private static final long WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = 10000L;

    private static final String TAG = "ContentShellATR";

    protected static final long WAIT_PAGE_LOADING_TIMEOUT_SECONDS = 15L;

    private final boolean mLaunchActivity;

    public ContentShellActivityTestRule() {
        this(false, false);
    }

    public ContentShellActivityTestRule(boolean initialTouchMode, boolean launchActivity) {
        super(ContentShellActivity.class, initialTouchMode, launchActivity);
        mLaunchActivity = launchActivity;
    }

    @Override
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH)
    @SuppressWarnings("deprecation")
    protected void beforeActivityLaunched() {
        PowerManager pm = (PowerManager) InstrumentationRegistry.getInstrumentation()
                                  .getContext()
                                  .getSystemService(Context.POWER_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isInteractive());
        } else {
            Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isScreenOn());
        }
    }

    /**
     * Starts the ContentShell activity and loads the given URL.
     * The URL can be null, in which case will default to ContentShellActivity.DEFAULT_SHELL_URL.
     */
    public ContentShellActivity launchContentShellWithUrl(String url) {
        Assert.assertFalse(
                "Activity is already launched, setup the test rule to NOT auto-launch activity",
                mLaunchActivity);
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        ContentShellActivity.class));
        return launchActivity(intent);
    }

    /**
     * Starts the content shell activity with the provided test url.
     * The url is synchronously loaded.
     * @param url Test url to load.
     */
    public ContentShellActivity launchContentShellWithUrlSync(String url) {
        String isolatedTestFileUrl = UrlUtils.getIsolatedTestFileUrl(url);
        ContentShellActivity activity = launchContentShellWithUrl(isolatedTestFileUrl);
        Assert.assertNotNull(getActivity());
        waitForActiveShellToBeDoneLoading();
        Assert.assertEquals(isolatedTestFileUrl, getWebContents().getLastCommittedUrl());
        return activity;
    }

    /**
     * Returns the OnCursorUpdateHelper.
     */
    public OnCursorUpdateHelper getOnCursorUpdateHelper() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<OnCursorUpdateHelper>() {
            @Override
            public OnCursorUpdateHelper call() {
                return getActivity()
                        .getActiveShell()
                        .getViewAndroidDelegate()
                        .getOnCursorUpdateHelper();
            }
        });
    }

    /**
     * Returns the current {@link ViewEventSink} or null if there is none;
     */
    public ViewEventSink getViewEventSink() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> {
                return ViewEventSink.from(getActivity().getActiveShell().getWebContents());
            });
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns the WebContents of this Shell.
     */
    public WebContents getWebContents() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> { return getActivity().getActiveShell().getWebContents(); });
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns the {@link SelectionPopupControllerImpl} of the WebContents.
     */
    public SelectionPopupControllerImpl getSelectionPopupController() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> {
                return SelectionPopupControllerImpl.fromWebContents(
                        getActivity().getActiveShell().getWebContents());
            });
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns the {@link ImeAdapterImpl} of the WebContents.
     */
    public ImeAdapterImpl getImeAdapter() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> ImeAdapterImpl.fromWebContents(getWebContents()));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns the {@link SelectPopup} of the WebContents.
     */
    public SelectPopup getSelectPopup() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> SelectPopup.fromWebContents(getWebContents()));
        } catch (ExecutionException e) {
            return null;
        }
    }

    public WebContentsAccessibilityImpl getWebContentsAccessibility() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> WebContentsAccessibilityImpl.fromWebContents(getWebContents()));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns the RenderCoordinates of the WebContents.
     */
    public RenderCoordinatesImpl getRenderCoordinates() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> ((WebContentsImpl) getWebContents()).getRenderCoordinates());
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns the current container view or null if there is no WebContents.
     */
    public ViewGroup getContainerView() {
        final WebContents webContents = getWebContents();
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> {
                return webContents != null ? webContents.getViewAndroidDelegate().getContainerView()
                                           : null;
            });
        } catch (ExecutionException e) {
            Log.w(TAG, "Getting container view failed. Returning null", e);
            return null;
        }
    }

    public JavascriptInjector getJavascriptInjector() {
        return JavascriptInjector.fromWebContents(getWebContents());
    }

    /**
     * Waits for the Active shell to finish loading.  This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds and it shouldn't be used for long
     * loading pages. Instead it should be used more for test initialization. The proper way
     * to wait is to use a TestCallbackHelperContainer after the initial load is completed.
     */
    public void waitForActiveShellToBeDoneLoading() {
        // Wait for the Content Shell to be initialized.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Shell shell = getActivity().getActiveShell();
                // There are two cases here that need to be accounted for.
                // The first is that we've just created a Shell and it isn't
                // loading because it has no URL set yet.  The second is that
                // we've set a URL and it actually is loading.
                if (shell == null) {
                    updateFailureReason("Shell is null.");
                    return false;
                }
                if (shell.isLoading()) {
                    updateFailureReason("Shell is still loading.");
                    return false;
                }
                if (TextUtils.isEmpty(shell.getWebContents().getLastCommittedUrl())) {
                    updateFailureReason("Shell's URL is empty or null.");
                    return false;
                }
                return true;
            }
        }, WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Creates a new {@link Shell} and waits for it to finish loading.
     * @param url The URL to create the new {@link Shell} with.
     * @return A new instance of a {@link Shell}.
     * @throws ExecutionException
     */
    public Shell loadNewShell(String url) throws ExecutionException {
        Shell shell = TestThreadUtils.runOnUiThreadBlocking(new Callable<Shell>() {
            @Override
            public Shell call() {
                getActivity().getShellManager().launchShell(url);
                return getActivity().getActiveShell();
            }
        });
        Assert.assertNotNull("Unable to create shell.", shell);
        Assert.assertEquals("Active shell unexpected.", shell,
                getActivity().getActiveShell());
        waitForActiveShellToBeDoneLoading();
        return shell;
    }

    /**
     * Loads a URL in the specified content view.
     *
     * @param navigationController The navigation controller to load the URL in.
     * @param callbackHelperContainer The callback helper container used to monitor progress.
     * @param params The URL params to use.
     */
    public void loadUrl(NavigationController navigationController,
            TestCallbackHelperContainer callbackHelperContainer, LoadUrlParams params)
            throws Throwable {
        handleBlockingCallbackAction(
                callbackHelperContainer.getOnPageFinishedHelper(), new Runnable() {
                    @Override
                    public void run() {
                        navigationController.loadUrl(params);
                    }
                });
    }

    /**
     * Handles performing an action on the UI thread that will return when the specified callback
     * is incremented.
     *
     * @param callbackHelper The callback helper that will be blocked on.
     * @param uiThreadAction The action to be performed on the UI thread.
     */
    public void handleBlockingCallbackAction(CallbackHelper callbackHelper, Runnable uiThreadAction)
            throws Throwable {
        int currentCallCount = callbackHelper.getCallCount();
        runOnUiThread(uiThreadAction);
        callbackHelper.waitForCallback(
                currentCallCount, 1, WAIT_PAGE_LOADING_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    // TODO(aelias): This method needs to be removed once http://crbug.com/179511 is fixed.
    // Meanwhile, we have to wait if the page has the <meta viewport> tag.
    /**
     * Waits till the RenderCoordinates receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        final RenderCoordinatesImpl coord = getRenderCoordinates();
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(expectedScale, new Callable<Float>() {
                    @Override
                    public Float call() {
                        return coord.getPageScaleFactor();
                    }
                }));
    }

    /**
     * Annotation for tests that should be executed a second time after replacing
     * the container view.
     * <p>Please note that activity launch is only invoked once before both runs,
     * and that any state changes produced by the first run are visible to the second run.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface RerunWithUpdatedContainerView {}
}
