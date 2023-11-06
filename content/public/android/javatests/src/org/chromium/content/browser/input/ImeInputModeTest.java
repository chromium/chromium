// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.text.InputType;
import android.view.inputmethod.EditorInfo;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.web.WebTextInputMode;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.ui.base.ime.TextInputType;

/** IME (input method editor) and text input tests for input-mode attribute. */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(ImeTest.IME_BATCH)
public class ImeInputModeTest {
    @Rule public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_MODE_HTML);
    }

    @After
    public void tearDown() throws Exception {
        mRule.getActivity().finish();
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testNumericPassword() throws Exception {
        mRule.focusElement("input_numeric_password", /* shouldShowKeyboard= */ true);

        mRule.waitForKeyboardStates(
                1,
                0,
                1,
                new Integer[] {TextInputType.PASSWORD},
                new Integer[] {WebTextInputMode.NUMERIC});
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());

        Assert.assertTrue(
                (mRule.getConnectionFactory().getOutAttrs().inputType
                                & (InputType.TYPE_CLASS_NUMBER
                                        | InputType.TYPE_NUMBER_VARIATION_PASSWORD))
                        != 0);
    }

    @DisabledTest(message = "crbug.com/1463785")
    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testShowAndHideInputMode() throws Exception {
        mRule.focusElement("contenteditable_none", false);

        // hideSoftKeyboard()
        mRule.waitForKeyboardStates(0, 1, 0, new Integer[] {}, new Integer[] {});
        Assert.assertNotNull(mRule.getInputConnection());

        Assert.assertTrue(
                (mRule.getConnectionFactory().getOutAttrs().imeOptions
                                & (EditorInfo.IME_FLAG_NO_FULLSCREEN
                                        | EditorInfo.IME_FLAG_NO_EXTRACT_UI))
                        != 0);

        // showSoftInput(), mRule.restartInput()
        mRule.focusElement("contenteditable_text");
        mRule.waitForKeyboardStates(
                1,
                1,
                1,
                new Integer[] {TextInputType.CONTENT_EDITABLE},
                new Integer[] {WebTextInputMode.TEXT});
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());

        mRule.focusElement("contenteditable_tel");
        // Hide should never be called here. Otherwise we will see a flicker. Restarted to
        // reset internal states to handle the new input form.
        mRule.waitForKeyboardStates(
                2,
                1,
                2,
                new Integer[] {TextInputType.CONTENT_EDITABLE, TextInputType.CONTENT_EDITABLE},
                new Integer[] {WebTextInputMode.TEXT, WebTextInputMode.TEL});

        mRule.focusElement("contenteditable_url");
        mRule.waitForKeyboardStates(
                3,
                1,
                3,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {WebTextInputMode.TEXT, WebTextInputMode.TEL, WebTextInputMode.URL});

        mRule.focusElement("contenteditable_email");
        mRule.waitForKeyboardStates(
                4,
                1,
                4,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.TEXT,
                    WebTextInputMode.TEL,
                    WebTextInputMode.URL,
                    WebTextInputMode.EMAIL
                });

        mRule.focusElement("contenteditable_numeric");
        mRule.waitForKeyboardStates(
                5,
                1,
                5,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.TEXT,
                    WebTextInputMode.TEL,
                    WebTextInputMode.URL,
                    WebTextInputMode.EMAIL,
                    WebTextInputMode.NUMERIC
                });

        mRule.focusElement("contenteditable_decimal");
        mRule.waitForKeyboardStates(
                6,
                1,
                6,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.TEXT,
                    WebTextInputMode.TEL,
                    WebTextInputMode.URL,
                    WebTextInputMode.EMAIL,
                    WebTextInputMode.NUMERIC,
                    WebTextInputMode.DECIMAL
                });

        mRule.focusElement("contenteditable_search");
        mRule.waitForKeyboardStates(
                7,
                1,
                7,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.TEXT,
                    WebTextInputMode.TEL,
                    WebTextInputMode.URL,
                    WebTextInputMode.EMAIL,
                    WebTextInputMode.NUMERIC,
                    WebTextInputMode.DECIMAL,
                    WebTextInputMode.SEARCH
                });

        mRule.focusElement("contenteditable_none", false);
        mRule.waitForKeyboardStates(
                7,
                2,
                7,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.TEXT,
                    WebTextInputMode.TEL,
                    WebTextInputMode.URL,
                    WebTextInputMode.EMAIL,
                    WebTextInputMode.NUMERIC,
                    WebTextInputMode.DECIMAL,
                    WebTextInputMode.SEARCH
                });
    }

    @DisabledTest(message = "crbug.com/1463785")
    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testShowAndHideInputModeWithPhysicalKeyboard() throws Throwable {
        // First show should hide the keyboard.
        mRule.focusElement("contenteditable_none", false);

        // hideSoftKeyboard()
        mRule.waitForKeyboardStates(0, 1, 0, new Integer[] {}, new Integer[] {});
        Assert.assertNotNull(mRule.getInputConnection());

        // Now with the physical keyboard attached it should cause a show (because of the
        // IME composition window).
        mRule.attachPhysicalKeyboard();

        // showSoftKeyboard(), mRule.restartInput()
        mRule.waitForKeyboardStates(
                1,
                1,
                1,
                new Integer[] {TextInputType.CONTENT_EDITABLE},
                new Integer[] {WebTextInputMode.NONE});
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());

        // Move focus to another input, cause it to do a show there.
        mRule.focusElement("contenteditable_text");
        mRule.waitForKeyboardStates(
                2,
                1,
                2,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE, TextInputType.CONTENT_EDITABLE,
                },
                new Integer[] {WebTextInputMode.NONE, WebTextInputMode.TEXT});
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());

        // Focusing the none content editable should show the keyboard when there is one
        // physically attached.
        mRule.focusElement("contenteditable_none");

        // mRule.restartInput()
        mRule.waitForKeyboardStates(
                2,
                1,
                3,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.NONE, WebTextInputMode.TEXT, WebTextInputMode.NONE
                });
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());

        mRule.detachPhysicalKeyboard();

        // mRule.restartInput(), hideSoftKeyboard()
        mRule.waitForKeyboardStates(
                2,
                2,
                4,
                new Integer[] {
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE,
                    TextInputType.CONTENT_EDITABLE
                },
                new Integer[] {
                    WebTextInputMode.NONE,
                    WebTextInputMode.TEXT,
                    WebTextInputMode.NONE,
                    WebTextInputMode.NONE
                });
        Assert.assertNotNull(mRule.getInputConnection());
    }
}
