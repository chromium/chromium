// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_shell.Shell;

import java.util.concurrent.ExecutionException;

/**
 * Test suite to verify the behavior of the shell management logic.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentShellShellManagementTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String TEST_PAGE_1 = UrlUtils.encodeHtmlDataUri(
            "<html><body style='background: red;'></body></html>");
    private static final String TEST_PAGE_2 = UrlUtils.encodeHtmlDataUri(
            "<html><body style='background: green;'></body></html>");

    @Test
    @SmallTest
    @Feature({"Main"})
    @RetryOnFailure
    public void testMultipleShellsLaunched() throws ExecutionException {
        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl(TEST_PAGE_1);
        Assert.assertEquals(TEST_PAGE_1,
                activity.getActiveShell()
                        .getWebContents()
                        .getVisibleUrl());

        Shell previousActiveShell = activity.getActiveShell();
        Assert.assertFalse(previousActiveShell.isDestroyed());

        mActivityTestRule.loadNewShell(TEST_PAGE_2);
        Assert.assertEquals(TEST_PAGE_2,
                activity.getActiveShell()
                        .getWebContents()
                        .getVisibleUrl());

        Assert.assertNotSame(previousActiveShell, activity.getActiveShell());
        Assert.assertNull(previousActiveShell.getWebContents());
        Assert.assertTrue(previousActiveShell.isDestroyed());
    }
}
