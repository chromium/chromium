// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import androidx.test.filters.MediumTest;

import org.junit.After;
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

/**
 * IME (input method editor) and text input tests for VK policy and show/hide APIs.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-blink-features=VirtualKeyboard", "expose-internals-for-testing"})
@Batch(ImeTest.IME_BATCH)
public class ImeInputVKApiTest {
    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();
    @Rule
    public ExpectedException thrown = ExpectedException.none();

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
}
