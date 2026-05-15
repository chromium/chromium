// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Handles calling back to C++ for text suggestions. */
@JNINamespace("content")
@NullMarked
public class TextSuggestionHost {
    private long mNativePtr;
    private final Runnable mOnDismissCallback;

    public TextSuggestionHost(long nativePtr, Runnable onDismissCallback) {
        mNativePtr = nativePtr;
        mOnDismissCallback = onDismissCallback;
    }

    public void destroy() {
        mNativePtr = 0;
    }

    /** Tells Blink to replace the active suggestion range with the specified replacement. */
    public void applySpellCheckSuggestion(String suggestion) {
        if (mNativePtr == 0) return;
        TextSuggestionHostJni.get().applySpellCheckSuggestion(mNativePtr, suggestion);
    }

    /**
     * Tells Blink to replace the active suggestion range with the specified suggestion on the
     * specified marker.
     */
    public void applyTextSuggestion(int markerTag, int suggestionIndex) {
        if (mNativePtr == 0) return;
        TextSuggestionHostJni.get().applyTextSuggestion(mNativePtr, markerTag, suggestionIndex);
    }

    /** Tells Blink to delete the active suggestion range. */
    public void deleteActiveSuggestionRange() {
        if (mNativePtr == 0) return;
        TextSuggestionHostJni.get().deleteActiveSuggestionRange(mNativePtr);
    }

    /** Tells Blink to remove spelling markers under all instances of the specified word. */
    public void onNewWordAddedToDictionary(@Nullable String word) {
        if (mNativePtr == 0) return;
        TextSuggestionHostJni.get().onNewWordAddedToDictionary(mNativePtr, word);
    }

    /**
     * Tells Blink the suggestion menu was closed (and also clears the reference to the
     * SuggestionsPopupWindow instance so it can be garbage collected).
     */
    public void onSuggestionMenuClosed(boolean dismissedByItemTap) {
        if (!dismissedByItemTap && mNativePtr != 0) {
            TextSuggestionHostJni.get().onSuggestionMenuClosed(mNativePtr);
        }
        mOnDismissCallback.run();
    }

    @NativeMethods
    interface Natives {
        void applySpellCheckSuggestion(long nativeTextSuggestionHostAndroid, String suggestion);

        void applyTextSuggestion(
                long nativeTextSuggestionHostAndroid, int markerTag, int suggestionIndex);

        void deleteActiveSuggestionRange(long nativeTextSuggestionHostAndroid);

        void onNewWordAddedToDictionary(
                long nativeTextSuggestionHostAndroid, @Nullable String word);

        void onSuggestionMenuClosed(long nativeTextSuggestionHostAndroid);
    }
}
