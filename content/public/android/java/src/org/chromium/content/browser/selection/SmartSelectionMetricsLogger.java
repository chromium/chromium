// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;

import org.chromium.base.Log;
import org.chromium.base.annotations.VerifiesOnP;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionMetricsLogger;

/**
 * Smart Selection logger, wrapper of Android logger methods.
 * We are logging word indices here. For one example:
 *     New York City , NY
 *    -1   0    1    2 3  4
 * We selected "York" at the first place, so York is [0, 1). Then we assume that Smart Selection
 * expanded the selection range to the whole "New York City , NY", we need to log [-1, 4). After
 * that, we single tap on "City", Smart Selection reset get triggered, we need to log [1, 2). Spaces
 * are ignored but we count each punctuation mark as a word.
 */
@VerifiesOnP
@TargetApi(Build.VERSION_CODES.P)
public class SmartSelectionMetricsLogger implements SelectionMetricsLogger {
    private static final String TAG = "SmartSelectionLogger";
    private static final boolean DEBUG = false;

    private Context mContext;

    private TextClassifier mSession;

    private SelectionIndicesConverter mConverter;

    public static SmartSelectionMetricsLogger create(Context context) {
        if (context == null) {
            return null;
        }
        return new SmartSelectionMetricsLogger(context);
    }

    private SmartSelectionMetricsLogger(Context context) {
        mContext = context;
    }

    public void logSelectionStarted(String selectionText, int startOffset, boolean editable) {
        mSession = createSession(mContext, editable);
        mConverter = new SelectionIndicesConverter();
        mConverter.updateSelectionState(selectionText, startOffset);
        mConverter.setInitialStartOffset(startOffset);

        if (DEBUG) Log.d(TAG, "logSelectionStarted");
        logEvent(SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_MANUAL, 0));
    }

    public void logSelectionModified(
            String selectionText, int startOffset, SelectionClient.Result result) {
        if (mSession == null) return;
        if (!mConverter.updateSelectionState(selectionText, startOffset)) {
            // DOM change detected, end logging session.
            endTextClassificationSession();
            return;
        }

        int endOffset = startOffset + selectionText.length();
        int[] indices = new int[2];
        if (!mConverter.getWordDelta(startOffset, endOffset, indices)) {
            // Invalid indices, end logging session.
            endTextClassificationSession();
            return;
        }

        if (DEBUG) Log.d(TAG, "logSelectionModified [%d, %d)", indices[0], indices[1]);
        if (result != null && result.textSelection != null) {
            logEvent(SelectionEvent.createSelectionModifiedEvent(
                    indices[0], indices[1], result.textSelection));
        } else if (result != null && result.textClassification != null) {
            logEvent(SelectionEvent.createSelectionModifiedEvent(
                    indices[0], indices[1], result.textClassification));
        } else {
            logEvent(SelectionEvent.createSelectionModifiedEvent(indices[0], indices[1]));
        }
    }

    public void logSelectionAction(
            String selectionText, int startOffset, int action, SelectionClient.Result result) {
        if (mSession == null) {
            return;
        }
        if (!mConverter.updateSelectionState(selectionText, startOffset)) {
            // DOM change detected, end logging session.
            endTextClassificationSession();
            return;
        }

        int endOffset = startOffset + selectionText.length();
        int[] indices = new int[2];
        if (!mConverter.getWordDelta(startOffset, endOffset, indices)) {
            // Invalid indices, end logging session.
            endTextClassificationSession();
            return;
        }

        if (DEBUG) {
            Log.d(TAG, "logSelectionAction [%d, %d)", indices[0], indices[1]);
            Log.d(TAG, "logSelectionAction ActionType = %d", action);
        }

        if (result != null && result.textClassification != null) {
            logEvent(SelectionEvent.createSelectionActionEvent(
                    indices[0], indices[1], action, result.textClassification));
        } else {
            logEvent(SelectionEvent.createSelectionActionEvent(indices[0], indices[1], action));
        }

        if (SelectionEvent.isTerminal(action)) {
            endTextClassificationSession();
        }
    }

    public TextClassifier createSession(Context context, boolean editable) {
        TextClassificationContext textClassificationContext =
                new TextClassificationContext
                        .Builder(mContext.getPackageName(),
                                editable ? TextClassifier.WIDGET_TYPE_EDIT_WEBVIEW
                                         : TextClassifier.WIDGET_TYPE_WEBVIEW)
                        .build();
        TextClassificationManager tcm = (TextClassificationManager) context.getSystemService(
                Context.TEXT_CLASSIFICATION_SERVICE);
        return tcm.createTextClassificationSession(textClassificationContext);
    }

    private void endTextClassificationSession() {
        if (mSession == null || mSession.isDestroyed()) {
            return;
        }
        mSession.destroy();
        mSession = null;
    }

    public void logEvent(SelectionEvent selectionEvent) {
        mSession.onSelectionEvent(selectionEvent);
    }
}
