// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;

/**
 * Test suite for ensureing that Geolocation interacts as expected
 * with ContentView APIs - e.g. that it's started and stopped as the
 * ContentView is hidden or shown.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentViewLocationTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private TestCallbackHelperContainer mTestCallbackHelperContainer;
    private TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper mJavascriptHelper;
    private MockLocationProvider mMockLocationProvider;

    private void hideContentViewOnUiThread() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                mActivityTestRule
                                        .getWebContents()
                                        .updateWebContentsVisibility(Visibility.HIDDEN);
                            }
                        });
    }

    private void showContentViewOnUiThread() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                mActivityTestRule
                                        .getWebContents()
                                        .updateWebContentsVisibility(Visibility.VISIBLE);
                            }
                        });
    }

    private void pollForPositionCallback() throws Throwable {
        mJavascriptHelper.evaluateJavaScriptForTests(
                mActivityTestRule.getWebContents(), "positionCount = 0");
        mJavascriptHelper.waitUntilHasValue();
        Assert.assertEquals(0, Integer.parseInt(mJavascriptHelper.getJsonResultAndClear()));

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    mJavascriptHelper.evaluateJavaScriptForTests(
                            mActivityTestRule.getWebContents(), "positionCount");
                    try {
                        mJavascriptHelper.waitUntilHasValue();
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                    int result = Integer.parseInt(mJavascriptHelper.getJsonResultAndClear());
                    Criteria.checkThat(result, Matchers.greaterThan(0));
                });
    }

    private void startGeolocationWatchPosition() throws Throwable {
        mJavascriptHelper.evaluateJavaScriptForTests(
                mActivityTestRule.getWebContents(), "initiate_watchPosition();");
        mJavascriptHelper.waitUntilHasValue();
    }

    private void ensureGeolocationRunning(final boolean running) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mMockLocationProvider.isRunning(), Matchers.is(running));
                });
    }

    @Before
    public void setUp() {
        mMockLocationProvider = new MockLocationProvider();
        LocationProviderOverrider.setLocationProviderImpl(mMockLocationProvider);

        try {
            mActivityTestRule.launchContentShellWithUrlSync(
                    "content/test/data/android/geolocation.html");
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }

        mTestCallbackHelperContainer =
                new TestCallbackHelperContainer(mActivityTestRule.getWebContents());
        mJavascriptHelper = new OnEvaluateJavaScriptResultHelper();

        ensureGeolocationRunning(false);
    }

    @After
    public void tearDown() {
        mMockLocationProvider.stopUpdates();
    }

    @Test
    @MediumTest
    @Feature({"Location"})
    public void testWatchHideShowStop() throws Throwable {
        startGeolocationWatchPosition();
        pollForPositionCallback();
        ensureGeolocationRunning(true);

        // Now hide the ContentView and ensure that geolocation stops.
        hideContentViewOnUiThread();
        ensureGeolocationRunning(false);

        mJavascriptHelper.evaluateJavaScriptForTests(
                mActivityTestRule.getWebContents(), "positionCount = 0");
        mJavascriptHelper.waitUntilHasValue();

        // Show the ContentView again and ensure that geolocation starts again.
        showContentViewOnUiThread();
        pollForPositionCallback();
        ensureGeolocationRunning(true);

        // Navigate away and ensure that geolocation stops.
        mActivityTestRule.loadUrl(
                mActivityTestRule.getWebContents().getNavigationController(),
                mTestCallbackHelperContainer,
                new LoadUrlParams("about:blank"));
        ensureGeolocationRunning(false);
    }

    @Test
    @MediumTest
    @Feature({"Location"})
    public void testHideWatchResume() throws Throwable {
        hideContentViewOnUiThread();
        startGeolocationWatchPosition();
        ensureGeolocationRunning(false);

        showContentViewOnUiThread();
        pollForPositionCallback();
        ensureGeolocationRunning(true);
    }

    @Test
    @MediumTest
    @Feature({"Location"})
    public void testWatchHideNewWatchShow() throws Throwable {
        startGeolocationWatchPosition();
        pollForPositionCallback();
        ensureGeolocationRunning(true);

        hideContentViewOnUiThread();

        // Make sure that when starting a new watch while paused we still don't
        // start up geolocation until we show the content view again.
        startGeolocationWatchPosition();
        ensureGeolocationRunning(false);

        showContentViewOnUiThread();
        pollForPositionCallback();
        ensureGeolocationRunning(true);
    }

    @Test
    @MediumTest
    @Feature({"Location"})
    public void testHideWatchStopShow() throws Throwable {
        hideContentViewOnUiThread();
        startGeolocationWatchPosition();
        ensureGeolocationRunning(false);

        mActivityTestRule.loadUrl(
                mActivityTestRule.getWebContents().getNavigationController(),
                mTestCallbackHelperContainer,
                new LoadUrlParams("about:blank"));
        showContentViewOnUiThread();
        ensureGeolocationRunning(false);
    }
}
