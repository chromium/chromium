// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * Test suite for viewport-related properties.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ViewportTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    protected String evaluateStringValue(String expression) throws Throwable {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), expression);
    }

    protected float evaluateFloatValue(String expression) throws Throwable {
        return Float.valueOf(evaluateStringValue(expression));
    }

    protected int evaluateIntegerValue(String expression) throws Throwable {
        return Integer.parseInt(evaluateStringValue(expression));
    }

    @Test
    @MediumTest
    @Feature({"Viewport", "InitialViewportSize"})
    public void testDefaultViewportSize() throws Throwable {
        mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        Context context = InstrumentationRegistry.getTargetContext();
        WindowManager winManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        DisplayMetrics metrics = new DisplayMetrics();
        winManager.getDefaultDisplay().getMetrics(metrics);

        // window.devicePixelRatio should match the default display. Only check to 1 decimal place
        // to allow for rounding.
        Assert.assertEquals(metrics.density, evaluateFloatValue("window.devicePixelRatio"), 0.1);

        // Check that the viewport width is vaguely sensible.
        int viewportWidth = evaluateIntegerValue("document.documentElement.clientWidth");
        Assert.assertTrue(Math.abs(evaluateIntegerValue("window.innerWidth") - viewportWidth) <= 1);
        Assert.assertTrue(viewportWidth >= 979);
        Assert.assertTrue(
                viewportWidth <= Math.max(981, metrics.widthPixels / metrics.density + 1));
    }
}
