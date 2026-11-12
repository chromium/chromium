// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniRawPtr;

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

import java.util.HashSet;
import java.util.Set;

/**
 * Handles displaying and hiding the Android spellcheck/text suggestion menu (provided by {@link
 * SuggestionsPopupWindow}) when requested by the C++ class TextSuggestionHostAndroid. {@link
 * TextSuggestionHost} handles applying menu commands back to C++.
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
    // One TextSuggestionHost per document, created on demand by the native
    // TextSuggestionHostAndroid, which holds it weakly so WebView can still be garbage collected.
    // Holding the strong references here also guarantees destroy() releases each host's JniRawPtr
    // when WebContentsImpl.clearNativePtr() destroys this controller. Several hosts can coexist
    // across frames and navigations; this controller shows at most one popup at a time.
    private final Set<TextSuggestionHost> mTextSuggestionHosts = new HashSet<>();
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

    @Override
    public void destroy() {
        boolean lastShownDestroyed = mLastShownHost == null;
        for (TextSuggestionHost host : mTextSuggestionHosts) {
            if (host == mLastShownHost) {
                lastShownDestroyed = true;
            }
            host.destroy();
        }
        mTextSuggestionHosts.clear();
        assert lastShownDestroyed;
        mLastShownHost = null;
        hidePopups();
    }

    @CalledByNative
    static @Nullable TextSuggestionHost createTextSuggestionHost(
            WebContents webContents,
            JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr) {
        TextSuggestionPopupController controller = fromWebContents(webContents);
        if (controller == null) {
            nativePtr.release();
            return null;
        }
        return controller.createTextSuggestionHost(nativePtr);
    }

    private TextSuggestionHost createTextSuggestionHost(
            JniRawPtr<TextSuggestionHost.NativeTextSuggestionHostAndroid> nativePtr) {
        TextSuggestionHost host =
                new TextSuggestionHost(
                        nativePtr,
                        () -> {
                            mSpellCheckPopupWindow = null;
                            mTextSuggestionsPopupWindow = null;
                        });
        mTextSuggestionHosts.add(host);
        return host;
    }

    @CalledByNative
    private static void onNativeTextSuggestionHostDestroyed(
            WebContents webContents, TextSuggestionHost textSuggestionHost) {
        TextSuggestionPopupController host =
                webContents.getOrSetUserData(TextSuggestionPopupController.class, null);
        if (host == null) return;
        if (!host.mTextSuggestionHosts.remove(textSuggestionHost)) return;

        textSuggestionHost.destroy();
        if (host.mLastShownHost == textSuggestionHost) {
            host.hidePopups();
            host.mLastShownHost = null;
        }
    }

    @VisibleForTesting
    double calculateYOffset(double caretYPx) {
        float contentOffsetYPix = mWebContents.getRenderCoordinates().getContentOffsetYPix();
        if (Double.isNaN(caretYPx)) return contentOffsetYPix;

        int containerHeight = assumeNonNull(mViewDelegate.getContainerView()).getHeight();
        double maxOffset = Math.max((double) contentOffsetYPix, (double) containerHeight);
        return Math.clamp(caretYPx + contentOffsetYPix, contentOffsetYPix, maxOffset);
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
    static void showSpellCheckSuggestionMenu(
            WebContents webContents,
            TextSuggestionHost textSuggestionHost,
            double caretXPx,
            double caretYPx,
            String markedText,
            String[] suggestions) {
        TextSuggestionPopupController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.showSpellCheckSuggestionMenu(
                textSuggestionHost, caretXPx, caretYPx, markedText, suggestions);
    }

    private void showSpellCheckSuggestionMenu(
            TextSuggestionHost textSuggestionHost,
            double caretXPx,
            double caretYPx,
            String markedText,
            String[] suggestions) {
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

        mSpellCheckPopupWindow.show(caretXPx, calculateYOffset(caretYPx), markedText, suggestions);
        if (!mSpellCheckPopupWindow.isShowing()) {
            mSpellCheckPopupWindow = null;
            textSuggestionHost.onSuggestionMenuClosed(false);
        }
    }

    @CalledByNative
    private static void showTextSuggestionMenu(
            WebContents webContents,
            TextSuggestionHost textSuggestionHost,
            double caretXPx,
            double caretYPx,
            String markedText,
            SuggestionInfo[] suggestions) {
        TextSuggestionPopupController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.showTextSuggestionMenu(
                textSuggestionHost, caretXPx, caretYPx, markedText, suggestions);
    }

    private void showTextSuggestionMenu(
            TextSuggestionHost textSuggestionHost,
            double caretXPx,
            double caretYPx,
            String markedText,
            SuggestionInfo[] suggestions) {
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
                caretXPx, calculateYOffset(caretYPx), markedText, suggestions);
        if (!mTextSuggestionsPopupWindow.isShowing()) {
            mTextSuggestionsPopupWindow = null;
            textSuggestionHost.onSuggestionMenuClosed(false);
        }
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

    public void setLastShownHostForTesting(TextSuggestionHost host) {
        mLastShownHost = host;
    }

    public Set<TextSuggestionHost> getTextSuggestionHostsForTesting() {
        return mTextSuggestionHosts;
    }
}
