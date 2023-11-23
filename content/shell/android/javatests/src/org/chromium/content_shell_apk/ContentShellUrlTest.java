// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;

/** Example test that just starts the content shell. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentShellUrlTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    // URL used for base tests.
    private static final String URL = "data:text";

    @Test
    @SmallTest
    @Feature({"Main"})
    @DisabledTest(message = "https://crbug.com/1371971")
    public void testBaseStartup() {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(URL);

        // Make sure the activity was created as expected.
        Assert.assertNotNull(activity);

        // Make sure that the URL is set as expected.
        Assert.assertEquals(
                URL, activity.getActiveShell().getWebContents().getVisibleUrl().getSpec());
    }
}
