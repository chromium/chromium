// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.content_shell_apk.ContentShellActivityTestRule.RerunWithUpdatedContainerView;

import java.util.concurrent.TimeUnit;

/**
 * Integration Tests for SelectPopup.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SelectPopupTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final long WAIT_TIMEOUT_SECONDS = 2L;
    private static final String SELECT_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
            + "<body>Which animal is the strongest:<br/>"
            + "<select id=\"select\">"
            + "<option>Black bear</option>"
            + "<option>Polar bear</option>"
            + "<option>Grizzly</option>"
            + "<option>Tiger</option>"
            + "<option>Lion</option>"
            + "<option>Gorilla</option>"
            + "<option>Chipmunk</option>"
            + "</select>"
            + "</body></html>");

    private class PopupShowingCriteria extends Criteria {
        public PopupShowingCriteria() {
            super("The select popup is not showing as expected.");
        }

        @Override
        public boolean isSatisfied() {
            return mActivityTestRule.getSelectPopup().isVisibleForTesting();
        }
    }

    private class PopupHiddenCriteria extends Criteria {
        public PopupHiddenCriteria() {
            super("The select popup is not hidden as expected.");
        }

        @Override
        public boolean isSatisfied() {
            return !mActivityTestRule.getSelectPopup().isVisibleForTesting();
        }
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchContentShellWithUrl(SELECT_URL);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
    }

    /**
     * Tests that showing a select popup and having the page reload while the popup is showing does
     * not assert.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    @RerunWithUpdatedContainerView
    @RetryOnFailure
    public void testReloadWhilePopupShowing() throws Exception, Throwable {
        // The popup should be hidden before the click.
        CriteriaHelper.pollUiThread(new PopupHiddenCriteria());

        final WebContents webContents = mActivityTestRule.getWebContents();
        final TestCallbackHelperContainer viewClient = new TestCallbackHelperContainer(webContents);
        final OnPageFinishedHelper onPageFinishedHelper = viewClient.getOnPageFinishedHelper();

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(webContents, "select");
        CriteriaHelper.pollInstrumentationThread(new PopupShowingCriteria());

        // Reload the test page.
        int currentCallCount = onPageFinishedHelper.getCallCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Now reload the page while the popup is showing, it gets hidden.
            mActivityTestRule.getWebContents().getNavigationController().reload(true);
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // The popup should be hidden after the page reload.
        CriteriaHelper.pollUiThread(new PopupHiddenCriteria());

        // Click the select and wait for the popup to show.
        DOMUtils.clickNode(webContents, "select");
        CriteriaHelper.pollUiThread(new PopupShowingCriteria());
    }
}
