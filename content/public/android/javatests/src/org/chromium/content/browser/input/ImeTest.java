// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Color;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.InputType;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;
import android.text.style.UnderlineSpan;
import android.view.KeyEvent;
import android.view.ViewConfiguration;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ime.TextInputType;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * IME (input method editor) and text input tests.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"expose-internals-for-testing"})
public class ImeTest {
    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();
    @Rule
    public ExpectedException thrown = ExpectedException.none();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_FORM_HTML);
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedWhenNavigating() throws Throwable {
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        mRule.fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(ImeActivityTestRule.INPUT_FORM_HTML));
        mRule.assertWaitForKeyboardStatus(false);

        DOMUtils.clickNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);

        // Hide keyboard when navigating.
        final String code = "document.getElementById(\"link\").click()";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);
        mRule.assertWaitForKeyboardStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        mRule.setComposingText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        mRule.performGo(mRule.getTestCallBackHelperContainer());

        mRule.assertWaitForKeyboardStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testDoesNotHang_getTextAfterKeyboardHides() throws Throwable {
        mRule.setComposingText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        mRule.performGo(mRule.getTestCallBackHelperContainer());

        // This may time out if we do not get the information on time.
        // TODO(changwan): find a way to remove the loop.
        for (int i = 0; i < 100; ++i) {
            mRule.getTextBeforeCursor(10, 0);
        }

        mRule.assertWaitForKeyboardStatus(false);
    }

    // crbug.com/643519
    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCompositionWithNullTextNotCrash() throws Throwable {
        mRule.commitText(null, 1);
        mRule.assertTextsAroundCursor("", null, "");

        mRule.setComposingText(null, 1);
        mRule.assertTextsAroundCursor("", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextWithRangeSelection() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.setSelection(1, 4);
        mRule.waitAndVerifyUpdateSelection(1, 1, 4, -1, -1);

        mRule.deleteSurroundingText(0, 0);
        mRule.assertTextsAroundCursor("h", "ell", "o");

        mRule.deleteSurroundingText(1, 1);
        mRule.assertTextsAroundCursor("", "ell", "");

        mRule.deleteSurroundingText(1, 0);
        mRule.assertTextsAroundCursor("", "ell", "");

        mRule.deleteSurroundingText(0, 1);
        mRule.assertTextsAroundCursor("", "ell", "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextWithCursorSelection() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.setSelection(2, 2);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        mRule.deleteSurroundingText(0, 0);
        mRule.assertTextsAroundCursor("he", null, "llo");

        mRule.deleteSurroundingText(1, 1);
        mRule.assertTextsAroundCursor("h", null, "lo");

        mRule.deleteSurroundingText(1, 0);
        mRule.assertTextsAroundCursor("", null, "lo");

        mRule.deleteSurroundingText(0, 10);
        mRule.assertTextsAroundCursor("", null, "");

        mRule.deleteSurroundingText(10, 10);
        mRule.assertTextsAroundCursor("", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardAppFinishesCompositionOnUnexpectedSelectionChange() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea2");
        mRule.commitText("12345", 1);
        mRule.setSelection(3, 3);
        mRule.setComposingRegion(2, 3);

        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.waitAndVerifyUpdateSelection(1, 3, 3, -1, -1);
        mRule.waitAndVerifyUpdateSelection(2, 3, 3, 2, 3);

        // Unexpected selection change occurs, e.g., the user clicks on an area.
        // There was already one click during test setup; we have to wait out the double-tap
        // timeout or the test will be flaky.
        Thread.sleep(ViewConfiguration.getDoubleTapTimeout());
        DOMUtils.clickNode(mRule.getWebContents(), "textarea2");
        mRule.waitAndVerifyUpdateSelection(3, 5, 5, 2, 3);
        // Keyboard app finishes composition. We emulate this in TestInputMethodManagerWrapper.
        mRule.waitAndVerifyUpdateSelection(4, 5, 5, -1, -1);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextInCodePointsWithRangeSelection() throws Throwable {
        final String trophy = "\uD83C\uDFC6";
        mRule.commitText("ab" + trophy + "cdef" + trophy + "gh", 1);
        mRule.waitAndVerifyUpdateSelection(0, 12, 12, -1, -1);

        mRule.setSelection(6, 8);
        mRule.waitAndVerifyUpdateSelection(1, 6, 8, -1, -1);
        mRule.assertTextsAroundCursor("ab" + trophy + "cd", "ef", trophy + "gh");

        mRule.deleteSurroundingTextInCodePoints(2, 2);
        mRule.waitAndVerifyUpdateSelection(2, 4, 6, -1, -1);
        mRule.assertTextsAroundCursor("ab" + trophy, "ef", "h");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextInCodePointsWithCursorSelection() throws Throwable {
        final String trophy = "\uD83C\uDFC6";
        mRule.commitText("ab" + trophy + "cd" + trophy, 1);
        mRule.waitAndVerifyUpdateSelection(0, 8, 8, -1, -1);

        mRule.setSelection(4, 4);
        mRule.waitAndVerifyUpdateSelection(1, 4, 4, -1, -1);
        mRule.assertTextsAroundCursor("ab" + trophy, null, "cd" + trophy);

        mRule.deleteSurroundingTextInCodePoints(2, 2);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        mRule.assertTextsAroundCursor("a", null, trophy);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingTextForNewCursorPositions() throws Throwable {
        // Cursor is on the right of composing text when newCursorPosition > 0.
        mRule.setComposingText("ab", 1);
        mRule.waitAndVerifyUpdateSelection(0, 2, 2, 0, 2);

        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        // Cursor exceeds the left boundary.
        mRule.setComposingText("cdef", -100);
        mRule.waitAndVerifyUpdateSelection(2, 0, 0, 2, 6);

        // Cursor is on the left boundary.
        mRule.getInputMethodManagerWrapper().expectsSelectionOutsideComposition();
        mRule.setComposingText("cd", -2);
        mRule.waitAndVerifyUpdateSelection(3, 0, 0, 2, 4);

        mRule.getInputMethodManagerWrapper().expectsSelectionOutsideComposition();
        // Cursor is between the left boundary and the composing text.
        mRule.setComposingText("cd", -1);
        mRule.waitAndVerifyUpdateSelection(4, 1, 1, 2, 4);

        // Cursor is on the left of composing text.
        mRule.setComposingText("cd", 0);
        mRule.waitAndVerifyUpdateSelection(5, 2, 2, 2, 4);

        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(6, 2, 2, -1, -1);

        // Cursor is on the right of composing text.
        mRule.setComposingText("ef", 1);
        mRule.waitAndVerifyUpdateSelection(7, 4, 4, 2, 4);

        mRule.getInputMethodManagerWrapper().expectsSelectionOutsideComposition();
        // Cursor is between the composing text and the right boundary.
        mRule.setComposingText("ef", 2);
        mRule.waitAndVerifyUpdateSelection(8, 5, 5, 2, 4);

        mRule.getInputMethodManagerWrapper().expectsSelectionOutsideComposition();
        // Cursor is on the right boundary.
        mRule.setComposingText("ef", 3);
        mRule.waitAndVerifyUpdateSelection(9, 6, 6, 2, 4);

        mRule.getInputMethodManagerWrapper().expectsSelectionOutsideComposition();
        // Cursor exceeds the right boundary.
        mRule.setComposingText("efgh", 100);
        mRule.waitAndVerifyUpdateSelection(10, 8, 8, 2, 6);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitTextForNewCursorPositions() throws Throwable {
        // Cursor is on the left of committing text.
        mRule.commitText("ab", 0);
        mRule.waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);

        // Cursor is on the right of committing text.
        mRule.commitText("cd", 1);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        // Cursor is between the committing text and the right boundary.
        mRule.commitText("ef", 2);
        mRule.waitAndVerifyUpdateSelection(2, 5, 5, -1, -1);

        // Cursor is between the left boundary and the committing text.
        mRule.commitText("gh", -3);
        mRule.waitAndVerifyUpdateSelection(3, 2, 2, -1, -1);

        // Cursor is on the right boundary.
        mRule.commitText("ij", 7);
        mRule.waitAndVerifyUpdateSelection(4, 10, 10, -1, -1);

        // Cursor is on the left boundary.
        mRule.commitText("kl", -10);
        mRule.waitAndVerifyUpdateSelection(5, 0, 0, -1, -1);

        // Cursor exceeds the right boundary.
        mRule.commitText("mn", 100);
        mRule.waitAndVerifyUpdateSelection(6, 14, 14, -1, -1);

        // Cursor exceeds the left boundary.
        mRule.commitText("op", -100);
        mRule.waitAndVerifyUpdateSelection(7, 0, 0, -1, -1);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingTextWithEmptyText() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.setComposingText("AB", 1);
        mRule.waitAndVerifyUpdateSelection(1, 7, 7, 5, 7);

        // With previous composition.
        mRule.setComposingText("", -3);
        mRule.waitAndVerifyUpdateSelection(2, 2, 2, -1, -1);
        mRule.assertTextsAroundCursor("he", null, "llo");

        // Without previous composition.
        mRule.setComposingText("", 3);
        mRule.waitAndVerifyUpdateSelection(3, 4, 4, -1, -1);
        mRule.assertTextsAroundCursor("hell", null, "o");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitTextWithEmptyText() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.setSelection(2, 2);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        mRule.setComposingText("world", 1);
        mRule.waitAndVerifyUpdateSelection(2, 7, 7, 2, 7);
        // With previous composition.
        mRule.commitText("", 2);
        mRule.waitAndVerifyUpdateSelection(3, 3, 3, -1, -1);

        // Without previous composition.
        mRule.commitText("", -1);
        mRule.waitAndVerifyUpdateSelection(4, 2, 2, -1, -1);

        // Although it is not documented in the spec, commitText() also removes existing selection.
        mRule.setSelection(2, 5);
        mRule.commitText("", 1);
        mRule.waitAndVerifyUpdateSelection(5, 2, 5, -1, -1);
        mRule.waitAndVerifyUpdateSelection(6, 2, 2, -1, -1);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitWhileComposingText() throws Throwable {
        mRule.setComposingText("h", 1);
        mRule.waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);

        mRule.setComposingText("he", 1);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, 0, 2);

        mRule.setComposingText("hel", 1);
        mRule.waitAndVerifyUpdateSelection(2, 3, 3, 0, 3);

        mRule.commitText("hel", 1);
        mRule.waitAndVerifyUpdateSelection(3, 3, 3, -1, -1);

        mRule.setComposingText("lo", 1);
        mRule.waitAndVerifyUpdateSelection(4, 5, 5, 3, 5);

        mRule.commitText("", 1);
        mRule.waitAndVerifyUpdateSelection(5, 3, 3, -1, -1);

        mRule.assertTextsAroundCursor("hel", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitEnterKeyWhileComposingText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");

        mRule.setComposingText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        // Cancel the current composition and replace it with enter.
        mRule.commitText("\n", 1);
        mRule.waitAndVerifyUpdateSelection(1, 1, 1, -1, -1);
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        mRule.assertTextsAroundCursor("\n", null, "\n");

        mRule.commitText("world", 1);
        mRule.waitAndVerifyUpdateSelection(2, 6, 6, -1, -1);
        mRule.assertTextsAroundCursor("\nworld", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeCopy() throws Exception {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.setSelection(2, 5);
        mRule.waitAndVerifyUpdateSelection(1, 2, 5, -1, -1);

        mRule.copy();
        mRule.assertClipboardContents(mRule.getActivity(), "llo");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testEnterTextAndRefocus() throws Exception {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.restartInput();
        DOMUtils.clickNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);

        Assert.assertEquals(5, mRule.getConnectionFactory().getOutAttrs().initialSelStart);
        Assert.assertEquals(5, mRule.getConnectionFactory().getOutAttrs().initialSelEnd);
    }

    private static int getImeAction(EditorInfo editorInfo) {
        return editorInfo.imeOptions & EditorInfo.IME_MASK_ACTION;
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testAdvanceFocusNextAndPrevious() throws Exception {
        mRule.focusElement("textarea");
        // Forward direction focus. Excessive focus advance should be ignored.
        for (int i = 0; i < 10; ++i) {
            // Forward direction focus.
            mRule.performEditorAction(EditorInfo.IME_ACTION_NEXT);
        }
        mRule.waitForKeyboardStates(7, 0, 7,
                new Integer[] {TextInputType.TEXT_AREA, TextInputType.TEXT_AREA,
                        TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.CONTENT_EDITABLE,
                        TextInputType.SEARCH, TextInputType.TEXT});
        ArrayList<EditorInfo> editorInfoList =
                mRule.getInputMethodManagerWrapper().getEditorInfoList();
        Assert.assertEquals(7, editorInfoList.size());
        // textarea.
        Assert.assertEquals(EditorInfo.IME_ACTION_NONE, getImeAction(editorInfoList.get(0)));
        // textarea2.
        Assert.assertEquals(EditorInfo.IME_ACTION_NONE, getImeAction(editorInfoList.get(1)));
        // input_number1.
        Assert.assertEquals(EditorInfo.IME_ACTION_NEXT, getImeAction(editorInfoList.get(2)));
        // input_number2.
        Assert.assertEquals(EditorInfo.IME_ACTION_NEXT, getImeAction(editorInfoList.get(3)));
        // content_editable1.
        Assert.assertEquals(EditorInfo.IME_ACTION_NONE, getImeAction(editorInfoList.get(4)));
        // search1.
        Assert.assertEquals(EditorInfo.IME_ACTION_SEARCH, getImeAction(editorInfoList.get(5)));
        // input_text1.
        Assert.assertEquals(EditorInfo.IME_ACTION_GO, getImeAction(editorInfoList.get(6)));

        mRule.resetAllStates();

        // Backward direction focus. Excessive focus advance should be ignored.
        for (int i = 0; i < 10; ++i) {
            // Backward direction focus.
            mRule.performEditorAction(EditorInfo.IME_ACTION_PREVIOUS);
        }
        mRule.waitForKeyboardStates(6, 0, 6,
                new Integer[] {TextInputType.SEARCH, TextInputType.CONTENT_EDITABLE,
                        TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT_AREA,
                        TextInputType.TEXT_AREA});
        editorInfoList = mRule.getInputMethodManagerWrapper().getEditorInfoList();
        Assert.assertEquals(6, editorInfoList.size());
        // search1.
        Assert.assertEquals(EditorInfo.IME_ACTION_SEARCH, getImeAction(editorInfoList.get(0)));
        // content_editable1.
        Assert.assertEquals(EditorInfo.IME_ACTION_NONE, getImeAction(editorInfoList.get(1)));
        // input_number2.
        Assert.assertEquals(EditorInfo.IME_ACTION_NEXT, getImeAction(editorInfoList.get(2)));
        // input_number1.
        Assert.assertEquals(EditorInfo.IME_ACTION_NEXT, getImeAction(editorInfoList.get(3)));
        // textarea2.
        Assert.assertEquals(EditorInfo.IME_ACTION_NONE, getImeAction(editorInfoList.get(4)));
        // textarea.
        Assert.assertEquals(EditorInfo.IME_ACTION_NONE, getImeAction(editorInfoList.get(5)));
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testShowAndHideSoftInput() throws Exception {
        mRule.focusElement("input_radio", false);

        // hideSoftKeyboard(), mRule.restartInput()
        mRule.waitForKeyboardStates(0, 1, 1, new Integer[] {});

        // When input connection is null, we still need to set flags to prevent InputMethodService
        // from entering fullscreen mode and from opening custom UI.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRule.getInputConnection() == null;
            }
        });
        Assert.assertTrue(
                (mRule.getConnectionFactory().getOutAttrs().imeOptions
                        & (EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI))
                != 0);

        // showSoftInput(), mRule.restartInput()
        mRule.focusElement("input_number1");
        mRule.waitForKeyboardStates(1, 1, 2, new Integer[] {TextInputType.NUMBER});
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());

        mRule.focusElement("input_number2");
        // Hide should never be called here. Otherwise we will see a flicker. Restarted to
        // reset internal states to handle the new input form.
        mRule.waitForKeyboardStates(
                2, 1, 3, new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER});

        mRule.focusElement("input_text");
        // showSoftInput() on input_text. mRule.restartInput() on input_number1 due to focus change,
        // and mRule.restartInput() on input_text later.
        mRule.waitForKeyboardStates(3, 1, 4,
                new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT});

        mRule.setComposingText("a", 1);
        mRule.waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        mRule.waitAndVerifyUpdateSelection(1, 1, 1, 0, 1);
        mRule.resetUpdateSelectionList();

        // JavaScript changes focus.
        String code = "(function() { "
                + "var textarea = document.getElementById('textarea');"
                + "textarea.focus();"
                + "})();";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);
        mRule.waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        mRule.resetUpdateSelectionList();

        mRule.waitForKeyboardStates(4, 1, 5,
                new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT,
                        TextInputType.TEXT_AREA});
        Assert.assertEquals(0, mRule.getConnectionFactory().getOutAttrs().initialSelStart);
        Assert.assertEquals(0, mRule.getConnectionFactory().getOutAttrs().initialSelEnd);

        mRule.setComposingText("aa", 1);
        mRule.waitAndVerifyUpdateSelection(0, 2, 2, 0, 2);

        mRule.focusElement("input_text");
        mRule.waitForKeyboardStates(5, 1, 6,
                new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT,
                        TextInputType.TEXT_AREA, TextInputType.TEXT});
        Assert.assertEquals(1, mRule.getConnectionFactory().getOutAttrs().initialSelStart);
        Assert.assertEquals(1, mRule.getConnectionFactory().getOutAttrs().initialSelEnd);

        mRule.focusElement("input_radio", false);
        // hideSoftInput(), mRule.restartInput()
        mRule.waitForKeyboardStates(5, 2, 7,
                new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT,
                        TextInputType.TEXT_AREA, TextInputType.TEXT});
    }
    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testKeyboardNotDismissedAfterCopySelection() throws Exception {
        mRule.commitText("Sample_Text", 1);
        mRule.waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);

        // Select 'text' part.
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");

        mRule.assertWaitForSelectActionBarStatus(true);

        mRule.selectAll();
        mRule.copy();
        mRule.assertClipboardContents(mRule.getActivity(), "Sample_Text");
        Assert.assertEquals(11, mRule.getInputMethodManagerWrapper().getSelection().end());
        mRule.assertWaitForKeyboardStatus(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotDismissedAfterCutSelection() throws Exception {
        mRule.commitText("Sample_Text", 1);
        mRule.waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForSelectActionBarStatus(true);
        mRule.assertWaitForKeyboardStatus(true);
        mRule.cut();
        mRule.assertWaitForKeyboardStatus(true);
        mRule.assertWaitForSelectActionBarStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingEmptyInput() throws Exception {
        DOMUtils.focusNode(mRule.getWebContents(), "input_radio");
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(false);
        mRule.commitText("Sample_Text", 1);
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarShownOnLongPressingInput() throws Exception {
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForSelectActionBarStatus(false);
        mRule.commitText("Sample_Text", 1);
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForSelectActionBarStatus(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testLongPressInputWhileComposingText() throws Exception {
        mRule.assertWaitForSelectActionBarStatus(false);
        mRule.setComposingText("SampleTextThatIsVeryLong Test", 1);
        mRule.waitAndVerifyUpdateSelection(0, 29, 29, 0, 29);
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");

        mRule.assertWaitForSelectActionBarStatus(true);

        // Long press will first change selection region, and then trigger IME app to show up.
        // See RenderFrameImpl::didChangeSelection() and RenderWidget::didHandleGestureEvent().
        mRule.waitAndVerifyUpdateSelection(1, 0, 24, 0, 29);

        // Now IME app wants to finish composing text because an external selection
        // change has been detected. At least Google Latin IME and Samsung IME
        // behave this way.
        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(2, 0, 24, -1, -1);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeShownWhenLongPressOnAlreadySelectedText() throws Exception {
        mRule.assertWaitForSelectActionBarStatus(false);
        mRule.commitText("Sample_Text", 1);

        int showCount = mRule.getInputMethodManagerWrapper().getShowSoftInputCounter();
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForSelectActionBarStatus(true);
        Assert.assertEquals(
                showCount + 1, mRule.getInputMethodManagerWrapper().getShowSoftInputCounter());

        // Now long press again. Selection region remains the same, but the logic
        // should trigger IME to show up. Note that Android does not provide show /
        // hide status of IME, so we will just check whether showIme() has been triggered.
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        final int newCount = showCount + 2;
        CriteriaHelper.pollUiThread(Criteria.equals(newCount, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mRule.getInputMethodManagerWrapper().getShowSoftInputCounter();
            }
        }));
    }

    private void reloadPage() {
        // Reload the page, then focus will be lost and keyboard should be hidden.
        mRule.fullyLoadUrl(mRule.getWebContents().getLastCommittedUrl());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @SuppressWarnings("TryFailThrowable") // TODO(tedchoc): Remove after fixing timeout.
    public void testPhysicalKeyboard_AttachDetach() throws Throwable {
        mRule.attachPhysicalKeyboard();
        // We still call showSoftKeyboard, which will be ignored by physical keyboard.
        mRule.waitForKeyboardStates(1, 0, 1, new Integer[] {TextInputType.TEXT});
        mRule.setComposingText("a", 1);
        mRule.waitForKeyboardStates(1, 0, 1, new Integer[] {TextInputType.TEXT});
        mRule.detachPhysicalKeyboard();
        mRule.assertWaitForKeyboardStatus(true);
        // Now we really show soft keyboard. We also call mRule.restartInput when configuration
        // changes.
        mRule.waitForKeyboardStates(
                2, 0, 2, new Integer[] {TextInputType.TEXT, TextInputType.TEXT});

        reloadPage();

        // Depending on the timing, hideSoftInput and mRule.restartInput call counts may vary here
        // because render widget gets restarted. But the end result should be the same.
        mRule.assertWaitForKeyboardStatus(false);

        mRule.detachPhysicalKeyboard();

        // We should not show soft keyboard here because focus has been lost.
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRule.getInputMethodManagerWrapper().isShowWithoutHideOutstanding();
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingInput() throws Exception {
        mRule.commitText("Sample_Text", 1);
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);
        mRule.assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForSelectActionBarStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingOutsideInput() throws Exception {
        mRule.commitText("Sample_Text", 1);
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);
        mRule.assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(mRule.getWebContents(), "plain_text");
        mRule.assertWaitForKeyboardStatus(false);
        mRule.assertWaitForSelectActionBarStatus(false);

        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);
        mRule.assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(mRule.getWebContents(), "input_radio");
        mRule.assertWaitForKeyboardStatus(false);
        mRule.assertWaitForSelectActionBarStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingDifferentEmptyInputs() throws Exception {
        DOMUtils.focusNode(mRule.getWebContents(), "input_radio");
        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(false);
        DOMUtils.longPressNode(mRule.getWebContents(), "textarea");
        mRule.assertWaitForKeyboardStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeStaysOnLongPressingDifferentNonEmptyInputs() throws Exception {
        DOMUtils.focusNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);

        mRule.commitText("Sample_Text", 1);
        // We should wait to avoid race condition.
        mRule.waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);

        DOMUtils.focusNode(mRule.getWebContents(), "textarea");
        mRule.waitAndVerifyUpdateSelection(1, 0, 0, -1, -1);

        mRule.commitText("Sample_Text", 1);
        mRule.waitAndVerifyUpdateSelection(2, 11, 11, -1, -1);

        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);
        mRule.assertWaitForSelectActionBarStatus(true);

        DOMUtils.longPressNode(mRule.getWebContents(), "textarea");
        mRule.assertWaitForKeyboardStatus(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        mRule.commitText("snarful", 1);
        mRule.waitAndVerifyUpdateSelection(0, 7, 7, -1, -1);

        mRule.setSelection(1, 5);
        mRule.waitAndVerifyUpdateSelection(1, 1, 5, -1, -1);

        mRule.cut();
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        mRule.assertTextsAroundCursor("s", null, "ul");
        mRule.assertClipboardContents(mRule.getActivity(), "narf");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImePaste() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClipboardManager clipboardManager =
                    (ClipboardManager) mRule.getActivity().getSystemService(
                            Context.CLIPBOARD_SERVICE);
            clipboardManager.setPrimaryClip(ClipData.newPlainText("blarg", "blarg"));
        });

        mRule.paste();
        // Paste is a two step process when there is a non-zero selection.
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.assertTextsAroundCursor("blarg", null, "");

        mRule.setSelection(3, 5);
        mRule.waitAndVerifyUpdateSelection(1, 3, 5, -1, -1);
        mRule.assertTextsAroundCursor("bla", "rg", "");

        mRule.paste();
        // Paste is a two step process when there is a non-zero selection.
        mRule.waitAndVerifyUpdateSelection(2, 8, 8, -1, -1);
        mRule.assertTextsAroundCursor("blablarg", null, "");

        mRule.paste();
        mRule.waitAndVerifyUpdateSelection(3, 13, 13, -1, -1);
        mRule.assertTextsAroundCursor("blablargblarg", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndCollapseSelection() throws Exception {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.selectAll();
        mRule.waitAndVerifyUpdateSelection(1, 0, 5, -1, -1);

        mRule.collapseSelection();
        mRule.waitAndVerifyUpdateSelection(2, 5, 5, -1, -1);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testShowVirtualKeyboardIfEnabled() throws Throwable {
        // ShowVirtualKeyboardIfEnabled() is now implicitly called by the updated focus
        // heuristic so no need to call explicitly. http://crbug.com/371927
        DOMUtils.focusNode(mRule.getWebContents(), "input_radio");
        mRule.assertWaitForKeyboardStatus(false);

        DOMUtils.focusNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForKeyboardStatus(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testFinishComposingText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");

        mRule.commitText("hllo", 1);
        mRule.waitAndVerifyUpdateSelection(0, 4, 4, -1, -1);

        mRule.commitText(" ", 1);
        mRule.waitAndVerifyUpdateSelection(1, 5, 5, -1, -1);

        mRule.setSelection(1, 1);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        mRule.assertTextsAroundCursor("h", null, "llo ");

        mRule.setComposingRegion(0, 4);
        mRule.waitAndVerifyUpdateSelection(3, 1, 1, 0, 4);

        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(4, 1, 1, -1, -1);

        mRule.commitText("\n", 1);
        mRule.waitAndVerifyUpdateSelection(5, 2, 2, -1, -1);
        mRule.assertTextsAroundCursor("h\n", null, "llo ");
    }

    // http://crbug.com/445499
    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteText() throws Throwable {
        mRule.focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Andr
        // when the noted key is touched on screen.
        // H
        mRule.setComposingText("h", 1);
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));

        // A
        mRule.setComposingText("ha", 1);
        Assert.assertEquals("ha", mRule.getTextBeforeCursor(9, 0));

        mRule.setComposingText("h", 1);
        mRule.setComposingRegion(0, 1);
        mRule.setComposingText("h", 1);
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));

        // I
        mRule.setComposingText("hi", 1);
        Assert.assertEquals("hi", mRule.getTextBeforeCursor(9, 0));

        // SPACE
        mRule.commitText("hi", 1);
        mRule.commitText(" ", 1);
        Assert.assertEquals("hi ", mRule.getTextBeforeCursor(9, 0));

        // DEL
        mRule.deleteSurroundingText(1, 0);
        mRule.setComposingRegion(0, 2);
        Assert.assertEquals("hi", mRule.getTextBeforeCursor(9, 0));

        mRule.setComposingText("h", 1);
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));

        mRule.commitText("", 1);
        Assert.assertEquals("", mRule.getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        mRule.deleteSurroundingText(1, 0); // DEL on empty still sends 1,0
        Assert.assertEquals("", mRule.getTextBeforeCursor(9, 0));
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSwipingText() throws Throwable {
        mRule.focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Android 4.4) sends
        // when the word is swiped on the soft keyboard.  Exercise care when altering to make sure
        // that the test reflects reality.  If this test breaks, it's possible that code has
        // changed and different calls need to be made instead.
        // "three"
        mRule.setComposingText("three", 1);
        Assert.assertEquals("three", mRule.getTextBeforeCursor(99, 0));

        // "word"
        mRule.commitText("three", 1);
        mRule.commitText(" ", 1);
        mRule.setComposingText("word", 1);
        Assert.assertEquals("three word", mRule.getTextBeforeCursor(99, 0));

        // "test"
        mRule.commitText("word", 1);
        mRule.commitText(" ", 1);
        mRule.setComposingText("test", 1);
        Assert.assertEquals("three word test", mRule.getTextBeforeCursor(99, 0));
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteMultiCharacterCodepoint() throws Throwable {
        // This smiley is a multi character codepoint.
        final String smiley = "\uD83D\uDE0A";

        mRule.commitText(smiley, 1);
        mRule.waitAndVerifyUpdateSelection(0, 2, 2, -1, -1);
        mRule.assertTextsAroundCursor(smiley, null, "");

        // DEL, sent via mRule.dispatchKeyEvent like it is in Android WebView or a physical
        // keyboard.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        mRule.waitAndVerifyUpdateSelection(1, 0, 0, -1, -1);

        // Make sure that we accept further typing after deleting the smiley.
        mRule.setComposingText("s", 1);
        mRule.setComposingText("sm", 1);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, 0, 1);
        mRule.waitAndVerifyUpdateSelection(3, 2, 2, 0, 2);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testBackspaceKeycode() throws Throwable {
        mRule.focusElement("textarea");

        // H
        mRule.commitText("h", 1);
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));

        // A
        mRule.commitText("a", 1);
        Assert.assertEquals("ha", mRule.getTextBeforeCursor(9, 0));

        // DEL, sent via mRule.dispatchKeyEvent like it is in Android WebView or a physical
        // keyboard.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testRepeatBackspaceKeycode() throws Throwable {
        mRule.focusElement("textarea");

        // H
        mRule.commitText("h", 1);
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));

        // A
        mRule.commitText("a", 1);
        Assert.assertEquals("ha", mRule.getTextBeforeCursor(9, 0));

        // Multiple keydowns should each delete one character (this is for physical keyboard
        // key-repeat).
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        Assert.assertEquals("", mRule.getTextBeforeCursor(9, 0));
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");

        // Type 'a' using a physical keyboard.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A));
        mRule.waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        // Type 'enter' key.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        mRule.assertTextsAroundCursor("a\n", null, "\n");

        // Type 'b'.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        mRule.waitAndVerifyUpdateSelection(2, 3, 3, -1, -1);
        mRule.assertTextsAroundCursor("a\nb", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard_AccentKeyCodes() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");
        int index = 0;

        // h
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 1, 1, -1, -1);

        // ALT-i  (circumflex accent key on virtual keyboard). Accent should not appear until the
        // next letter is entered.
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("h", mRule.getTextBeforeCursor(9, 0));

        // finishComposingText() should not prevent the accent from being joined.
        mRule.finishComposingText();

        // o
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_O));
        Assert.assertEquals("hô", mRule.getTextBeforeCursor(9, 0));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_O));
        Assert.assertEquals("hô", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 2, 2, -1, -1);

        // o again. Should not have accent mark this time.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_O));
        Assert.assertEquals("hôo", mRule.getTextBeforeCursor(9, 0));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_O));
        Assert.assertEquals("hôo", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 3, 3, -1, -1);

        // ALT-i. Should not display anything until the next key is pressed.
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("hôo", mRule.getTextBeforeCursor(9, 0));

        // ALT-i again should commit the caret this time.
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("hôoˆ", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 4, 4, -1, -1);

        // b (cannot be accented, should just appear after)
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        Assert.assertEquals("hôoˆb", mRule.getTextBeforeCursor(9, 0));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        Assert.assertEquals("hôoˆb", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 5, 5, -1, -1);

        // ALT-i. Should not display anything.
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("hôoˆb", mRule.getTextBeforeCursor(9, 0));

        // Backspace. Should delete the b even though we have a pending accent.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        Assert.assertEquals("hôoˆ", mRule.getTextBeforeCursor(9, 0));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));
        Assert.assertEquals("hôoˆ", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 4, 4, -1, -1);

        // Alt-i. Should not display anything (the pending accent should have been cleared by the
        // backspace).
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("hôoˆ", mRule.getTextBeforeCursor(9, 0));

        // Space. Should display the pending accent.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SPACE));
        Assert.assertEquals("hôoˆˆ", mRule.getTextBeforeCursor(9, 0));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SPACE));
        Assert.assertEquals("hôoˆˆ", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 5, 5, -1, -1);

        // Alt-i. Should not display anything but should set a circumflex as the pending accent.
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("hôoˆˆ", mRule.getTextBeforeCursor(9, 0));

        // Alt-e. Should output the circumflex and set an acute accent as the pending accent.
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_E, 0, KeyEvent.META_ALT_ON));
        mRule.dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_E, 0, KeyEvent.META_ALT_ON));
        Assert.assertEquals("hôoˆˆˆ", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 6, 6, -1, -1);

        // e. Should output an e with an acute accent.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_E));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_E));
        Assert.assertEquals("hôoˆˆˆé", mRule.getTextBeforeCursor(9, 0));
        mRule.waitAndVerifyUpdateSelection(index++, 7, 7, -1, -1);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingRegionOutOfBounds() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");
        mRule.setComposingText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        mRule.setComposingRegion(0, 0);
        mRule.waitAndVerifyUpdateSelection(1, 5, 5, -1, -1);
        mRule.setComposingRegion(0, 9);
        mRule.waitAndVerifyUpdateSelection(2, 5, 5, 0, 5);
        mRule.setComposingRegion(9, 1);
        mRule.waitAndVerifyUpdateSelection(3, 5, 5, 1, 5);
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_AfterCommitText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");

        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        mRule.waitAndVerifyUpdateSelection(1, 6, 6, -1, -1);
        mRule.assertTextsAroundCursor("hello\n", null, "\n");

        mRule.commitText("world", 1);
        mRule.waitAndVerifyUpdateSelection(2, 11, 11, -1, -1);
        mRule.assertTextsAroundCursor("hello\nworld", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_WhileComposingText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");

        mRule.setComposingText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        // IME app should call this, otherwise enter key should clear the current composition.
        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(1, 5, 5, -1, -1);

        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));

        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        mRule.waitAndVerifyUpdateSelection(2, 6, 6, -1, -1);

        mRule.commitText("world", 1);
        mRule.waitAndVerifyUpdateSelection(3, 11, 11, -1, -1);
        mRule.assertTextsAroundCursor("hello\nworld", null, "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDpadKeyCodesWhileSwipingText() throws Throwable {
        int showCount = mRule.getInputMethodManagerWrapper().getShowSoftInputCounter();
        mRule.focusElement("textarea");

        // focusElement() calls showSoftInput().
        CriteriaHelper.pollUiThread(Criteria.equals(showCount + 1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mRule.getInputMethodManagerWrapper().getShowSoftInputCounter();
            }
        }));

        // DPAD_CENTER should cause keyboard to appear on keyup.
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER));

        // Should not have called showSoftInput() on keydown.
        CriteriaHelper.pollUiThread(Criteria.equals(showCount + 1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mRule.getInputMethodManagerWrapper().getShowSoftInputCounter();
            }
        }));

        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER));

        // Should have called showSoftInput() on keyup.
        CriteriaHelper.pollUiThread(Criteria.equals(showCount + 2, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mRule.getInputMethodManagerWrapper().getShowSoftInputCounter();
            }
        }));
    }

    @Test
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testNavigateTextWithDpadKeyCodes() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("textarea");

        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT));
        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_LEFT));

        mRule.assertTextsAroundCursor("hell", null, "o");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupShowAndHide() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        mRule.selectAll();
        mRule.waitAndVerifyUpdateSelection(1, 0, 5, -1, -1);
        mRule.assertTextsAroundCursor("", "hello", "");

        mRule.cut();
        mRule.waitAndVerifyUpdateSelection(2, 0, 0, -1, -1);
        mRule.assertTextsAroundCursor("", null, "");

        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRule.getSelectionPopupController().isPastePopupShowing()
                        && mRule.getSelectionPopupController().isInsertionForTesting();
            }
        });

        mRule.setComposingText("h", 1);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mRule.getSelectionPopupController().isPastePopupShowing();
            }
        });
        Assert.assertFalse(mRule.getSelectionPopupController().isInsertionForTesting());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectionClearedOnKeyEvent() throws Throwable {
        mRule.commitText("Sample_Text", 1);
        mRule.waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);

        DOMUtils.longPressNode(mRule.getWebContents(), "input_text");
        mRule.assertWaitForSelectActionBarStatus(true);

        mRule.setComposingText("h", 1);
        mRule.assertWaitForSelectActionBarStatus(false);
        Assert.assertFalse(mRule.getSelectionPopupController().hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testTextHandlesPreservedWithDpadNavigation() throws Throwable {
        DOMUtils.longPressNode(mRule.getWebContents(), "plain_text");
        mRule.assertWaitForSelectActionBarStatus(true);
        Assert.assertTrue(mRule.getSelectionPopupController().hasSelection());

        mRule.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN));
        mRule.assertWaitForSelectActionBarStatus(true);
        Assert.assertTrue(mRule.getSelectionPopupController().hasSelection());
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testRestartInputWhileComposingText() throws Throwable {
        mRule.setComposingText("abc", 1);
        mRule.waitAndVerifyUpdateSelection(0, 3, 3, 0, 3);
        mRule.restartInput();
        // We don't do anything when input gets restarted. But we depend on Android's
        // InputMethodManager and/or input methods to call mRule.finishComposingText() in setting
        // current input connection as active or finishing the current input connection.
        Thread.sleep(1000);
        Assert.assertEquals(
                1, mRule.getInputMethodManagerWrapper().getUpdateSelectionList().size());
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testRestartInputKeepsTextAndCursor() throws Exception {
        mRule.commitText("ab", 2);
        mRule.restartInput();
        Assert.assertEquals("ab", mRule.getTextBeforeCursor(10, 0));
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_ComposingText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("contenteditable_event");
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.setComposingText("a", 1);
        mRule.waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);
        mRule.waitForEventLogs(
                "keydown(229),compositionstart(),compositionupdate(a),input(a),keyup(229),"
                + "selectionchange");
        mRule.clearEventLogs();

        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(1, 1, 1, -1, -1);
        mRule.waitForEventLogs("compositionend(a)");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_ComposingText() throws Throwable {
        mRule.setComposingText("a", 1);
        mRule.waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);
        mRule.waitForEventLogs("keydown(229),compositionstart(),compositionupdate(a),"
                + "input(a),keyup(229),selectionchange");
        mRule.clearEventLogs();

        mRule.finishComposingText();
        mRule.waitAndVerifyUpdateSelection(1, 1, 1, -1, -1);
        mRule.waitForEventLogs("compositionend(a)");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_CommitText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("contenteditable_event");
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.commitText("a", 1);
        mRule.waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        mRule.waitForEventLogs("keydown(229),input(a),keyup(229),selectionchange");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_CommitText() throws Throwable {
        mRule.commitText("a", 1);
        mRule.waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        mRule.waitForEventLogs("keydown(229),input(a),keyup(229),selectionchange");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_DeleteSurroundingText() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("contenteditable_event");
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.waitForEventLogs("keydown(229),input(hello),keyup(229),selectionchange");
        mRule.clearEventLogs();

        mRule.setSelection(2, 2);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.deleteSurroundingText(1, 1);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);

        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        mRule.waitForEventLogs(
                "keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_DeleteSurroundingText() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.waitForEventLogs("keydown(229),input(hello),keyup(229),selectionchange");
        mRule.clearEventLogs();

        mRule.setSelection(2, 2);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.deleteSurroundingText(1, 1);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        mRule.waitForEventLogs(
                "keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_DeleteSurroundingTextInCodePoints() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("contenteditable_event");
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.waitForEventLogs("keydown(229),input(hello),keyup(229),selectionchange");
        mRule.clearEventLogs();

        mRule.setSelection(2, 2);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.deleteSurroundingTextInCodePoints(1, 1);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        mRule.waitForEventLogs(
                "keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_DeleteSurroundingTextInCodePoints() throws Throwable {
        mRule.commitText("hello", 1);
        mRule.waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        mRule.waitForEventLogs("keydown(229),input(hello),keyup(229),selectionchange");
        mRule.clearEventLogs();

        mRule.setSelection(2, 2);
        mRule.waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();

        mRule.deleteSurroundingTextInCodePoints(1, 1);
        mRule.waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        mRule.waitForEventLogs(
                "keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testGetCursorCapsMode() throws Throwable {
        mRule.focusElementAndWaitForStateUpdate("contenteditable_event");
        mRule.commitText("Hello World", 1);
        mRule.waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);
        Assert.assertEquals(0, mRule.getCursorCapsMode(InputType.TYPE_TEXT_FLAG_CAP_WORDS));
        mRule.setSelection(6, 6);
        mRule.waitAndVerifyUpdateSelection(1, 6, 6, -1, -1);
        Assert.assertEquals(InputType.TYPE_TEXT_FLAG_CAP_WORDS,
                mRule.getCursorCapsMode(InputType.TYPE_TEXT_FLAG_CAP_WORDS));
        mRule.commitText("\n", 1);
        Assert.assertEquals(InputType.TYPE_TEXT_FLAG_CAP_WORDS,
                mRule.getCursorCapsMode(InputType.TYPE_TEXT_FLAG_CAP_WORDS));
    }

    // https://crbug.com/604675
    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testAlertInKeyUpListenerDoesNotCrash() throws Exception {
        // Call 'alert()' when 'keyup' event occurs. Since we are in contentshell,
        // this does not actually pops up the alert window.
        String code = "(function() { "
                + "var editor = document.getElementById('input_text');"
                + "editor.addEventListener('keyup', function(e) { alert('keyup') });"
                + "})();";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);
        mRule.setComposingText("ab", 1);
        mRule.finishComposingText();
        Assert.assertEquals("ab", mRule.getTextBeforeCursor(10, 0));
    }

    // https://crbug.com/616334
    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testCastToBaseInputConnection() throws Exception {
        mRule.commitText("a", 1);
        final BaseInputConnection baseInputConnection = (BaseInputConnection) mRule.getConnection();
        Assert.assertEquals("a", mRule.runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return baseInputConnection.getTextBeforeCursor(10, 0);
            }
        }));
    }

    // Tests that the method call order is kept.
    // See crbug.com/601707 for details.
    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testSetSelectionCommitTextOrder() throws Exception {
        final ChromiumBaseInputConnection connection = mRule.getConnection();
        mRule.runBlockingOnImeThread(new Callable<Void>() {
            @Override
            public Void call() {
                connection.beginBatchEdit();
                connection.commitText("hello world", 1);
                connection.setSelection(6, 6);
                connection.deleteSurroundingText(0, 5);
                connection.commitText("'", 1);
                connection.commitText("world", 1);
                connection.setSelection(7, 7);
                connection.setComposingText("", 1);
                connection.endBatchEdit();
                return null;
            }
        });
        mRule.waitAndVerifyUpdateSelection(0, 7, 7, -1, -1);
    }

    // crbug.com/643477
    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testUiThreadAccess() throws Exception {
        final ChromiumBaseInputConnection connection = mRule.getConnection();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // We allow UI thread access for most functions, except for
            // beginBatchEdit(), endBatchEdit(), and get* methods().
            Assert.assertTrue(connection.commitText("a", 1));
            Assert.assertTrue(connection.setComposingText("b", 1));
            Assert.assertTrue(connection.setComposingText("bc", 1));
            Assert.assertTrue(connection.finishComposingText());
        });
        Assert.assertEquals("abc", mRule.runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return connection.getTextBeforeCursor(5, 0);
            }
        }));
    }

    @Test
    @MediumTest
    @Feature({"TextInput"})
    public void testBackgroundAndUnderlineSpans() throws Throwable {
        mRule.fullyLoadUrl("data:text/html, <div contenteditable id=\"div\" />");

        WebContents webContents = mRule.getWebContents();
        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("hello world");
        BackgroundColorSpan backgroundColorSpan = new BackgroundColorSpan(Color.MAGENTA);
        UnderlineSpan underlineSpan = new UnderlineSpan();
        textToCommit.setSpan(backgroundColorSpan, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        textToCommit.setSpan(underlineSpan, 6, 11, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        mRule.commitText(textToCommit, 1);

        // Wait for renderer to acknowledge commitText(). ImeActivityTestRule.commitText() blocks
        // and waits for the IME thread to finish, but the communication between the IME thread and
        // the renderer is asynchronous, so if we try to run JavaScript right away, the text won't
        // necessarily have been committed yet.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(webContents, "div").equals("hello world");
                } catch (TimeoutException e) {
                    return false;
                }
            }
        });

        Assert.assertEquals("2",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "internals.markerCountForNode("
                                + "  document.getElementById('div').firstChild, "
                                + "  'composition')"));

        // Colors come back as ARGB.
        Assert.assertEquals(0xFFFF00FFL,
                (long) Double.parseDouble(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                "internals.markerBackgroundColorForNode("
                                        + "  document.getElementById('div').firstChild, "
                                        + "  'composition', 0)")));

        Assert.assertEquals(0x0000000L,
                (long) Double.parseDouble(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                "internals.markerBackgroundColorForNode("
                                        + "  document.getElementById('div').firstChild, "
                                        + "  'composition', 1)")));

        Assert.assertEquals(0x00000000L,
                (long) Double.parseDouble(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                "internals.markerUnderlineColorForNode("
                                        + "  document.getElementById('div').firstChild, "
                                        + "  'composition', 0)")));

        Assert.assertEquals(0x00000000L,
                (long) Double.parseDouble(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                "internals.markerUnderlineColorForNode("
                                        + "  document.getElementById('div').firstChild, "
                                        + "  'composition', 1)")));

        Assert.assertEquals("0",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "internals.markerRangeForNode("
                                + "  document.getElementById('div').firstChild, "
                                + "  'composition', 0).startOffset"));

        Assert.assertEquals("5",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "internals.markerRangeForNode("
                                + "  document.getElementById('div').firstChild, "
                                + "  'composition', 0).endOffset"));

        Assert.assertEquals("6",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "internals.markerRangeForNode("
                                + "  document.getElementById('div').firstChild, "
                                + "  'composition', 1).startOffset"));

        Assert.assertEquals("11",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "internals.markerRangeForNode("
                                + "  document.getElementById('div').firstChild, "
                                + "  'composition', 1).endOffset"));
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testAutocorrectAttribute() throws Exception {
        // Autocorrect should be on for a text field that doesn't have an autocorrect attribute.
        mRule.focusElement("input_text");
        Assert.assertEquals(EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);

        // Autocorrect should be on for a text field that has autocorrect="on" set.
        mRule.focusElement("autocorrect_on");
        Assert.assertEquals(EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);

        // Autocorrect should be off for a text field that has autocorrect="off" set.
        mRule.focusElement("autocorrect_off");
        Assert.assertEquals(0,
                mRule.getConnectionFactory().getOutAttrs().inputType
                        & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);
    }
}
