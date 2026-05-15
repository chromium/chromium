// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Callback;
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

import java.util.HashMap;
import java.util.Map;

/**
 * Handles displaying the Android spellcheck/text suggestion menu (provided by
 * SuggestionsPopupWindow) when requested by the C++ class TextSuggestionHostAndroid and applying
 * the commands in that menu (by calling back to the C++ class).
 *
 * <p>TextSuggestionPopupController is owned by the WebContents to prevent multiple text suggestion
 * popups from showing up at the same time.
 */
@JNINamespace("content")
@NullMarked
public class TextSuggestionPopupController implements WindowEventObserver, HideablePopup, UserData {
    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<TextSuggestionPopupController> INSTANCE =
                TextSuggestionPopupController::new;
    }

    private @Nullable TextSuggestionHost mLastShownHost;
    // The load bearing Java strong reference to the TextSuggestionHost is owned by this class. This
    // avoids the issue where the C++ TextSuggestionHost owns a strong JavaRef to the
    // TextSuggestionHost, which would prevent WebView from being GC'd.
    //
    // This map also handles parallel TextSuggestionHosts from existing at the same time due to the
    // asynchronous destruction order of Android popups.
    private final Map<Long, TextSuggestionHost> mTextSuggestionHosts = new HashMap<>();
    private final WebContentsImpl mWebContents;
    private final Context mContext;
    private final ViewAndroidDelegate mViewDelegate;

    private boolean mIsAttachedToWindow;
    private @Nullable WindowAndroid mWindowAndroid;

    private @Nullable SpellCheckPopupWindow mSpellCheckPopupWindow;
    private @Nullable TextSuggestionsPopupWindow mTextSuggestionsPopupWindow;

    private static @Nullable Callback<TextSuggestionPopupController> sOnCreationCallbackForTesting;

    /** Set callback for testing when a new instance is created. */
    static void setCreationCallbackForTesting(Callback<TextSuggestionPopupController> callback) {
        sOnCreationCallbackForTesting = callback;
    }

    public static @Nullable TextSuggestionPopupController fromWebContents(WebContents webContents) {
        return webContents.getOrSetUserData(
                TextSuggestionPopupController.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    /**
     * Create {@link TextSuggestionPopupController} instance.
     *
     * @param webContents WebContents instance.
     */
    public TextSuggestionPopupController(WebContents webContents) {
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

        if (sOnCreationCallbackForTesting != null) {
            sOnCreationCallbackForTesting.onResult(this);
        }
    }

    @CalledByNative
    private static void onNativeTextSuggestionHostDestroyed(
            WebContents webContents, long nativeTextSuggestionHostPtr) {
        TextSuggestionPopupController host =
                webContents.getOrSetUserData(TextSuggestionPopupController.class, null);
        if (host == null) return;

        TextSuggestionHost textSuggestionHost =
                host.mTextSuggestionHosts.remove(nativeTextSuggestionHostPtr);
        if (textSuggestionHost == null) return;

        textSuggestionHost.destroy();
        if (host.mLastShownHost == textSuggestionHost) {
            host.hidePopups();
            host.mLastShownHost = null;
        }
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

    private TextSuggestionHost getOrCreateTextSuggestionHost(long nativeTextSuggestionHostPtr) {
        TextSuggestionHost textSuggestionHost =
                mTextSuggestionHosts.get(nativeTextSuggestionHostPtr);
        if (textSuggestionHost == null) {
            textSuggestionHost =
                    new TextSuggestionHost(
                            nativeTextSuggestionHostPtr,
                            () -> {
                                mSpellCheckPopupWindow = null;
                                mTextSuggestionsPopupWindow = null;
                            });
            mTextSuggestionHosts.put(nativeTextSuggestionHostPtr, textSuggestionHost);
        }
        return textSuggestionHost;
    }

    @CalledByNative
    private static void showSpellCheckSuggestionMenu(
            WebContents webContents,
            long nativeTextSuggestionHostPtr,
            double caretXPx,
            double caretYPx,
            String markedText,
            String[] suggestions) {
        TextSuggestionPopupController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.showSpellCheckSuggestionMenu(
                nativeTextSuggestionHostPtr, caretXPx, caretYPx, markedText, suggestions);
    }

    private void showSpellCheckSuggestionMenu(
            long nativeTextSuggestionHostPtr,
            double caretXPx,
            double caretYPx,
            String markedText,
            String[] suggestions) {
        TextSuggestionHost textSuggestionHost =
                getOrCreateTextSuggestionHost(nativeTextSuggestionHostPtr);

        if (!mIsAttachedToWindow) {
            textSuggestionHost.onSuggestionMenuClosed(false);
            return;
        }

        hidePopups();
        mLastShownHost = textSuggestionHost;
        mSpellCheckPopupWindow =
                new SpellCheckPopupWindow(
                        mContext,
                        textSuggestionHost,
                        mWindowAndroid,
                        assumeNonNull(mViewDelegate.getContainerView()));

        mSpellCheckPopupWindow.show(
                caretXPx, caretYPx + getContentOffsetYPix(), markedText, suggestions);
    }

    @CalledByNative
    private static void showTextSuggestionMenu(
            WebContents webContents,
            long nativeTextSuggestionHostPtr,
            double caretXPx,
            double caretYPx,
            String markedText,
            SuggestionInfo[] suggestions) {
        TextSuggestionPopupController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.showTextSuggestionMenu(
                nativeTextSuggestionHostPtr, caretXPx, caretYPx, markedText, suggestions);
    }

    private void showTextSuggestionMenu(
            long nativeTextSuggestionHostPtr,
            double caretXPx,
            double caretYPx,
            String markedText,
            SuggestionInfo[] suggestions) {
        TextSuggestionHost textSuggestionHost =
                getOrCreateTextSuggestionHost(nativeTextSuggestionHostPtr);

        if (!mIsAttachedToWindow) {
            textSuggestionHost.onSuggestionMenuClosed(false);
            return;
        }

        hidePopups();
        mLastShownHost = textSuggestionHost;
        mTextSuggestionsPopupWindow =
                new TextSuggestionsPopupWindow(
                        mContext,
                        textSuggestionHost,
                        mWindowAndroid,
                        assumeNonNull(mViewDelegate.getContainerView()));

        mTextSuggestionsPopupWindow.show(
                caretXPx, caretYPx + getContentOffsetYPix(), markedText, suggestions);
    }

    /** Hides the text suggestion menu (and informs Blink that it was closed). */
    @CalledByNative
    public static void hidePopups(WebContents webContents) {
        TextSuggestionPopupController controller = fromWebContents(webContents);
        if (controller != null) {
            controller.hidePopups();
        }
    }

    /** Hides the text suggestion menu (and informs Blink that it was closed). */
    public void hidePopups() {
        if (mTextSuggestionsPopupWindow != null && mTextSuggestionsPopupWindow.isShowing()) {
            mTextSuggestionsPopupWindow.dismiss();
        }
        mTextSuggestionsPopupWindow = null;

        if (mSpellCheckPopupWindow != null && mSpellCheckPopupWindow.isShowing()) {
            mSpellCheckPopupWindow.dismiss();
        }
        mSpellCheckPopupWindow = null;
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

    public @Nullable TextSuggestionHost getLastShownHostForTesting() {
        return mLastShownHost;
    }

    public Map<Long, TextSuggestionHost> getTextSuggestionHostsForTesting() {
        return mTextSuggestionHosts;
    }
}
