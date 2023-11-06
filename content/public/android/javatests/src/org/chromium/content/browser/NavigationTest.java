// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/** Tests for various aspects of navigation. */
@RunWith(BaseJUnit4ClassRunner.class)
public class NavigationTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String URL_1 = UrlUtils.encodeHtmlDataUri("<html>1</html>");
    private static final String URL_2 = UrlUtils.encodeHtmlDataUri("<html>2</html>");
    private static final String URL_3 = UrlUtils.encodeHtmlDataUri("<html>3</html>");
    private static final String URL_4 = UrlUtils.encodeHtmlDataUri("<html>4</html>");
    private static final String URL_5 = UrlUtils.encodeHtmlDataUri("<html>5</html>");
    private static final String URL_6 = UrlUtils.encodeHtmlDataUri("<html>6</html>");
    private static final String URL_7 = UrlUtils.encodeHtmlDataUri("<html>7</html>");

    private void goBack(
            final NavigationController navigationController,
            TestCallbackHelperContainer testCallbackHelperContainer)
            throws Throwable {
        mActivityTestRule.handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        navigationController.goBack();
                    }
                });
    }

    private void reload(
            final NavigationController navigationController,
            TestCallbackHelperContainer testCallbackHelperContainer)
            throws Throwable {
        mActivityTestRule.handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        navigationController.reload(true);
                    }
                });
    }

    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisabledTest(message = "https://crbug.com/1316064")
    public void testDirectedNavigationHistory() throws Throwable {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();
        NavigationController navigationController = webContents.getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(webContents);

        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_2));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_3));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_4));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_5));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_6));
        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_7));

        NavigationHistory history = navigationController.getDirectedNavigationHistory(false, 3);
        Assert.assertEquals(3, history.getEntryCount());
        Assert.assertEquals(URL_6, history.getEntryAtIndex(0).getUrl().getSpec());
        Assert.assertEquals(URL_5, history.getEntryAtIndex(1).getUrl().getSpec());
        Assert.assertEquals(URL_4, history.getEntryAtIndex(2).getUrl().getSpec());

        history = navigationController.getDirectedNavigationHistory(true, 3);
        Assert.assertEquals(history.getEntryCount(), 0);

        goBack(navigationController, testCallbackHelperContainer);
        goBack(navigationController, testCallbackHelperContainer);
        goBack(navigationController, testCallbackHelperContainer);

        history = navigationController.getDirectedNavigationHistory(false, 4);
        Assert.assertEquals(3, history.getEntryCount());
        Assert.assertEquals(URL_3, history.getEntryAtIndex(0).getUrl().getSpec());
        Assert.assertEquals(URL_2, history.getEntryAtIndex(1).getUrl().getSpec());
        Assert.assertEquals(URL_1, history.getEntryAtIndex(2).getUrl().getSpec());

        history = navigationController.getDirectedNavigationHistory(true, 4);
        Assert.assertEquals(3, history.getEntryCount());
        Assert.assertEquals(URL_5, history.getEntryAtIndex(0).getUrl().getSpec());
        Assert.assertEquals(URL_6, history.getEntryAtIndex(1).getUrl().getSpec());
        Assert.assertEquals(URL_7, history.getEntryAtIndex(2).getUrl().getSpec());
    }

    /**
     * Tests whether a page was successfully reloaded.
     * Checks to make sure that OnPageFinished events were fired and that the timestamps of when
     * the page loaded are different after the reload.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testPageReload() throws Throwable {
        final String htmlLoadTime =
                "<html><head><script type=\"text/javascript\">var loadTimestamp = new"
                        + " Date().getTime();function getLoadtime() { return loadTimestamp;"
                        + " }</script></head></html>";
        final String urlLoadTime = UrlUtils.encodeHtmlDataUri(htmlLoadTime);

        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(urlLoadTime);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(webContents);
        OnEvaluateJavaScriptResultHelper javascriptHelper = new OnEvaluateJavaScriptResultHelper();

        // Grab the first timestamp.
        javascriptHelper.evaluateJavaScriptForTests(webContents, "getLoadtime();");
        javascriptHelper.waitUntilHasValue();
        String firstTimestamp = javascriptHelper.getJsonResultAndClear();
        Assert.assertNotNull("Timestamp was null.", firstTimestamp);

        // Grab the timestamp after a reload and make sure they don't match.
        reload(webContents.getNavigationController(), testCallbackHelperContainer);
        javascriptHelper.evaluateJavaScriptForTests(webContents, "getLoadtime();");
        javascriptHelper.waitUntilHasValue();
        String secondTimestamp = javascriptHelper.getJsonResultAndClear();
        Assert.assertNotNull("Timestamp was null.", secondTimestamp);
        Assert.assertFalse("Timestamps matched.", firstTimestamp.equals(secondTimestamp));
    }
}
