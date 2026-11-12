// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Handles calling back to C++ for text suggestions. One instance exists per native
 * TextSuggestionHostAndroid and owns its {@link JniRawPtr} until {@link #destroy()} is called by
 * the native destructor or {@link TextSuggestionPopupController#destroy()}.
 */
@JNINamespace("content")
@NullMarked
public class TextSuggestionHost {
    @JniType("::content::TextSuggestionHostAndroid")
    interface NativeTextSuggestionHostAndroid extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeTextSuggestionHostAndroid> mNativePtr;
    private final Runnable mOnDismissCallback;

    TextSuggestionHost(
            JniRawPtr<NativeTextSuggestionHostAndroid> nativePtr, Runnable onDismissCallback) {
        mNativePtr = nativePtr;
        mOnDismissCallback = onDismissCallback;
    }

    @CalledByNative
    public void destroy() {
        if (mNativePtr != null) {
            mNativePtr.release();
            mNativePtr = null;
        }
    }

    @Nullable JniRawPtr<NativeTextSuggestionHostAndroid> getNativePtrForTesting() {
        return mNativePtr;
    }

    /** Tells Blink to replace the active suggestion range with the specified replacement. */
    public void applySpellCheckSuggestion(String suggestion) {
        if (mNativePtr == null) return;
        TextSuggestionHostJni.get().applySpellCheckSuggestion(mNativePtr, suggestion);
    }

    /**
     * Tells Blink to replace the active suggestion range with the specified suggestion on the
     * specified marker.
     */
    public void applyTextSuggestion(int markerTag, int suggestionIndex) {
        if (mNativePtr == null) return;
        TextSuggestionHostJni.get().applyTextSuggestion(mNativePtr, markerTag, suggestionIndex);
    }

    /** Tells Blink to delete the active suggestion range. */
    public void deleteActiveSuggestionRange() {
        if (mNativePtr == null) return;
        TextSuggestionHostJni.get().deleteActiveSuggestionRange(mNativePtr);
    }

    /** Tells Blink to remove spelling markers under all instances of the specified word. */
    public void onNewWordAddedToDictionary(@Nullable String word) {
        if (mNativePtr == null) return;
        TextSuggestionHostJni.get().onNewWordAddedToDictionary(mNativePtr, word);
    }

    /**
     * Tells Blink the suggestion menu was closed (and also clears the reference to the
     * SuggestionsPopupWindow instance so it can be garbage collected).
     */
    public void onSuggestionMenuClosed(boolean dismissedByItemTap) {
        if (!dismissedByItemTap && mNativePtr != null) {
            TextSuggestionHostJni.get().onSuggestionMenuClosed(mNativePtr);
        }
        mOnDismissCallback.run();
    }

    @NativeMethods
    interface Natives {
        void applySpellCheckSuggestion(
                JniPtr<NativeTextSuggestionHostAndroid> self, String suggestion);

        void applyTextSuggestion(
                JniPtr<NativeTextSuggestionHostAndroid> self, int markerTag, int suggestionIndex);

        void deleteActiveSuggestionRange(JniPtr<NativeTextSuggestionHostAndroid> self);

        void onNewWordAddedToDictionary(
                JniPtr<NativeTextSuggestionHostAndroid> self, @Nullable String word);

        void onSuggestionMenuClosed(JniPtr<NativeTextSuggestionHostAndroid> self);
    }
}
