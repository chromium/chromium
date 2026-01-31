// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Typeface;
import android.text.TextUtils;
import android.text.Spanned;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;
import android.view.inputmethod.InputConnection;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.content_public.browser.ContentFeatureList;

/** Unit tests for {@TextInputState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextInputStateTest {
    @Test
    @Feature({"TextInput"})
    @EnableFeatures({ContentFeatureList.ACCESSIBILITY_IME_GET_FORMATTED_TEXT})
    public void testEmptySelection() {
        int flagsDefault = 0;
        int flagsWithStyles = InputConnection.GET_TEXT_WITH_STYLES;
        TextInputState stateEmptySelectionString =
                new TextInputState("hello", new Range(3, 3), new Range(-1, -1), false, true);
        TextInputState stateEmptySelectionSpanned =
                new TextInputState(createSpannedText("hello", 1, 4), new Range(3, 3),
                    new Range(-1, -1), false, true);

        // Test with text from String and style flag set to 0.
        verifyTextAfterSelection(stateEmptySelectionString, flagsDefault, true);
        verifyTextBeforeSelection(stateEmptySelectionString, flagsDefault, true);
        assertEquals(null, stateEmptySelectionString.getSelectedText(flagsDefault));

        // Test with text from SpannableStringBuilder and style flag set to 0.
        verifyTextAfterSelection(stateEmptySelectionSpanned, flagsDefault, true);
        verifyTextBeforeSelection(stateEmptySelectionSpanned, flagsDefault, true);
        assertEquals(null, stateEmptySelectionSpanned.getSelectedText(flagsDefault));

        // Test with text from String and style flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        verifyTextAfterSelection(stateEmptySelectionString, flagsWithStyles, true);
        verifyTextBeforeSelection(stateEmptySelectionString, flagsWithStyles, true);
        assertEquals(null, stateEmptySelectionString.getSelectedText(flagsWithStyles));

        CharSequence textAfterSelectionString =
                stateEmptySelectionString.getTextAfterSelection(1, flagsWithStyles);
        int expectedSpanCount = 0;
        int expectedSpanStart = 0;
        int expectedSpanEnd = textAfterSelectionString.length();
        verifySpansInSpannedText(
                textAfterSelectionString, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        CharSequence textBeforeSelectionString =
                stateEmptySelectionString.getTextBeforeSelection(1, flagsWithStyles);
        expectedSpanCount = 0;
        expectedSpanStart = 0;
        expectedSpanEnd = textBeforeSelectionString.length();
        verifySpansInSpannedText(
                textBeforeSelectionString, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        // Test with text from SpannableStringBuilder and style flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        verifyTextAfterSelection(stateEmptySelectionSpanned, flagsWithStyles, true);
        verifyTextBeforeSelection(stateEmptySelectionSpanned, flagsWithStyles, true);
        assertEquals(null, stateEmptySelectionSpanned.getSelectedText(flagsWithStyles));

        CharSequence textAfterSelectionSpanned =
                stateEmptySelectionSpanned.getTextAfterSelection(1, flagsWithStyles);
        expectedSpanCount = 1;
        expectedSpanStart = 0;
        expectedSpanEnd = textAfterSelectionSpanned.length();
        verifySpansInSpannedText(
                textAfterSelectionSpanned, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        CharSequence textBeforeSelectionSpanned =
                stateEmptySelectionSpanned.getTextBeforeSelection(3, flagsWithStyles);
        expectedSpanCount = 1;
        expectedSpanStart = 1;
        expectedSpanEnd = textBeforeSelectionSpanned.length();
        verifySpansInSpannedText(
                textBeforeSelectionSpanned, expectedSpanStart, expectedSpanEnd, expectedSpanCount);
    }

    @Test
    @Feature({"TextInput"})
    @EnableFeatures({ContentFeatureList.ACCESSIBILITY_IME_GET_FORMATTED_TEXT})
    public void testNonEmptySelection() {
        int flagsDefault = 0;
        int flagsWithStyles = InputConnection.GET_TEXT_WITH_STYLES;
        TextInputState stateNonEmptySelectionString =
                new TextInputState("hello", new Range(3, 4), new Range(3, 4), false, true);
        TextInputState stateNonEmptySelectionSpanned =
                new TextInputState(createSpannedText("hello", 1, 5), new Range(3, 4),
                    new Range(3, 4), false, true);

        // Test with text from String and style flag set to 0.
        verifyTextAfterSelection(stateNonEmptySelectionString, flagsDefault, false);
        verifyTextBeforeSelection(stateNonEmptySelectionString, flagsDefault, false);
        assertEquals("l", stateNonEmptySelectionString.getSelectedText(flagsDefault).toString());
        assertTrue(stateNonEmptySelectionString.getSelectedText(flagsDefault) instanceof String);

        // Test with text from SpannableStringBuilder and style flag set to 0.
        verifyTextAfterSelection(stateNonEmptySelectionSpanned, flagsDefault, false);
        verifyTextBeforeSelection(stateNonEmptySelectionSpanned, flagsDefault, false);
        assertEquals("l", stateNonEmptySelectionSpanned.getSelectedText(flagsDefault));
        assertTrue(
                stateNonEmptySelectionSpanned.getSelectedText(flagsDefault) instanceof String);

        // Test with text from String and style flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        verifyTextAfterSelection(stateNonEmptySelectionString, flagsWithStyles, false);
        verifyTextBeforeSelection(stateNonEmptySelectionString, flagsWithStyles, false);
        assertEquals(
                "l", stateNonEmptySelectionString.getSelectedText(flagsWithStyles).toString());

        CharSequence textAfterSelectionString =
                stateNonEmptySelectionString.getTextAfterSelection(1, flagsWithStyles);
        int expectedSpanCount = 0;
        int expectedSpanStart = 0;
        int expectedSpanEnd = textAfterSelectionString.length();
        verifySpansInSpannedText(
                textAfterSelectionString, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        CharSequence textBeforeSelectionString =
                stateNonEmptySelectionString.getTextBeforeSelection(1, flagsWithStyles);
        expectedSpanCount = 0;
        expectedSpanStart = 0;
        expectedSpanEnd = textBeforeSelectionString.length();
        verifySpansInSpannedText(
                textBeforeSelectionString, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        CharSequence selectedTextString =
                stateNonEmptySelectionString.getSelectedText(flagsWithStyles);
        expectedSpanCount = 0;
        expectedSpanStart = 0;
        expectedSpanEnd = selectedTextString.length();
        verifySpansInSpannedText(selectedTextString, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        // Test with text from SpannableStringBuilder and style flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        verifyTextAfterSelection(stateNonEmptySelectionSpanned, flagsWithStyles, false);
        verifyTextBeforeSelection(stateNonEmptySelectionSpanned, flagsWithStyles, false);
        assertEquals("l",
                stateNonEmptySelectionSpanned.getSelectedText(flagsWithStyles).toString());

        CharSequence textAfterSelectionSpanned =
                stateNonEmptySelectionSpanned.getTextAfterSelection(1, flagsWithStyles);
        expectedSpanCount = 1;
        expectedSpanStart = 0;
        expectedSpanEnd = textAfterSelectionSpanned.length();
        verifySpansInSpannedText(
                textAfterSelectionSpanned, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        CharSequence textBeforeSelectionSpanned =
                stateNonEmptySelectionSpanned.getTextBeforeSelection(3, flagsWithStyles);
        expectedSpanCount = 1;
        expectedSpanStart = 1;
        expectedSpanEnd = textBeforeSelectionSpanned.length();
        verifySpansInSpannedText(
                textBeforeSelectionSpanned, expectedSpanStart, expectedSpanEnd, expectedSpanCount);

        CharSequence selectedTextSpanned =
                stateNonEmptySelectionSpanned.getSelectedText(flagsWithStyles);
        expectedSpanCount = 1;
        expectedSpanStart = 0;
        expectedSpanEnd = selectedTextSpanned.length();
        verifySpansInSpannedText(
                selectedTextSpanned, expectedSpanStart, expectedSpanEnd, expectedSpanCount);
    }

    @Test
    @Feature({"TextInput"})
    @EnableFeatures({ContentFeatureList.ACCESSIBILITY_IME_GET_FORMATTED_TEXT})
    public void textGetSurroundingText() {
        String stringText = "hello";
        SpannableStringBuilder spannedText = createSpannedText("The quick brown fox", 3, 6);
        int flagsDefault = 0;
        int flagsWithStyles = InputConnection.GET_TEXT_WITH_STYLES;

        // Test empty selection with String and styling flag set to 0.
        TextInputState stateEmptySelectionString =
                new TextInputState(stringText, new Range(3, 3), new Range(-1, -1), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateEmptySelectionString, flagsDefault);

        // Test non-empty selection with String and styling flag set to 0.
        TextInputState stateNonEmptySelectionString =
                new TextInputState(stringText, new Range(3, 4), new Range(3, 4), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateNonEmptySelectionString, flagsDefault);

        // Test empty selection with SpannableStringBuilder and styling flag set to 0.
        TextInputState stateEmptySelectionSpanned =
                new TextInputState(spannedText, new Range(3, 3), new Range(-1, -1), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateEmptySelectionSpanned, flagsDefault);

        // Test non-empty selection with SpannableStringBuilder and styling flag set to 0.
        TextInputState stateNonEmptySelectionSpanned =
                new TextInputState(spannedText, new Range(3, 4), new Range(3, 4), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateNonEmptySelectionSpanned, flagsDefault);

        // Test empty selection with String and styling flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.

        TextInputState stateEmptySelectionStringWithStyles =
                new TextInputState(stringText, new Range(3, 3), new Range(-1, -1), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateEmptySelectionStringWithStyles, flagsWithStyles);

        // Test non-empty selection with String and styling flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        TextInputState stateNonEmptySelectionStringWithStyles =
                new TextInputState(stringText, new Range(3, 4), new Range(3, 4), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateNonEmptySelectionStringWithStyles, flagsWithStyles);

        // Test empty selection with SpannableStringBuilder and styling flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        TextInputState stateEmptySelectionSpannedWithStyles =
                new TextInputState(spannedText, new Range(3, 3), new Range(-1, -1), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateEmptySelectionSpannedWithStyles, flagsWithStyles);

        // Test non-empty selection with SpannableStringBuilder and styling flag set to
        // InputConnection.GET_TEXT_WITH_STYLES.
        TextInputState stateNonEmptySelectionSpannedWithStyles =
                new TextInputState(spannedText, new Range(3, 4), new Range(3, 4), false, true);
        bulkVerifySurroundingTextWithDynamicRange(stateNonEmptySelectionSpannedWithStyles, flagsWithStyles);

        // Verify that the spans are included in the resulting text when styling flag is set to
        // InputConnection.GET_TEXT_WITH_STYLES.

        SpannableStringBuilder spannedTextWithStyles = createSpannedText("The quick brown fox", 8, 11);
        TextInputState stateSpannedWithStyles =
                new TextInputState(spannedTextWithStyles, new Range(7, 7), new Range(-1, -1), false, true);
        CharSequence surroundingTextWithStyles = stateSpannedWithStyles.getSurroundingTextInternal(
                3, 5, InputConnection.GET_TEXT_WITH_STYLES).mText;
        int expectedSpanCount = 1;
        int expectedSpanStart = 4;
        int expectedSpanEnd = 7;
        verifySpansInSpannedText(surroundingTextWithStyles, expectedSpanStart, expectedSpanEnd, expectedSpanCount);
    }

    void bulkVerifySurroundingTextWithDynamicRange(TextInputState inputState, int flags) {
        for (int before = -1; before < 6; ++before) {
            for (int after = -1; after < 6; ++after) {
                int beforeLength = before == 5 ? Integer.MAX_VALUE : before;
                int afterLength = after == 5 ? Integer.MAX_VALUE : after;
                int offset = Math.max(0, inputState.selection().start()
                        - Math.max(0, beforeLength));
                boolean isInputStateSpanned = inputState.text() instanceof Spanned;
                verifySurroundingText(
                        getSurroundingTextFrameworkDefaultVersion(
                                inputState, beforeLength, afterLength),
                        inputState.getSurroundingTextInternal(
                                beforeLength, afterLength, flags),
                        offset, flags, isInputStateSpanned);
            }
        }
    }

    void verifyTextAfterSelection(TextInputState inputState, int flags, boolean isEmptySelection) {
        String[] expectedTextAfterSelection;
        int[] expectedTextAfterSelectionLength;
        if (isEmptySelection) {
            expectedTextAfterSelection = new String[] {"lo", "lo", "lo", "", ""};
            expectedTextAfterSelectionLength = new int[] {Integer.MAX_VALUE, 3, 2, 0, -1};
        } else {
            expectedTextAfterSelection = new String[] {"o", "o", "o", "", ""};
            expectedTextAfterSelectionLength = new int[] {Integer.MAX_VALUE, 2, 1, 0, -1};
        }

        for (int i = 0; i < expectedTextAfterSelection.length; i++) {
            CharSequence textAfterSelection =
                    inputState.getTextAfterSelection(expectedTextAfterSelectionLength[i], flags);
            assertEquals(expectedTextAfterSelection[i], textAfterSelection.toString());
            if (flags == InputConnection.GET_TEXT_WITH_STYLES) {
                assertTrue(textAfterSelection instanceof Spanned);
            } else {
                assertTrue(textAfterSelection instanceof String);
            }
        }
    }

    void verifyTextBeforeSelection(TextInputState inputState, int flags, boolean isEmptySelection) {
        String[] expectedTextBeforeSelection;
        int[] expectedTextBeforeSelectionLength;
        if (isEmptySelection) {
            expectedTextBeforeSelection = new String[] {"hel", "hel", "el", "", ""};
            expectedTextBeforeSelectionLength = new int[] {Integer.MAX_VALUE, 3, 2, 0, -1};
        } else {
            expectedTextBeforeSelection = new String[] {"hel", "hel", "hel", "", ""};
            expectedTextBeforeSelectionLength = new int[] {Integer.MAX_VALUE, 4, 3, 0, -1};
        }

        for (int i = 0; i < expectedTextBeforeSelection.length; i++) {
            CharSequence textBeforeSelection =
                    inputState.getTextBeforeSelection(expectedTextBeforeSelectionLength[i], flags);
            assertEquals(expectedTextBeforeSelection[i], textBeforeSelection.toString());
            if (flags == InputConnection.GET_TEXT_WITH_STYLES) {
                assertTrue(textBeforeSelection instanceof Spanned);
            } else {
                assertTrue(textBeforeSelection instanceof String);
            }
        }
    }

    void verifySurroundingText(
            TextInputState.SurroundingTextInternal expected,
            TextInputState.SurroundingTextInternal value, int offset, int flags,
            boolean isInputStateSpanned) {
        assertEquals(expected.mText.toString(), value.mText.toString());
        assertEquals(expected.mSelectionStart, value.mSelectionStart);
        assertEquals(expected.mSelectionEnd, value.mSelectionEnd);
        assertEquals(offset, value.mOffset);
        if (flags == InputConnection.GET_TEXT_WITH_STYLES || isInputStateSpanned) {
            assertTrue(value.mText instanceof Spanned);
        } else {
            assertTrue(value.mText instanceof String);
        }
    }

    void verifySpansInSpannedText(
            CharSequence text, int expectedSpanStart, int expectedSpanEnd, int expectedSpanLength) {
        assertTrue(text instanceof Spanned);
        Spanned spannedText = (Spanned) text;
        StyleSpan[] spans = spannedText.getSpans(0, spannedText.length(), StyleSpan.class);
        assertEquals(expectedSpanLength, spans.length);
        if (expectedSpanLength > 0) {
            assertEquals(Typeface.BOLD, spans[0].getStyle());
            assertEquals(expectedSpanStart, spannedText.getSpanStart(spans[0]));
            assertEquals(expectedSpanEnd, spannedText.getSpanEnd(spans[0]));
        }
    }

    SpannableStringBuilder createSpannedText(CharSequence text, int start, int end) {
        SpannableStringBuilder builder = new SpannableStringBuilder();
        builder.append(text);
        builder.setSpan(
                new StyleSpan(Typeface.BOLD), start, end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        return builder;
    }

    // From Android framework code InputConnection#getSurroundingtext(int, int,
    // int). Our implementation should match the default implementation behavior.
    // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/view/inputmethod/InputConnection.java;l=320
    private TextInputState.SurroundingTextInternal getSurroundingTextFrameworkDefaultVersion(
            TextInputState inputState, int beforeLength, int afterLength) {
        CharSequence textBeforeCursor = inputState.getTextBeforeSelection(beforeLength, 0);
        if (textBeforeCursor == null) {
            return null;
        }

        CharSequence textAfterCursor = inputState.getTextAfterSelection(afterLength, 0);
        if (textAfterCursor == null) {
            return null;
        }

        CharSequence selectedText = inputState.getSelectedText(0);
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
