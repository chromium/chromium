// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.text.InputType;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/** IME (input method editor) and text input tests for password fields. */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"expose-internals-for-testing"})
@Batch(ImeTest.IME_BATCH)
public class ImePasswordTest {
    @Rule public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.PASSWORD_FORM_HTML);
    }

    @After
    public void tearDown() throws Exception {
        mRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardOnPasswordFieldChangingType() throws Throwable {
        mRule.focusElement("input_text");
        Assert.assertNotEquals(
                InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);

        // <input type="password"> should be considered a password field.
        mRule.focusElement("input_password");
        Assert.assertEquals(
                InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);

        // Change input_password to type text and remove focus.
        final String code =
                "document.getElementById(\"input_password\").type = \"text\";"
                        + " document.getElementById(\"input_password\").blur();";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);

        // <input type="password"> that was changed to type="text" should be considered
        // as visible password field.
        mRule.focusElement("input_password");
        Assert.assertEquals(
                InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);

        // Temporarily focus input_text and verify that it is not a password input.
        mRule.focusElement("input_text");
        Assert.assertNotEquals(
                InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);

        // Return to input_password and verify that it is still considered a
        // visible password input despite having input="text" now.
        mRule.focusElement("input_password");
        Assert.assertEquals(
                InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        Assert.assertEquals(
                "\"text\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mRule.getWebContents(), "document.activeElement.type"));
    }
}
