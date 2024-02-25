// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.Handler;
import android.os.IBinder;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** This is a fake View that is only exposed to InputMethodManager. */
public class ThreadedInputConnectionProxyView extends View {
    private static final String TAG = "ImeProxyView";
    private static final boolean DEBUG_LOGS = false;

    private final Handler mImeThreadHandler;
    private final View mContainerView;
    private final AtomicBoolean mFocused = new AtomicBoolean();
    private final AtomicBoolean mWindowFocused = new AtomicBoolean();
    private final AtomicReference<IBinder> mWindowToken = new AtomicReference<>();
    private final AtomicReference<View> mRootView = new AtomicReference<>();
    private final ThreadedInputConnectionFactory mFactory;

    ThreadedInputConnectionProxyView(
            Context context,
            Handler imeThreadHandler,
            View containerView,
            ThreadedInputConnectionFactory factory) {
        super(context);
        mImeThreadHandler = imeThreadHandler;
        mContainerView = containerView;
        setFocusable(true);
        setFocusableInTouchMode(true);
        setVisibility(View.VISIBLE);
        if (DEBUG_LOGS) Log.w(TAG, "constructor");

        mFocused.set(mContainerView.hasFocus());
        mWindowFocused.set(mContainerView.hasWindowFocus());
        mWindowToken.set(mContainerView.getWindowToken());
        mRootView.set(mContainerView.getRootView());
        mFactory = factory;
    }

    public void onOriginalViewFocusChanged(boolean gainFocus) {
        mFocused.set(gainFocus);
    }

    public void onOriginalViewWindowFocusChanged(boolean gainFocus) {
        if (DEBUG_LOGS) Log.w(TAG, "onOriginalViewWindowFocusChanged: " + gainFocus);
        mWindowFocused.set(gainFocus);
    }

    public void onOriginalViewAttachedToWindow() {
        mWindowToken.set(mContainerView.getWindowToken());
        // Note: this is an approximation of the real behavior.
        // Real root view may change upon addView / removeView, but this is good
        // enough for IME purpose.
        mRootView.set(mContainerView.getRootView());
    }

    public void onOriginalViewDetachedFromWindow() {
        mWindowToken.set(null);
        // Note: we are not asking mContainerView.getRootView() here. We cannot get the correct
        // root view here as ViewRootImpl's mParent is set to null *after* this call.
        // In vanilla Android, getRootView() is never called when window is detaching or detached
        // anyways.
        mRootView.set(null);
    }

    @Override
    public Handler getHandler() {
        if (DEBUG_LOGS) Log.w(TAG, "getHandler");
        return mImeThreadHandler;
    }

    @Override
    public boolean checkInputConnectionProxy(View view) {
        if (DEBUG_LOGS) Log.w(TAG, "checkInputConnectionProxy");
        return mContainerView == view;
    }

    @Override
    public InputConnection onCreateInputConnection(final EditorInfo outAttrs) {
        if (DEBUG_LOGS) Log.w(TAG, "onCreateInputConnection");
        return PostTask.runSynchronously(
                TaskTraits.UI_USER_BLOCKING,
                () -> {
                    mFactory.setTriggerDelayedOnCreateInputConnection(false);
                    InputConnection connection = mContainerView.onCreateInputConnection(outAttrs);
                    mFactory.setTriggerDelayedOnCreateInputConnection(true);
                    return connection;
                });
    }

    @Override
    public boolean hasWindowFocus() {
        boolean focused = mWindowFocused.get();
        if (DEBUG_LOGS) Log.w(TAG, "hasWindowFocus: " + focused);
        return focused;
    }

    @Override
    public View getRootView() {
        // Returning a null here matches mCurRootView being null value in InputMethodManager,
        // which represents that the current focused window is not IME target window.
        // In this case, you are still able to type.
        View rootView = mWindowFocused.get() ? mRootView.get() : null;
        if (DEBUG_LOGS) Log.w(TAG, "getRootView: " + rootView);
        return rootView;
    }

    @Override
    public boolean onCheckIsTextEditor() {
        if (DEBUG_LOGS) Log.w(TAG, "onCheckIsTextEditor");
        // We do not allow Android apps to override WebView#onCheckIsTextEditor() for now.
        return true;
    }

    @Override
    public boolean isFocused() {
        boolean focused = mFocused.get();
        if (DEBUG_LOGS) Log.w(TAG, "isFocused: " + focused);
        return focused;
    }

    @Override
    public IBinder getWindowToken() {
        if (DEBUG_LOGS) Log.w(TAG, "getWindowToken");
        return mWindowToken.get();
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (DEBUG_LOGS) Log.w(TAG, "onWindowFocusChanged:" + hasWindowFocus);
        super.onWindowFocusChanged(hasWindowFocus);
    }
}
