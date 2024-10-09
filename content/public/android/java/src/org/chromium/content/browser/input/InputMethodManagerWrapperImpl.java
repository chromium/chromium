// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.os.StrictMode;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.InputMethodManagerWrapper;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.ref.WeakReference;

/** Wrapper around Android's InputMethodManager */
public class InputMethodManagerWrapperImpl implements InputMethodManagerWrapper {
    private static final boolean DEBUG_LOGS = false;
    private static final String TAG = "IMM";

    private final Context mContext;

    private WindowAndroid mWindowAndroid;

    private Delegate mDelegate;

    private Runnable mPendingRunnableOnInputConnection;

    private boolean mOptimizeImmHideCalls;

    public InputMethodManagerWrapperImpl(
            Context context, WindowAndroid windowAndroid, Delegate delegate) {
        if (DEBUG_LOGS) Log.i(TAG, "Constructor");
        mContext = context;
        mWindowAndroid = windowAndroid;
        mDelegate = delegate;
        mOptimizeImmHideCalls =
                ContentFeatureMap.isEnabled(ContentFeatureList.OPTIMIZE_IMM_HIDE_CALLS);
    }

    @Override
    public void onWindowAndroidChanged(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    private Context getContextForMultiDisplay() {
        Activity activity = getActivityFromWindowAndroid(mWindowAndroid);
        if (DEBUG_LOGS) {
            if (activity == null) Log.i(TAG, "activity is null.");
        }
        return activity == null ? mContext : activity;
    }

    /**
     * Get an Activity from WindowAndroid.
     *
     * @return The Activity. May return null if it fails.
     */
    private static Activity getActivityFromWindowAndroid(WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        // Unwrap this when we actually need it.
        WeakReference<Activity> weakRef = windowAndroid.getActivity();
        if (weakRef == null) return null;
        return weakRef.get();
    }

    private InputMethodManager getInputMethodManager() {
        // For multi-display case, we need to have the correct activity context.
        // However, for Chrome, we wrap application context as container view's
        // context in order to prevent leakage. (e.g. see TabImpl.java).
        // This is a workaround to update mContext with the activity from
        // ActivityWindowAndroid. https://crbug.com/1021403
        Context context = getContextForMultiDisplay();
        return (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    @Override
    public void restartInput(View view) {
        if (DEBUG_LOGS) Log.i(TAG, "restartInput");
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        manager.restartInput(view);
    }

    @VisibleForTesting
    protected boolean hasCorrectDisplayId(Context context, Activity activity) {
        // We did not support multi-display before O.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return true;

        int contextDisplayId = getDisplayId(context);
        int activityDisplayId = getDisplayId(activity);
        if (activityDisplayId != contextDisplayId) {
            Log.w(
                    TAG,
                    "Activity's display ID(%d) does not match context's display ID(%d). "
                            + "Using a workaround to show soft input on the correct display...",
                    activityDisplayId,
                    contextDisplayId);
            return false;
        }
        return true;
    }

    @VisibleForTesting
    protected int getDisplayId(Context context) {
        return DisplayAndroid.getNonMultiDisplay(context).getDisplayId();
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "showSoftInput");
        mPendingRunnableOnInputConnection = null;
        Activity activity = getActivityFromWindowAndroid(mWindowAndroid);
        if (activity != null && !hasCorrectDisplayId(mContext, activity)) {
            // https://crbug.com/1021403
            // This is a workaround for multi-display case. We need this as long as
            // Chrome uses the application context for creating the content view.
            // Note that this will create a few ms delay in showing keyboard.
            activity.getWindow().setLocalFocus(true, true);

            if (mDelegate != null && !mDelegate.hasInputConnection()) {
                // Delay keyboard showing until input connection is established.
                mPendingRunnableOnInputConnection =
                        () -> {
                            if (isActive(view)) showSoftInputInternal(view, flags, resultReceiver);
                        };
                return;
            }
            // If we already have InputConnection, then show soft input now.
        }
        showSoftInputInternal(view, flags, resultReceiver);
    }

    private void showSoftInputInternal(View view, int flags, ResultReceiver resultReceiver) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites(); // crbug.com/616283
        try {
            InputMethodManager manager = getInputMethodManager();
            if (manager != null) {
                boolean result = manager.showSoftInput(view, flags, resultReceiver);
                if (DEBUG_LOGS) Log.i(TAG, "showSoftInputInternal: " + view + ", " + result);
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public boolean isActive(View view) {
        InputMethodManager manager = getInputMethodManager();
        final boolean active = manager != null && manager.isActive(view);
        if (DEBUG_LOGS) Log.i(TAG, "isActive: " + active);
        return active;
    }

    @Override
    public boolean hideSoftInputFromWindow(
            IBinder windowToken, int flags, ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "hideSoftInputFromWindow");
        mPendingRunnableOnInputConnection = null;
        InputMethodManager manager = getInputMethodManager();
        if (manager == null || (mOptimizeImmHideCalls && !manager.isAcceptingText())) return false;
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites(); // crbug.com/616283
        try {
            return manager.hideSoftInputFromWindow(windowToken, flags, resultReceiver);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
        if (DEBUG_LOGS) {
            Log.i(
                    TAG,
                    "updateSelection: SEL [%d, %d], COM [%d, %d]",
                    selStart,
                    selEnd,
                    candidatesStart,
                    candidatesEnd);
        }
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        manager.updateSelection(view, selStart, selEnd, candidatesStart, candidatesEnd);
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        if (DEBUG_LOGS) Log.i(TAG, "updateCursorAnchorInfo");
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        manager.updateCursorAnchorInfo(view, cursorAnchorInfo);
    }

    @Override
    public void updateExtractedText(
            View view, int token, android.view.inputmethod.ExtractedText text) {
        if (DEBUG_LOGS) Log.d(TAG, "updateExtractedText");
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        manager.updateExtractedText(view, token, text);
    }

    @Override
    public void onInputConnectionCreated() {
        if (mPendingRunnableOnInputConnection == null) return;
        Runnable runnable = mPendingRunnableOnInputConnection;
        mPendingRunnableOnInputConnection = null;
        PostTask.postTask(TaskTraits.UI_DEFAULT, runnable);
    }
}
