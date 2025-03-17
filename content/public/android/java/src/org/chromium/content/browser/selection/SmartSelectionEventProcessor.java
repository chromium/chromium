// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Build;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionEventProcessor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Logs various selection events and manages the lifecycle of a text classifier session.
 * We are logging word indices here. For one example:
 *     New York City , NY
 *    -1   0    1    2 3  4
 * We selected "York" at the first place, so York is [0, 1). Then we assume that Smart Selection
 * expanded the selection range to the whole "New York City , NY", we need to log [-1, 4). After
 * that, we single tap on "City", Smart Selection reset get triggered, we need to log [1, 2). Spaces
 * are ignored but we count each punctuation mark as a word.
 */
@RequiresApi(Build.VERSION_CODES.P)
@NullMarked
public class SmartSelectionEventProcessor implements SelectionEventProcessor {
    private static final String TAG = "SmartSelectionLogger";
    private static final boolean DEBUG = false;

    // May be null if {@link onWindowAndroidChanged()} sets it to null.
    private @Nullable WindowAndroid mWindowAndroid;

    private @Nullable TextClassifier mSession;

    private @Nullable SelectionIndicesConverter mConverter;

    public static @Nullable SmartSelectionEventProcessor create(WebContents webContents) {
        var topWindow = webContents.getTopLevelNativeWindow();
        if (topWindow == null || topWindow.getContext().get() == null) {
            return null;
        }
        return new SmartSelectionEventProcessor(webContents);
    }

    private SmartSelectionEventProcessor(WebContents webContents) {
        mWindowAndroid = webContents.getTopLevelNativeWindow();
        WindowEventObserverManager manager = WindowEventObserverManager.maybeFrom(webContents);
        if (manager != null) {
            manager.addObserver(
                    new WindowEventObserver() {
                        @Override
                        public void onWindowAndroidChanged(
                                @Nullable WindowAndroid newWindowAndroid) {
                            mWindowAndroid = newWindowAndroid;
                        }
                    });
        }
    }

    public void onSelectionStarted(String selectionText, int startOffset, boolean editable) {
        if (mWindowAndroid == null) return;

        Context context = mWindowAndroid.getContext().get();
        if (context == null) return;

        mSession = createSession(context, editable);
        mConverter = new SelectionIndicesConverter();
        mConverter.updateSelectionState(selectionText, startOffset);
        mConverter.setInitialStartOffset(startOffset);

        if (DEBUG) Log.d(TAG, "logSelectionStarted");
        logEvent(SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_MANUAL, 0));
    }

    public void onSelectionModified(
            String selectionText, int startOffset, SelectionClient.@Nullable Result result) {
        if (mSession == null) return;
        assumeNonNull(mConverter);
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
            logEvent(
                    SelectionEvent.createSelectionModifiedEvent(
                            indices[0], indices[1], result.textSelection));
        } else if (result != null && result.textClassification != null) {
            logEvent(
                    SelectionEvent.createSelectionModifiedEvent(
                            indices[0], indices[1], result.textClassification));
        } else {
            logEvent(SelectionEvent.createSelectionModifiedEvent(indices[0], indices[1]));
        }
    }

    public void onSelectionAction(
            String selectionText,
            int startOffset,
            int action,
            SelectionClient.@Nullable Result result) {
        if (mSession == null) {
            return;
        }
        assumeNonNull(mConverter);
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
            logEvent(
                    SelectionEvent.createSelectionActionEvent(
                            indices[0], indices[1], action, result.textClassification));
        } else {
            logEvent(SelectionEvent.createSelectionActionEvent(indices[0], indices[1], action));
        }

        if (SelectionEvent.isTerminal(action)) {
            endTextClassificationSession();
        }
    }

    private TextClassifier createSession(Context context, boolean editable) {
        TextClassificationContext textClassificationContext =
                new TextClassificationContext.Builder(
                                context.getPackageName(),
                                editable
                                        ? TextClassifier.WIDGET_TYPE_EDIT_WEBVIEW
                                        : TextClassifier.WIDGET_TYPE_WEBVIEW)
                        .build();
        TextClassificationManager tcm =
                (TextClassificationManager)
                        context.getSystemService(Context.TEXT_CLASSIFICATION_SERVICE);
        return tcm.createTextClassificationSession(textClassificationContext);
    }

    private void endTextClassificationSession() {
        if (mSession == null || mSession.isDestroyed()) {
            return;
        }
        mSession.destroy();
        mSession = null;
    }

    @RequiresNonNull("mSession")
    public void logEvent(SelectionEvent selectionEvent) {
        mSession.onSelectionEvent(selectionEvent);
    }

    public @Nullable TextClassifier getTextClassifierSession() {
        return mSession;
    }
}
