// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Configuration;

import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;

/** Implementation of the interface {@link ViewEventSink}. */
@NullMarked
public final class ViewEventSinkImpl implements ViewEventSink, ActivityStateObserver, UserData {
    private final WebContentsImpl mWebContents;

    // Whether the container view has view-level focus.
    private @Nullable Boolean mHasViewFocus;

    // This is used in place of window focus on the container view, as we can't actually use window
    // focus due to issues where content expects to be focused while a popup steals window focus.
    // See https://crbug.com/686232 for more context.
    private boolean mIsTopActivity = true;

    // Whether we consider this WebContents to have input focus. This is computed through
    // mHasViewFocus and mIsTopActivity. See the comments on mIsTopActivity for how this doesn't
    // exactly match Android's notion of input focus and why we need to do this.
    private @Nullable Boolean mHasInputFocus;
    private boolean mHideKeyboardOnBlur;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<ViewEventSinkImpl> INSTANCE = ViewEventSinkImpl::new;
    }

    public static ViewEventSinkImpl from(WebContents webContents) {
        ViewEventSinkImpl ret =
                webContents.getOrSetUserData(
                        ViewEventSinkImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        assert ret != null;
        return ret;
    }

    public ViewEventSinkImpl(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
    }

    @Override
    public void setAccessDelegate(ViewEventSink.InternalAccessDelegate accessDelegate) {
        GestureListenerManagerImpl gestureManager =
                GestureListenerManagerImpl.fromWebContents(mWebContents);
        assumeNonNull(gestureManager);
        gestureManager.setScrollDelegate(accessDelegate);
        ContentUiEventHandler.fromWebContents(mWebContents).setEventDelegate(accessDelegate);
    }

    @Override
    public void onAttachedToWindow() {
        WindowEventObserverManager.from(mWebContents).onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        WindowEventObserverManager.from(mWebContents).onDetachedFromWindow();
        // Stylus Writing
        if (mWebContents.getStylusWritingHandler() != null) {
            ViewAndroidDelegate viewAndroidDelegate = mWebContents.getViewAndroidDelegate();
            if (viewAndroidDelegate != null) {
                mWebContents
                        .getStylusWritingHandler()
                        .onDetachedFromWindow(viewAndroidDelegate.getContainerView().getContext());
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        WindowEventObserverManager.from(mWebContents).onWindowFocusChanged(hasWindowFocus);
    }

    @Override
    public void onViewFocusChanged(boolean gainFocus) {
        if (mHasViewFocus != null && mHasViewFocus == gainFocus) return;
        mHasViewFocus = gainFocus;
        onFocusChanged();

        // Stylus Writing
        if (mWebContents.getStylusWritingHandler() != null) {
            mWebContents.getStylusWritingHandler().onFocusChanged(gainFocus);
        }
    }

    @Override
    public void setHideKeyboardOnBlur(boolean hideKeyboardOnBlur) {
        mHideKeyboardOnBlur = hideKeyboardOnBlur;
    }

    @SuppressWarnings("javadoc")
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        try {
            TraceEvent.begin("ViewEventSink.onConfigurationChanged");
            WindowEventObserverManager.from(mWebContents).onConfigurationChanged(newConfig);
            // To request layout has side effect, but it seems OK as it only happen in
            // onConfigurationChange and layout has to be changed in most case.
            ViewAndroidDelegate delegate = mWebContents.getViewAndroidDelegate();
            if (delegate != null) {
                ViewUtils.requestLayout(
                        delegate.getContainerView(), "ViewEventSinkImpl.onConfigurationChanged");
            }
        } finally {
            TraceEvent.end("ViewEventSink.onConfigurationChanged");
        }
    }

    private void onFocusChanged() {
        // Wait for view focus to be set before propagating focus changes.
        if (mHasViewFocus == null) return;

        // See the comments on mIsTopActivity for why we use it to compute input focus.
        boolean hasInputFocus = mHasViewFocus && mIsTopActivity;
        if (mHasInputFocus != null && mHasInputFocus == hasInputFocus) return;
        mHasInputFocus = hasInputFocus;

        if (mWebContents == null) {
            // CVC is on its way to destruction. The rest needs not running as all the states
            // will be discarded, or WebContentsUserData-based objects are not reachable
            // any more. Simply return here.
            return;
        }
        WindowEventObserverManager.from(mWebContents)
                .onViewFocusChanged(mHasInputFocus, mHideKeyboardOnBlur);
        mWebContents.setFocus(mHasInputFocus);
    }

    // ActivityStateObserver

    @Override
    public void onActivityTopResumedChanged(boolean isTopResumedActivity) {
        // When the activity is not in the top anymore (e.g.
        // Activity.onTopResumedActivityChanged() with false, the activity pauses), the content
        // should lose focus.
        // On devices which does not support Activity.onTopResumedActivityChanged() (i.e. Android 9
        // and lower, or WebView), WindowAndroid emulates onTopResumedActivityChanged() behavior
        // on `onActivityResumed()` and `onActivityPaused()`.

        // TODO(mthiesse): See https://crbug.com/686232 for context. Desktop platforms use keyboard
        // focus to trigger blur/focus, and the equivalent to this on Android is Window focus.
        // However, we don't use Window focus because of the complexity around popups stealing
        // Window focus.

        if (isTopResumedActivity == mIsTopActivity) {
            return;
        }
        mIsTopActivity = isTopResumedActivity;
        onFocusChanged();
    }

    @Override
    public void onActivityDestroyed() {}

    @Override
    public void onActivityTopResumedChangedForTesting(boolean isTopResumedActivity) {
        onActivityTopResumedChanged(isTopResumedActivity);
    }
}
