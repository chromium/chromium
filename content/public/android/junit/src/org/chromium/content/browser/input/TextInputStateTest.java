// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@TextInputState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextInputStateTest {
    @Test
    @Feature({"TextInput"})
    public void testEmptySelection() {
        TextInputState state =
                new TextInputState("hello", new Range(3, 3), new Range(-1, -1), false, true);
        assertEquals("lo", state.getTextAfterSelection(Integer.MAX_VALUE));
        assertEquals("lo", state.getTextAfterSelection(3));
        assertEquals("lo", state.getTextAfterSelection(2));
        assertEquals("", state.getTextAfterSelection(0));
        assertEquals("", state.getTextAfterSelection(-1));
        assertEquals("hel", state.getTextBeforeSelection(Integer.MAX_VALUE));
        assertEquals("hel", state.getTextBeforeSelection(3));
        assertEquals("el", state.getTextBeforeSelection(2));
        assertEquals("", state.getTextBeforeSelection(0));
        assertEquals("", state.getTextBeforeSelection(-1));
        assertEquals(null, state.getSelectedText());
    }

    @Test
    @Feature({"TextInput"})
    public void testNonEmptySelection() {
        TextInputState state =
                new TextInputState("hello", new Range(3, 4), new Range(3, 4), false, true);
        assertEquals("hel", state.getTextBeforeSelection(Integer.MAX_VALUE));
        assertEquals("hel", state.getTextBeforeSelection(4));
        assertEquals("hel", state.getTextBeforeSelection(3));
        assertEquals("", state.getTextBeforeSelection(0));
        assertEquals("", state.getTextBeforeSelection(-1));
        assertEquals("o", state.getTextAfterSelection(Integer.MAX_VALUE));
        assertEquals("o", state.getTextAfterSelection(2));
        assertEquals("o", state.getTextAfterSelection(1));
        assertEquals("", state.getTextAfterSelection(0));
        assertEquals("", state.getTextAfterSelection(-1));
        assertEquals("l", state.getSelectedText());
    }

    @Test
    @Feature({"TextInput"})
    public void textGetSurroundingText() {
        TextInputState stateEmptySelection =
                new TextInputState("hello", new Range(3, 3), new Range(-1, -1), false, true);
        for (int before = -1; before < 6; ++before) {
            for (int after = -1; after < 6; ++after) {
                int beforeLength = before == 5 ? before : Integer.MAX_VALUE;
                int afterLength = after == 5 ? after : Integer.MAX_VALUE;
                verifySurroundingText(
                        getSurroundingTextFrameworkDefaultVersion(
                                stateEmptySelection, beforeLength, afterLength),
                        stateEmptySelection.getSurroundingTextInternal(beforeLength, afterLength));
            }
        }

        TextInputState stateNonEmptySelection =
                new TextInputState("hello", new Range(3, 4), new Range(3, 4), false, true);
        for (int before = -1; before < 6; ++before) {
            for (int after = -1; after < 6; ++after) {
                int beforeLength = before == 5 ? before : Integer.MAX_VALUE;
                int afterLength = after == 5 ? after : Integer.MAX_VALUE;
                verifySurroundingText(
                        getSurroundingTextFrameworkDefaultVersion(
                                stateNonEmptySelection, beforeLength, afterLength),
                        stateNonEmptySelection.getSurroundingTextInternal(
                                beforeLength, afterLength));
            }
        }
    }

    void verifySurroundingText(
            TextInputState.SurroundingTextInternal expected,
            TextInputState.SurroundingTextInternal value) {
        assertEquals(expected.mText, value.mText);
        assertEquals(expected.mSelectionStart, value.mSelectionStart);
        assertEquals(expected.mSelectionEnd, value.mSelectionEnd);
    }

    // From Android framework code InputConnection#getSurroundingtext(int, int, int). Our
    // implementation should match the default implementation behavior.
    // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/view/inputmethod/InputConnection.java;l=320
    private TextInputState.SurroundingTextInternal getSurroundingTextFrameworkDefaultVersion(
            TextInputState inputState, int beforeLength, int afterLength) {
        CharSequence textBeforeCursor = inputState.getTextBeforeSelection(beforeLength);
        if (textBeforeCursor == null) {
            return null;
        }

        CharSequence textAfterCursor = inputState.getTextAfterSelection(afterLength);
        if (textAfterCursor == null) {
            return null;
        }

        CharSequence selectedText = inputState.getSelectedText();
        if (selectedText == null) {
            selectedText = "";
        }
        CharSequence surroundingText =
                TextUtils.concat(textBeforeCursor, selectedText, textAfterCursor);
        return new TextInputState.SurroundingTextInternal(
                surroundingText,
                textBeforeCursor.length(),
                textBeforeCursor.length() + selectedText.length(),
                -1);
    }
}
