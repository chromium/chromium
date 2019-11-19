// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.Build;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.os.StrictMode;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.Log;
import org.chromium.content_public.browser.InputMethodManagerWrapper;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Wrapper around Android's InputMethodManager
 */
public class InputMethodManagerWrapperImpl implements InputMethodManagerWrapper {
    private static final boolean DEBUG_LOGS = false;
    private static final String TAG = "IMM";

    private final Context mContext;

    public InputMethodManagerWrapperImpl(Context context) {
        if (DEBUG_LOGS) Log.i(TAG, "Constructor");
        mContext = context;
    }

    private InputMethodManager getInputMethodManager() {
        return (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    @Override
    public void restartInput(View view) {
        if (DEBUG_LOGS) Log.i(TAG, "restartInput");
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        manager.restartInput(view);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "showSoftInput");
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites(); // crbug.com/616283
        try {
            InputMethodManager manager = getInputMethodManager();
            if (manager != null) manager.showSoftInput(view, flags, resultReceiver);
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
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites(); // crbug.com/616283
        try {
            InputMethodManager manager = getInputMethodManager();
            return manager != null
                    && manager.hideSoftInputFromWindow(windowToken, flags, resultReceiver);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "updateSelection: SEL [%d, %d], COM [%d, %d]", selStart, selEnd,
                    candidatesStart, candidatesEnd);
        }
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        manager.updateSelection(view, selStart, selEnd, candidatesStart, candidatesEnd);
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        if (DEBUG_LOGS) Log.i(TAG, "updateCursorAnchorInfo");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            InputMethodManager manager = getInputMethodManager();
            if (manager == null) return;
            manager.updateCursorAnchorInfo(view, cursorAnchorInfo);
        }
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
    public void notifyUserAction() {
        // On N and above, this is not needed.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) return;
        if (DEBUG_LOGS) Log.i(TAG, "notifyUserAction");
        InputMethodManager manager = getInputMethodManager();
        if (manager == null) return;
        try {
            Method method = InputMethodManager.class.getMethod("notifyUserAction");
            method.invoke(manager);
        } catch (NoSuchMethodException | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException e) {
            if (DEBUG_LOGS) Log.i(TAG, "notifyUserAction failed");
            return;
        }
    }
}
