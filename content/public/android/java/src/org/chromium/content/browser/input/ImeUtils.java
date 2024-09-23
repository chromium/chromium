// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.util.StringBuilderPrinter;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.EditorInfo;

import androidx.core.view.inputmethod.EditorInfoCompat;

import org.chromium.base.ThreadUtils;
import org.chromium.blink_public.web.WebTextInputFlags;
import org.chromium.blink_public.web.WebTextInputMode;
import org.chromium.ui.base.ime.TextInputAction;
import org.chromium.ui.base.ime.TextInputType;

import java.util.Locale;

/** Utilities for IME such as computing outAttrs, and dumping object information. */
public class ImeUtils {
    /**
     * Compute {@link EditorInfo} based on the given parameters. This is needed for
     * {@link View#onCreateInputConnection(EditorInfo)}.
     *
     * @param inputType Type defined in {@link TextInputType}.
     * @param inputFlags Flags defined in {@link WebTextInputFlags}.
     * @param inputMode Flags defined in {@link WebTextInputMode}.
     * @param inputAction Flags defined in {@link TextInputAction}.
     * @param initialSelStart The initial selection start position.
     * @param initialSelEnd The initial selection end position.
     * @param outAttrs An instance of {@link EditorInfo} that we are going to change.
     */
    public static void computeEditorInfo(
            int inputType,
            int inputFlags,
            int inputMode,
            int inputAction,
            int initialSelStart,
            int initialSelEnd,
            String lastText,
            EditorInfo outAttrs) {
        outAttrs.inputType =
                EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;

        if ((inputFlags & WebTextInputFlags.AUTOCOMPLETE_OFF) != 0) {
            outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        }

        if (inputMode == WebTextInputMode.DEFAULT) {
            if (inputType == TextInputType.TEXT) {
                // Normal text field
                if ((inputFlags & WebTextInputFlags.AUTOCORRECT_OFF) == 0) {
                    outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT;
                }
            } else if (inputType == TextInputType.TEXT_AREA
                    || inputType == TextInputType.CONTENT_EDITABLE) {
                outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE;
                if ((inputFlags & WebTextInputFlags.AUTOCORRECT_OFF) == 0) {
                    outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT;
                }
            } else if (inputType == TextInputType.PASSWORD) {
                outAttrs.inputType =
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD;
            } else if (inputType == TextInputType.URL) {
                outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
            } else if (inputType == TextInputType.EMAIL) {
                // Email
                outAttrs.inputType =
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
            } else if (inputType == TextInputType.TELEPHONE) {
                // Telephone Number and telephone do not have both a Tab key and an action in
                // default OSK, so set the action to NEXT
                outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
            } else if (inputType == TextInputType.NUMBER) {
                // Number
                outAttrs.inputType =
                        InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL;
            }
        } else {
            switch (inputMode) {
                case WebTextInputMode.TEL:
                    outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
                    break;
                case WebTextInputMode.URL:
                    outAttrs.inputType =
                            InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
                    break;
                case WebTextInputMode.EMAIL:
                    outAttrs.inputType =
                            InputType.TYPE_CLASS_TEXT
                                    | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
                    break;
                case WebTextInputMode.NUMERIC:
                    outAttrs.inputType = InputType.TYPE_CLASS_NUMBER;
                    if (inputType == TextInputType.PASSWORD
                            || (inputFlags & WebTextInputFlags.HAS_BEEN_PASSWORD_FIELD) != 0) {
                        outAttrs.inputType |= InputType.TYPE_NUMBER_VARIATION_PASSWORD;
                    }
                    break;
                case WebTextInputMode.DECIMAL:
                    outAttrs.inputType =
                            InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL;
                    break;
                case WebTextInputMode.DEFAULT:
                case WebTextInputMode.TEXT:
                case WebTextInputMode.SEARCH:
                default:
                    outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE;
                    if ((inputFlags & WebTextInputFlags.AUTOCORRECT_OFF) == 0) {
                        outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT;
                    }
                    break;
            }
        }

        outAttrs.imeOptions |=
                getImeAction(
                        inputType,
                        inputFlags,
                        inputMode,
                        inputAction,
                        (outAttrs.inputType & EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE) != 0);

        // Handling of autocapitalize. Blink will send the flag taking into account the element's
        // type. This is not using AutocapitalizeNone because Android does not autocapitalize by
        // default and there is no way to express no capitalization.
        // Autocapitalize is meant as a hint to the virtual keyboard.
        if ((inputFlags & WebTextInputFlags.AUTOCAPITALIZE_CHARACTERS) != 0) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS;
        } else if ((inputFlags & WebTextInputFlags.AUTOCAPITALIZE_WORDS) != 0) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_WORDS;
        } else if ((inputFlags & WebTextInputFlags.AUTOCAPITALIZE_SENTENCES) != 0) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
        }

        if ((inputFlags & WebTextInputFlags.HAS_BEEN_PASSWORD_FIELD) != 0
                && (outAttrs.inputType & InputType.TYPE_NUMBER_VARIATION_PASSWORD) == 0) {
            // When input password is visible to user, Blink will send inputType as
            // TextInputType.TEXT, When it is changed back to non-visible password,
            // inputType is TextInputType.PASSWORD.
            if (inputType == TextInputType.TEXT) {
                outAttrs.inputType =
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
            } else if (inputType == TextInputType.PASSWORD) {
                outAttrs.inputType =
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD;
            }
        }

        outAttrs.initialSelStart = initialSelStart;
        outAttrs.initialSelEnd = initialSelEnd;
        // Note: Android's internal implementation trims the text up to 2048 chars before
        // sending it to the IMM service. In the future, if we consider limiting the number of
        // chars between renderer and browser, then consider calling
        // setInitialSurroundingSubText() instead.
        EditorInfoCompat.setInitialSurroundingText(outAttrs, lastText);
    }

    private static int getImeAction(
            int inputType,
            int inputFlags,
            int inputMode,
            int inputAction,
            boolean isMultiLineInput) {
        int imeAction = 0;
        if (inputAction == TextInputAction.DEFAULT) {
            if (inputMode == WebTextInputMode.DEFAULT && inputType == TextInputType.SEARCH) {
                imeAction |= EditorInfo.IME_ACTION_SEARCH;
            } else if (isMultiLineInput) {
                // For textarea that sends you to another webpage on enter key press using
                // JavaScript, we will only show ENTER.
                imeAction |= EditorInfo.IME_ACTION_NONE;
            } else if ((inputFlags & WebTextInputFlags.HAVE_NEXT_FOCUSABLE_ELEMENT) != 0) {
                imeAction |= EditorInfo.IME_ACTION_NEXT;
            } else {
                // For last element inside form, we should give preference to GO key as PREVIOUS
                // has less importance in those cases.
                imeAction |= EditorInfo.IME_ACTION_GO;
            }
        } else {
            switch (inputAction) {
                case TextInputAction.ENTER:
                    imeAction |= EditorInfo.IME_ACTION_NONE;
                    break;
                case TextInputAction.GO:
                    imeAction |= EditorInfo.IME_ACTION_GO;
                    break;
                case TextInputAction.DONE:
                    imeAction |= EditorInfo.IME_ACTION_DONE;
                    break;
                case TextInputAction.NEXT:
                    imeAction |= EditorInfo.IME_ACTION_NEXT;
                    break;
                case TextInputAction.PREVIOUS:
                    imeAction |= EditorInfo.IME_ACTION_PREVIOUS;
                    break;
                case TextInputAction.SEARCH:
                    imeAction |= EditorInfo.IME_ACTION_SEARCH;
                    break;
                case TextInputAction.SEND:
                    imeAction |= EditorInfo.IME_ACTION_SEND;
                    break;
            }
        }
        return imeAction;
    }

    /**
     * @param editorInfo The EditorInfo
     * @return Debug string for the given {@EditorInfo}.
     */
    static String getEditorInfoDebugString(EditorInfo editorInfo) {
        StringBuilder builder = new StringBuilder();
        StringBuilderPrinter printer = new StringBuilderPrinter(builder);
        editorInfo.dump(printer, "");
        return builder.toString();
    }

    /**
     * @param editable The editable.
     * @return Debug string for the given {@Editable}.
     */
    static String getEditableDebugString(Editable editable) {
        return String.format(
                Locale.US,
                "Editable {[%s] SEL[%d %d] COM[%d %d]}",
                editable.toString(),
                Selection.getSelectionStart(editable),
                Selection.getSelectionEnd(editable),
                BaseInputConnection.getComposingSpanStart(editable),
                BaseInputConnection.getComposingSpanEnd(editable));
    }

    /**
     * @param correctionInfo The correction info.
     * @return Debug string for the given {@CorrectionInfo}.
     */
    static String getCorrectionInfoDebugString(CorrectionInfo correctionInfo) {
        // TODO(changwan): implement it properly if needed.
        return correctionInfo.toString();
    }

    /**
     * Check the given condition and raise an error if it is false.
     * @param condition The condition to check.
     */
    static void checkCondition(boolean condition) {
        if (!condition) throw new AssertionError();
    }

    /**
     * Check the given condition and raise an error if it is false.
     * @param msg A message to show when raising an error.
     * @param condition The condition to check.
     */
    static void checkCondition(String msg, boolean condition) {
        if (!condition) throw new AssertionError(msg);
    }

    /** Check that the current thread is UI thread, and raise an error if it is not. */
    static void checkOnUiThread() {
        checkCondition("Should be on UI thread.", ThreadUtils.runningOnUiThread());
    }
}
