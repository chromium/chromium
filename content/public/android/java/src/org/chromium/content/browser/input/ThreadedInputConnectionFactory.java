// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.content_public.browser.InputMethodManagerWrapper;

/**
 * A factory class for {@link ThreadedInputConnection}. The class also includes triggering
 * mechanism (hack) to run our InputConnection on non-UI thread.
 */
// TODO(changwan): add unit tests once Robolectric supports Android API level >= 21.
// See crbug.com/588547 for details.
public class ThreadedInputConnectionFactory implements ChromiumBaseInputConnection.Factory {
    private static final String TAG = "Ime";
    private static final boolean DEBUG_LOGS = false;

    // Most of the time we do not need to retry. But if we have lost window focus while triggering
    // delayed creation, then there is a chance that detection may fail in the following scenario:
    // InputMethodManagerService checks the window focus by directly calling
    // WindowManagerService#inputMethodClientHasFocus(). But the window focus change is
    // propagated to the view via ViewRootImpl's message queue. Therefore, it may take another
    // UI message loop until View#hasWindowFocus() is aligned with what IMMS sees.
    private static final int CHECK_REGISTER_RETRY = 1;

    private final InputMethodManagerWrapper mInputMethodManagerWrapper;
    private final InputMethodUma mInputMethodUma;
    private ThreadedInputConnectionProxyView mProxyView;
    private ThreadedInputConnection mThreadedInputConnection;
    private CheckInvalidator mCheckInvalidator;
    private boolean mReentrantTriggering;
    private boolean mTriggerDelayedOnCreateInputConnection;

    // Initialization-on-demand holder for Handler.
    private static class LazyHandlerHolder {
        // Note that we never exit this thread to avoid lifetime or thread-safety issues.
        private static final Handler sHandler;
        static {
            HandlerThread handlerThread =
                    new HandlerThread("InputConnectionHandlerThread", HandlerThread.NORM_PRIORITY);
            handlerThread.start();
            sHandler = new Handler(handlerThread.getLooper());
        }
    }

    // A small class that can be updated to invalidate the check when there is an external event
    // such as window focus loss or view focus loss.
    private static class CheckInvalidator {
        private boolean mInvalid;

        public void invalidate() {
            ImeUtils.checkOnUiThread();
            mInvalid = true;
        }

        public boolean isInvalid() {
            ImeUtils.checkOnUiThread();
            return mInvalid;
        }
    }

    ThreadedInputConnectionFactory(InputMethodManagerWrapper inputMethodManagerWrapper) {
        mInputMethodManagerWrapper = inputMethodManagerWrapper;
        mInputMethodUma = createInputMethodUma();
        mTriggerDelayedOnCreateInputConnection = true;
    }

    @Override
    public Handler getHandler() {
        return LazyHandlerHolder.sHandler;
    }

    @VisibleForTesting
    protected ThreadedInputConnectionProxyView createProxyView(
            Handler handler, View containerView) {
        return new ThreadedInputConnectionProxyView(
                containerView.getContext(), handler, containerView, this);
    }

    @VisibleForTesting
    protected InputMethodUma createInputMethodUma() {
        return new InputMethodUma();
    }

    @VisibleForTesting
    @Override
    public void setTriggerDelayedOnCreateInputConnection(boolean trigger) {
        mTriggerDelayedOnCreateInputConnection = trigger;
    }

    // Note that ThreadedInputConnectionProxyView intentionally calls
    // View#onCreateInputConnection() and not a separate method in this class.
    // There are third party apps that override WebView#onCreateInputConnection(),
    // and we still want to call them for consistency.
    // We let ThreadedInputConnectionProxyView and TestInputMethodManagerWrapper call
    // setTriggerDelayedOnCreateInputConnection(false) explicitly to avoid delayed triggering.
    private boolean shouldTriggerDelayedOnCreateInputConnection() {
        return mTriggerDelayedOnCreateInputConnection;
    }

    @Override
    public ThreadedInputConnection initializeAndGet(View view, ImeAdapterImpl imeAdapter,
            int inputType, int inputFlags, int inputMode, int inputAction, int selectionStart,
            int selectionEnd, EditorInfo outAttrs) {
        ImeUtils.checkOnUiThread();

        // Compute outAttrs early in case we early out to prevent reentrancy. (crbug.com/636197)
        // TODO(changwan): move this up to ImeAdapter once ReplicaInputConnection is deprecated.
        ImeUtils.computeEditorInfo(inputType, inputFlags, inputMode, inputAction, selectionStart,
                selectionEnd, outAttrs);
        if (DEBUG_LOGS) {
            Log.i(TAG, "initializeAndGet. outAttr: " + ImeUtils.getEditorInfoDebugString(outAttrs));
        }

        // https://crbug.com/820756
        final String htcMailPackageId = "com.htc.android.mail";
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N
                || htcMailPackageId.equals(view.getContext().getPackageName())) {
            // IMM can internally ignore subsequent activation requests, e.g., by checking
            // mServedConnecting.
            if (mCheckInvalidator != null) mCheckInvalidator.invalidate();

            if (shouldTriggerDelayedOnCreateInputConnection()) {
                triggerDelayedOnCreateInputConnection(view);
                return null;
            }
            if (DEBUG_LOGS) Log.i(TAG, "initializeAndGet: called from proxy view");
        }

        if (mThreadedInputConnection == null) {
            if (DEBUG_LOGS) Log.i(TAG, "Creating ThreadedInputConnection...");
            mThreadedInputConnection = new ThreadedInputConnection(view, imeAdapter, getHandler());
        } else {
            mThreadedInputConnection.resetOnUiThread();
        }
        return mThreadedInputConnection;
    }

    private void triggerDelayedOnCreateInputConnection(final View view) {
        if (DEBUG_LOGS) Log.i(TAG, "triggerDelayedOnCreateInputConnection");
        // Prevent infinite loop when View methods trigger onCreateInputConnection
        // on some OEM phones. (crbug.com/636197)
        if (mReentrantTriggering) return;

        // We need to check this before creating invalidator.
        if (!view.hasFocus()) return;

        mCheckInvalidator = new CheckInvalidator();

        if (!view.hasWindowFocus()) mCheckInvalidator.invalidate();

        // We cannot reuse the existing proxy view, if any, due to crbug.com/664402.
        mProxyView = createProxyView(getHandler(), view);

        mReentrantTriggering = true;
        // This does not affect view focus of the real views.
        mProxyView.requestFocus();
        mReentrantTriggering = false;

        view.getHandler().post(new Runnable() {
            @Override
            public void run() {
                // This is a hack to make InputMethodManager believe that the proxy view
                // now has a focus. As a result, InputMethodManager will think that mProxyView
                // is focused, and will call getHandler() of the view when creating input
                // connection.

                // Step 1: Set mProxyView as InputMethodManager#mNextServedView.
                // This does not affect the real window focus.
                mProxyView.onWindowFocusChanged(true);

                // Step 2: Have InputMethodManager focus in on mNextServedView.
                // As a result, IMM will call onCreateInputConnection() on mProxyView on the same
                // thread as mProxyView.getHandler(). It will also call subsequent InputConnection
                // methods on this IME thread.
                mInputMethodManagerWrapper.isActive(view);

                // Step 3: Check that the above hack worked.
                // Do not check until activation finishes inside InputMethodManager (on IME thread).
                getHandler().post(new Runnable() {
                    @Override
                    public void run() {
                        postCheckRegisterResultOnUiThread(view, mCheckInvalidator,
                                CHECK_REGISTER_RETRY);
                    }
                });
            }
        });
    }

    // Note that this function is called both from IME thread and UI thread.
    private void postCheckRegisterResultOnUiThread(final View view,
            final CheckInvalidator checkInvalidator, final int retry) {
        // Now posting on UI thread to access view methods.
        final Handler viewHandler = view.getHandler();
        if (viewHandler == null) return;
        viewHandler.post(new Runnable() {
            @Override
            public void run() {
                checkRegisterResult(view, checkInvalidator, retry);
            }
        });
    }

    private void checkRegisterResult(View view, CheckInvalidator checkInvalidator, int retry) {
        if (DEBUG_LOGS) Log.i(TAG, "checkRegisterResult - retry: " + retry);
        // Success.
        if (mInputMethodManagerWrapper.isActive(mProxyView)) {
            onRegisterProxyViewSuccess();
            return;
        }

        if (retry > 0) {
            postCheckRegisterResultOnUiThread(view, checkInvalidator, retry - 1);
            return;
        }

        if (checkInvalidator.isInvalid()) return;

        onRegisterProxyViewFailure();
    }

    @VisibleForTesting
    protected void onRegisterProxyViewSuccess() {
        Log.d(TAG, "onRegisterProxyViewSuccess");
        mInputMethodUma.recordProxyViewSuccess();
    }

    @VisibleForTesting
    protected void onRegisterProxyViewFailure() {
        Log.w(TAG, "onRegisterProxyViewFailure");
        mInputMethodUma.recordProxyViewFailure();
    }

    @Override
    public void onWindowFocusChanged(boolean gainFocus) {
        if (DEBUG_LOGS) Log.d(TAG, "onWindowFocusChanged: " + gainFocus);
        if (!gainFocus && mCheckInvalidator != null) mCheckInvalidator.invalidate();
        if (mProxyView != null) mProxyView.onOriginalViewWindowFocusChanged(gainFocus);
    }

    @Override
    public void onViewFocusChanged(boolean gainFocus) {
        if (DEBUG_LOGS) Log.d(TAG, "onViewFocusChanged: " + gainFocus);
        if (!gainFocus && mCheckInvalidator != null) mCheckInvalidator.invalidate();
        if (mProxyView != null) mProxyView.onOriginalViewFocusChanged(gainFocus);
    }

    @Override
    public void onViewAttachedToWindow() {
        if (DEBUG_LOGS) Log.d(TAG, "onViewAttachedToWindow");
        if (mProxyView != null) mProxyView.onOriginalViewAttachedToWindow();
    }

    @Override
    public void onViewDetachedFromWindow() {
        if (DEBUG_LOGS) Log.d(TAG, "onViewDetachedFromWindow");
        if (mCheckInvalidator != null) mCheckInvalidator.invalidate();
        if (mProxyView != null) mProxyView.onOriginalViewDetachedFromWindow();
    }
}
