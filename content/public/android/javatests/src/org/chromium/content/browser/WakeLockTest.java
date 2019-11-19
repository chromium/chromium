// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the Wake Lock API.
 */
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
            Assert.fail("Couldn't load test page.");
        }
    }

    private void getWakeLock(String type) throws TimeoutException {
        final String code = "navigator.wakeLock.request('" + type + "');";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mActivityTestRule.getWebContents(), code);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    @Feature({"WakeLock"})
    public void testScreenLock() throws Exception {
        Assert.assertFalse(mActivityTestRule.getActivity()
                                   .getActiveShell()
                                   .getContentView()
                                   .getKeepScreenOn());

        getWakeLock("screen");

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity()
                        .getActiveShell()
                        .getContentView()
                        .getKeepScreenOn();
            }
        });
    }
}
