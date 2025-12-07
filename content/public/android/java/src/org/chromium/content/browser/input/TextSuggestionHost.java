// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.PopupController;
import org.chromium.content.browser.PopupController.HideablePopup;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles displaying the Android spellcheck/text suggestion menu (provided by
 * SuggestionsPopupWindow) when requested by the C++ class TextSuggestionHostAndroid and applying
 * the commands in that menu (by calling back to the C++ class).
 */
@JNINamespace("content")
@NullMarked
public class TextSuggestionHost implements WindowEventObserver, HideablePopup, UserData {
    private long mNativeTextSuggestionHost;
    private final WebContentsImpl mWebContents;
    private final Context mContext;
    private final ViewAndroidDelegate mViewDelegate;

    private boolean mIsAttachedToWindow;
    private @Nullable WindowAndroid mWindowAndroid;

    private @Nullable SpellCheckPopupWindow mSpellCheckPopupWindow;
    private @Nullable TextSuggestionsPopupWindow mTextSuggestionsPopupWindow;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<TextSuggestionHost> INSTANCE = TextSuggestionHost::new;
    }

    /**
     * Get {@link TextSuggestionHost} object used for the give WebContents. {@link #create()} should
     * precede any calls to this.
     *
     * @param webContents {@link WebContents} object.
     * @return {@link TextSuggestionHost} object.
     */
    @VisibleForTesting
    static TextSuggestionHost fromWebContents(WebContents webContents) {
        TextSuggestionHost ret =
                webContents.getOrSetUserData(
                        TextSuggestionHost.class, UserDataFactoryLazyHolder.INSTANCE);
        assert ret != null;
        return ret;
    }

    @CalledByNative
    private static TextSuggestionHost create(WebContents webContents, long nativePtr) {
        TextSuggestionHost host = fromWebContents(webContents);
        host.setNativePtr(nativePtr);
        return host;
    }

    /**
     * Create {@link TextSuggestionHost} instance.
     * @param webContents WebContents instance.
     */
    public TextSuggestionHost(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        Context context = mWebContents.getContext();
        assert context != null;
        mContext = context;
        mWindowAndroid = mWebContents.getTopLevelNativeWindow();

        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        assert viewDelegate != null;
        mViewDelegate = viewDelegate;
        PopupController.register(mWebContents, this);
        WindowEventObserverManager.from(mWebContents).addObserver(this);
    }

    private void setNativePtr(long nativePtr) {
        mNativeTextSuggestionHost = nativePtr;
    }

    private float getContentOffsetYPix() {
        return mWebContents.getRenderCoordinates().getContentOffsetYPix();
    }

    // WindowEventObserver

    @Override
    public void onWindowAndroidChanged(@Nullable WindowAndroid newWindowAndroid) {
        mWindowAndroid = newWindowAndroid;
        if (mSpellCheckPopupWindow != null) {
            mSpellCheckPopupWindow.updateWindowAndroid(mWindowAndroid);
        }
        if (mTextSuggestionsPopupWindow != null) {
            mTextSuggestionsPopupWindow.updateWindowAndroid(mWindowAndroid);
        }
    }

    @Override
    public void onAttachedToWindow() {
        mIsAttachedToWindow = true;
    }

    @Override
    public void onDetachedFromWindow() {
        mIsAttachedToWindow = false;
    }

    @Override
    public void onRotationChanged(int rotation) {
        hidePopups();
    }

    // HieablePopup
    @Override
    public void hide() {
        hidePopups();
    }

    @CalledByNative
    private void showSpellCheckSuggestionMenu(
            double caretXPx, double caretYPx, String markedText, String[] suggestions) {
        if (!mIsAttachedToWindow) {
            // This can happen if a new browser window is opened immediately after tapping a spell
            // check underline, before the timer to open the menu fires.
            onSuggestionMenuClosed(false);
            return;
        }

        hidePopups();
        mSpellCheckPopupWindow =
                new SpellCheckPopupWindow(
                        mContext,
                        this,
                        mWindowAndroid,
                        assumeNonNull(mViewDelegate.getContainerView()));

        mSpellCheckPopupWindow.show(
                caretXPx, caretYPx + getContentOffsetYPix(), markedText, suggestions);
    }

    @CalledByNative
    private void showTextSuggestionMenu(
            double caretXPx, double caretYPx, String markedText, SuggestionInfo[] suggestions) {
        if (!mIsAttachedToWindow) {
            // This can happen if a new browser window is opened immediately after tapping a spell
            // check underline, before the timer to open the menu fires.
            onSuggestionMenuClosed(false);
            return;
        }

        hidePopups();
        mTextSuggestionsPopupWindow =
                new TextSuggestionsPopupWindow(
                        mContext,
                        this,
                        mWindowAndroid,
                        assumeNonNull(mViewDelegate.getContainerView()));

        mTextSuggestionsPopupWindow.show(
                caretXPx, caretYPx + getContentOffsetYPix(), markedText, suggestions);
    }

    /** Hides the text suggestion menu (and informs Blink that it was closed). */
    @CalledByNative
    public void hidePopups() {
        if (mTextSuggestionsPopupWindow != null && mTextSuggestionsPopupWindow.isShowing()) {
            mTextSuggestionsPopupWindow.dismiss();
            mTextSuggestionsPopupWindow = null;
        }

        if (mSpellCheckPopupWindow != null && mSpellCheckPopupWindow.isShowing()) {
            mSpellCheckPopupWindow.dismiss();
            mSpellCheckPopupWindow = null;
        }
    }

    /** Tells Blink to replace the active suggestion range with the specified replacement. */
    public void applySpellCheckSuggestion(String suggestion) {
        TextSuggestionHostJni.get()
                .applySpellCheckSuggestion(mNativeTextSuggestionHost, suggestion);
    }

    /**
     * Tells Blink to replace the active suggestion range with the specified suggestion on the
     * specified marker.
     */
    public void applyTextSuggestion(int markerTag, int suggestionIndex) {
        TextSuggestionHostJni.get()
                .applyTextSuggestion(mNativeTextSuggestionHost, markerTag, suggestionIndex);
    }

    /** Tells Blink to delete the active suggestion range. */
    public void deleteActiveSuggestionRange() {
        TextSuggestionHostJni.get().deleteActiveSuggestionRange(mNativeTextSuggestionHost);
    }

    /** Tells Blink to remove spelling markers under all instances of the specified word. */
    public void onNewWordAddedToDictionary(@Nullable String word) {
        TextSuggestionHostJni.get().onNewWordAddedToDictionary(mNativeTextSuggestionHost, word);
    }

    /**
     * Tells Blink the suggestion menu was closed (and also clears the reference to the
     * SuggestionsPopupWindow instance so it can be garbage collected).
     */
    public void onSuggestionMenuClosed(boolean dismissedByItemTap) {
        if (!dismissedByItemTap) {
            TextSuggestionHostJni.get().onSuggestionMenuClosed(mNativeTextSuggestionHost);
        }
        mSpellCheckPopupWindow = null;
        mTextSuggestionsPopupWindow = null;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        hidePopups();
        mNativeTextSuggestionHost = 0;
    }

    /**
     * @return The TextSuggestionsPopupWindow, if one exists.
     */
    public @Nullable SuggestionsPopupWindow getTextSuggestionsPopupWindowForTesting() {
        return mTextSuggestionsPopupWindow;
    }

    /**
     * @return The SpellCheckPopupWindow, if one exists.
     */
    public @Nullable SuggestionsPopupWindow getSpellCheckPopupWindowForTesting() {
        return mSpellCheckPopupWindow;
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
