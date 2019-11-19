// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.os.IBinder;
import android.os.ResultReceiver;
import android.util.Pair;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.input.Range;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.InputMethodManagerWrapper;

import java.util.ArrayList;
import java.util.List;

/**
 * Overrides InputMethodManagerWrapper for testing purposes.
 */
public class TestInputMethodManagerWrapper implements InputMethodManagerWrapper {
    private static final String TAG = "Ime";

    private final InputConnectionProvider mInputConnectionProvider;
    private InputConnection mInputConnection;
    private int mRestartInputCounter;
    private int mShowSoftInputCounter;
    private int mHideSoftInputCounter;
    private final Range mSelection = new Range(-1, -1);
    private final Range mComposition = new Range(-1, -1);
    private boolean mIsShowWithoutHideOutstanding;
    private final List<Pair<Range, Range>> mUpdateSelectionList;
    private int mUpdateCursorAnchorInfoCounter;
    private CursorAnchorInfo mLastCursorAnchorInfo;
    private final ArrayList<EditorInfo> mEditorInfoList = new ArrayList<>();

    /**
     * Interface passed that helps this class create {@link InputConnection} instance.
     * This helps the wrapper avoid cross-reference {@link ImeAdapter} object.
     */
    public interface InputConnectionProvider {
        /*
         * @param info {@link EditInfo} object used to create a new {@link InputConnection}.
         * @return a newly created {@link InputConnection} instance.
         */
        InputConnection create(EditorInfo info);
    }

    /**
     * Default {@InputConnectionProvider} that uses a given {@link ImeAdapter} to create {@link
     * InputConnection}.
     */
    public static InputConnectionProvider defaultInputConnectionProvider(
            final ImeAdapter imeAdapter) {
        return (EditorInfo info) -> {
            ImeAdapterImpl imeAdapterImpl = (ImeAdapterImpl) imeAdapter;
            imeAdapterImpl.setTriggerDelayedOnCreateInputConnectionForTest(false);
            InputConnection connection = imeAdapter.onCreateInputConnection(info);
            imeAdapterImpl.setTriggerDelayedOnCreateInputConnectionForTest(true);
            return connection;
        };
    }

    /**
     * Default {@link TestInputMethodManagerWrapper} instance good enough for most of test cases.
     */
    public static TestInputMethodManagerWrapper create(ImeAdapter imeAdapter) {
        return new TestInputMethodManagerWrapper(defaultInputConnectionProvider(imeAdapter));
    }

    public TestInputMethodManagerWrapper(InputConnectionProvider provider) {
        Log.d(TAG, "TestInputMethodManagerWrapper constructor");
        mInputConnectionProvider = provider;
        mUpdateSelectionList = new ArrayList<>();
    }

    @Override
    public void restartInput(View view) {
        mRestartInputCounter++;
        Log.d(TAG, "restartInput: count [%d]", mRestartInputCounter);
        EditorInfo editorInfo = new EditorInfo();
        mInputConnection = mInputConnectionProvider.create(editorInfo);
        mEditorInfoList.add(editorInfo);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        mIsShowWithoutHideOutstanding = true;
        mShowSoftInputCounter++;
        Log.d(TAG, "showSoftInput: count [%d]", mShowSoftInputCounter);
        if (mInputConnection != null) return;
        EditorInfo editorInfo = new EditorInfo();
        mInputConnection = mInputConnectionProvider.create(editorInfo);
        mEditorInfoList.add(editorInfo);
    }

    @Override
    public boolean isActive(View view) {
        boolean result = mInputConnection != null;
        Log.d(TAG, "isActive: returns [%b]", result);
        return result;
    }

    @Override
    public boolean hideSoftInputFromWindow(
            IBinder windowToken, int flags, ResultReceiver resultReceiver) {
        mIsShowWithoutHideOutstanding = false;
        mHideSoftInputCounter++;
        Log.d(TAG, "hideSoftInputFromWindow: count [%d]", mHideSoftInputCounter);
        boolean retVal = mInputConnection == null;
        mInputConnection = null;
        return retVal;
    }

    @Override
    public void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
        Log.d(TAG, "updateSelection: [%d %d] [%d %d]", selStart, selEnd, candidatesStart,
                candidatesEnd);
        Pair<Range, Range> newUpdateSelection =
                new Pair<>(new Range(selStart, selEnd), new Range(candidatesStart, candidatesEnd));
        Range lastSelection = null;
        Range lastComposition = null;
        if (!mUpdateSelectionList.isEmpty()) {
            Pair<Range, Range> lastUpdateSelection =
                    mUpdateSelectionList.get(mUpdateSelectionList.size() - 1);
            if (lastUpdateSelection.equals(newUpdateSelection)) return;
            lastSelection = lastUpdateSelection.first;
            lastComposition = lastUpdateSelection.second;
        }
        mUpdateSelectionList.add(new Pair<Range, Range>(
                new Range(selStart, selEnd), new Range(candidatesStart, candidatesEnd)));
        mSelection.set(selStart, selEnd);
        mComposition.set(candidatesStart, candidatesEnd);
        onUpdateSelection(lastSelection, lastComposition, mSelection, mComposition);
    }

    @Override
    public void updateExtractedText(
            View view, int token, android.view.inputmethod.ExtractedText text) {}

    @Override
    public void notifyUserAction() {}

    public final List<Pair<Range, Range>> getUpdateSelectionList() {
        return mUpdateSelectionList;
    }

    public int getRestartInputCounter() {
        return mRestartInputCounter;
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        mUpdateCursorAnchorInfoCounter++;
        mLastCursorAnchorInfo = cursorAnchorInfo;
    }

    public int getShowSoftInputCounter() {
        Log.d(TAG, "getShowSoftInputCounter: %d", mShowSoftInputCounter);
        return mShowSoftInputCounter;
    }

    public int getHideSoftInputCounter() {
        return mHideSoftInputCounter;
    }

    public void reset() {
        Log.d(TAG, "reset");
        mRestartInputCounter = 0;
        mShowSoftInputCounter = 0;
        mHideSoftInputCounter = 0;
        mUpdateSelectionList.clear();
        mEditorInfoList.clear();
    }

    public InputConnection getInputConnection() {
        return mInputConnection;
    }

    public Range getSelection() {
        return mSelection;
    }

    public Range getComposition() {
        return mComposition;
    }

    public boolean isShowWithoutHideOutstanding() {
        return mIsShowWithoutHideOutstanding;
    }

    public int getUpdateCursorAnchorInfoCounter() {
        return mUpdateCursorAnchorInfoCounter;
    }

    public void clearLastCursorAnchorInfo() {
        mLastCursorAnchorInfo = null;
    }

    public CursorAnchorInfo getLastCursorAnchorInfo() {
        return mLastCursorAnchorInfo;
    }

    public ArrayList<EditorInfo> getEditorInfoList() {
        return mEditorInfoList;
    }

    public void onUpdateSelection(Range oldSel, Range oldComp, Range newSel, Range newComp) {}

    public void expectsSelectionOutsideComposition() {}
}
