// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.TimeoutException;

/** Integration tests for the Wake Lock API. */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-experimental-web-platform-features"})
public class WakeLockTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String TEST_PATH = "content/test/data/android/title1.html";

    @Before
    public void setUp() {
        try {
            mActivityTestRule.launchContentShellWithUrlSync(TEST_PATH);
        } catch (Throwable t) {
            throw new AssertionError("Couldn't load test page.", t);
        }
    }

    private void getWakeLock(String type) throws TimeoutException {
        final String code = "navigator.wakeLock.request('" + type + "');";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mActivityTestRule.getWebContents(), code);
    }

    @Test
    @SmallTest
    @Feature({"WakeLock"})
    public void testScreenLock() throws Exception {
        Assert.assertFalse(
                mActivityTestRule
                        .getActivity()
                        .getActiveShell()
                        .getContentView()
                        .getKeepScreenOn());

        getWakeLock("screen");

        CriteriaHelper.pollInstrumentationThread(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getActiveShell()
                                .getContentView()
                                .getKeepScreenOn());
    }
}
