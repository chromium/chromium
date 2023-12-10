// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.res.Configuration;

import org.chromium.base.ActivityState;
import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;

/** Manages {@link WindowEventObserver} instances used for WebContents. */
public final class WindowEventObserverManager implements DisplayAndroidObserver, UserData {
    private final ObserverList<WindowEventObserver> mWindowEventObservers = new ObserverList<>();

    private WindowAndroid mWindowAndroid;
    private ViewEventSinkImpl mViewEventSink;
    private boolean mAttachedToWindow;

    // The cache of device's current orientation and DIP scale factor.
    private int mRotation;
    private float mDipScale;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<WindowEventObserverManager> INSTANCE =
                WindowEventObserverManager::new;
    }

    public static WindowEventObserverManager from(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(
                        WindowEventObserverManager.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    private WindowEventObserverManager(WebContents webContents) {
        mViewEventSink = ViewEventSinkImpl.from(webContents);
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window != null) onWindowAndroidChanged(window);
        addObserver((WebContentsImpl) webContents);
    }

    /**
     * Add {@link WindowEventObserver} object.
     * @param observer Observer instance to add.
     */
    public void addObserver(WindowEventObserver observer) {
        assert !mWindowEventObservers.hasObserver(observer);
        mWindowEventObservers.addObserver(observer);
        if (mAttachedToWindow) observer.onAttachedToWindow();
    }

    /**
     * Remove {@link WindowEventObserver} object.
     * @param observer Observer instance to remove.
     */
    public void removeObserver(WindowEventObserver observer) {
        assert mWindowEventObservers.hasObserver(observer);
        mWindowEventObservers.removeObserver(observer);
    }

    /**
     * @see android.view.View#onAttachedToWindow()
     */
    public void onAttachedToWindow() {
        mAttachedToWindow = true;
        addUiObservers();
        for (WindowEventObserver observer : mWindowEventObservers) observer.onAttachedToWindow();
    }

    /**
     * @see android.view.View#onDetachedFromWindow()
     */
    public void onDetachedFromWindow() {
        removeUiObservers();
        mAttachedToWindow = false;
        for (WindowEventObserver observer : mWindowEventObservers) observer.onDetachedFromWindow();
    }

    /**
     * @see android.view.View#onWindowFocusChanged()
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onWindowFocusChanged(hasWindowFocus);
        }
    }

    /**
     * Called when {@link WindowAndroid} for WebContents is updated.
     * @param windowAndroid A new WindowAndroid object.
     */
    public void onWindowAndroidChanged(WindowAndroid windowAndroid) {
        if (windowAndroid == mWindowAndroid) return;
        removeUiObservers();

        mWindowAndroid = windowAndroid;
        addUiObservers();

        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onWindowAndroidChanged(windowAndroid);
        }
    }

    public void onConfigurationChanged(Configuration newConfig) {
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onConfigurationChanged(newConfig);
        }
    }

    public void onViewFocusChanged(boolean gainFocus, boolean hideKeyboardOnBlur) {
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onViewFocusChanged(gainFocus, hideKeyboardOnBlur);
        }
    }

    private void addDisplayAndroidObserverIfNeeded() {
        if (!mAttachedToWindow || mWindowAndroid == null) return;
        DisplayAndroid display = mWindowAndroid.getDisplay();
        display.addObserver(this);
        onRotationChanged(display.getRotation());
        onDIPScaleChanged(display.getDipScale());
    }

    private void addActivityStateObserver() {
        if (!mAttachedToWindow || mWindowAndroid == null) return;
        mWindowAndroid.addActivityStateObserver(mViewEventSink);
        // Sets the state of ViewEventSink right if activity is already in resumed state.
        // Can happen when the front tab gets moved down in the stack while Chrome
        // is in background. See https://crbug.com/852336.
        if (mWindowAndroid.getActivityState() == ActivityState.RESUMED) {
            mViewEventSink.onActivityResumed();
        }
    }

    private void addUiObservers() {
        addDisplayAndroidObserverIfNeeded();
        addActivityStateObserver();
    }

    private void removeUiObservers() {
        removeDisplayAndroidObserver();
        removeActivityStateObserver();
    }

    private void removeDisplayAndroidObserver() {
        if (mWindowAndroid == null) return;
        mWindowAndroid.getDisplay().removeObserver(this);
    }

    private void removeActivityStateObserver() {
        if (!mAttachedToWindow || mWindowAndroid == null) return;
        mWindowAndroid.removeActivityStateObserver(mViewEventSink);
    }

    // DisplayAndroidObserver

    @Override
    public void onRotationChanged(int rotation) {
        if (mRotation == rotation) return;
        mRotation = rotation;
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onRotationChanged(rotation);
        }
    }

    @Override
    public void onDIPScaleChanged(float dipScale) {
        if (mDipScale == dipScale) return;
        mDipScale = dipScale;
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onDIPScaleChanged(dipScale);
        }
    }
}
