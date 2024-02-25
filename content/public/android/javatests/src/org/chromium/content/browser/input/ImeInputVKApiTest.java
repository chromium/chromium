// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/** IME (input method editor) and text input tests for VK policy and show/hide APIs. */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-blink-features=VirtualKeyboard", "expose-internals-for-testing"})
@Batch(ImeTest.IME_BATCH)
public class ImeInputVKApiTest {
    @Rule public ImeActivityTestRule mRule = new ImeActivityTestRule();
    @Rule public ExpectedException thrown = ExpectedException.none();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_VK_API_HTML);
    }

    @After
    public void tearDown() throws Exception {
        mRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @Feature({"VKShowAndHide", "Main"})
    public void testKeyboardShowAndHide() throws Throwable {
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        mRule.fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(ImeActivityTestRule.INPUT_VK_API_HTML));
        mRule.assertWaitForKeyboardStatus(false);

        // Show keyboard when manual policy element has focus and show API is called.
        DOMUtils.clickNode(mRule.getWebContents(), "txt1");
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard once keyboard shows up and hide API is called.
        final String code = "navigator.virtualKeyboard.hide()";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);
        mRule.assertWaitForKeyboardStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"VKShowAndDontHide", "Main"})
    public void testKeyboardShowAndNotHide() throws Throwable {
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        mRule.fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(ImeActivityTestRule.INPUT_VK_API_HTML));
        mRule.assertWaitForKeyboardStatus(false);

        // Show keyboard when auto policy element has focus.
        DOMUtils.clickNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);

        // Don't change the state of the keyboard when policy is manual.
        // Show the keyboard as the state was shown.
        DOMUtils.clickNode(mRule.getWebContents(), "txt2");
        mRule.assertWaitForKeyboardStatus(true);
    }

    @Test
    @MediumTest
    @Feature({"VKManualAndNotShow", "Main"})
    public void testKeyboardManualAndNotShow() throws Throwable {
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        mRule.fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(ImeActivityTestRule.INPUT_VK_API_HTML));
        mRule.assertWaitForKeyboardStatus(false);

        // Don't change the state of the keyboard when policy is manual.
        // Hide since the original state was hidden.
        DOMUtils.clickNode(mRule.getWebContents(), "txt2");
        mRule.assertWaitForKeyboardStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"VKManualAndNotShowAfterJSFocus", "Main"})
    public void testKeyboardManualAndNotShowAfterJSFocus() throws Throwable {
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        mRule.fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(ImeActivityTestRule.INPUT_VK_API_HTML));
        mRule.assertWaitForKeyboardStatus(false);

        // Don't change the state of the keyboard when policy is manual and
        // script switches focus to another manual element.
        DOMUtils.clickNode(mRule.getWebContents(), "txt3");
        mRule.assertWaitForKeyboardStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"VKGeometryChange", "Main"})
    public void testKeyboardGeometryChangeEventWithInsertText() throws Throwable {
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        mRule.fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(ImeActivityTestRule.INPUT_VK_API_HTML));
        mRule.assertWaitForKeyboardStatus(false);

        // Show keyboard when manual policy element has focus and show API is called.
        DOMUtils.clickNode(mRule.getWebContents(), "txt1");
        mRule.assertWaitForKeyboardStatus(true);

        // Now that the keyboard has been raised, we notify about the bounding rectangle.
        mRule.notifyVirtualKeyboardOverlayRect(0, 63, 1080, 741);
        // Check the VK bounding rectangle and the event getting fired.
        String code = "numEvents";
        String expectedValue = "1";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.x";
        expectedValue = "0";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.y > 0";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.width > 0";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.height > 0";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "new_vv_width === old_vv_width";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "new_vv_height === old_vv_height";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));

        // Hide keyboard when manual policy element has focus and hide API is called.
        code = "navigator.virtualKeyboard.hide()";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);
        // keyboard has hidden.
        // Insert some text while the keyboard geometrychange event is fired for VK hide.
        mRule.commitText("ab", 0);
        mRule.waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        mRule.assertWaitForKeyboardStatus(false);
        mRule.notifyVirtualKeyboardOverlayRect(0, 0, 0, 0);
        mRule.assertWaitForKeyboardStatus(false);

        code = "numEvents";
        expectedValue = "2";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.x";
        expectedValue = "0";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.y";
        expectedValue = "0";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.width";
        expectedValue = "0";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "VKRect.height";
        expectedValue = "0";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "new_vv_width === old_vv_width";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
        code = "new_vv_height === old_vv_height";
        expectedValue = "true";
        Assert.assertEquals(
                expectedValue,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code));
    }
}
