// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.annotation.SuppressLint;
import android.app.RemoteAction;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.os.LocaleList;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextSelection;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** Controls Smart Text selection. Talks to the Android TextClassificationManager API. */
public class SmartSelectionProvider {
    private static final String TAG = "SmartSelProvider";

    @IntDef({RequestType.CLASSIFY, RequestType.SUGGEST_AND_CLASSIFY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface RequestType {
        int CLASSIFY = 0;
        int SUGGEST_AND_CLASSIFY = 1;
    }

    private SelectionClient.ResultCallback mResultCallback;
    private WindowAndroid mWindowAndroid;
    private ClassificationTask mClassificationTask;
    private TextClassifier mTextClassifier;

    private Handler mHandler;
    private Runnable mFailureResponseRunnable;
    @Nullable private final SmartSelectionEventProcessor mSelectionEventProcessor;

    public SmartSelectionProvider(
            SelectionClient.ResultCallback callback,
            WebContents webContents,
            @Nullable SmartSelectionEventProcessor selectionEventProcessor) {
        mResultCallback = callback;
        mWindowAndroid = webContents.getTopLevelNativeWindow();
        WindowEventObserverManager manager = WindowEventObserverManager.from(webContents);
        if (manager != null) {
            manager.addObserver(
                    new WindowEventObserver() {
                        @Override
                        public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
                            mWindowAndroid = newWindowAndroid;
                        }
                    });
        }

        mHandler = new Handler();
        mFailureResponseRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        mResultCallback.onClassified(new SelectionClient.Result());
                    }
                };
        mSelectionEventProcessor = selectionEventProcessor;
    }

    public void sendSuggestAndClassifyRequest(CharSequence text, int start, int end) {
        sendSmartSelectionRequest(RequestType.SUGGEST_AND_CLASSIFY, text, start, end);
    }

    public void sendClassifyRequest(CharSequence text, int start, int end) {
        sendSmartSelectionRequest(RequestType.CLASSIFY, text, start, end);
    }

    public void cancelAllRequests() {
        if (mClassificationTask != null) {
            mClassificationTask.cancel(false);
            mClassificationTask = null;
        }
    }

    @RequiresApi(Build.VERSION_CODES.O)
    public void setTextClassifier(TextClassifier textClassifier) {
        mTextClassifier = textClassifier;

        Context context = mWindowAndroid.getContext().get();
        if (context == null) {
            return;
        }
        ((TextClassificationManager) context.getSystemService(Context.TEXT_CLASSIFICATION_SERVICE))
                .setTextClassifier(textClassifier);
    }

    // TODO(wnwen): Remove this suppression once the constant is added to lint.
    @SuppressLint("WrongConstant")
    @RequiresApi(Build.VERSION_CODES.O)
    public TextClassifier getTextClassifier() {
        if (mTextClassifier != null) return mTextClassifier;

        if (mWindowAndroid == null) {
            return null;
        }
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return null;

        return ((TextClassificationManager)
                        context.getSystemService(Context.TEXT_CLASSIFICATION_SERVICE))
                .getTextClassifier();
    }

    public TextClassifier getCustomTextClassifier() {
        return mTextClassifier;
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private TextClassifier getTextClassificationSession() {
        if (mWindowAndroid == null) {
            return null;
        }
        Context context = mWindowAndroid.getContext().get();
        if (context == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P || mSelectionEventProcessor == null) {
            return getTextClassifier();
        }
        TextClassifier textClassifierSession = mSelectionEventProcessor.getTextClassifierSession();
        if (textClassifierSession == null || textClassifierSession.isDestroyed()) {
            return getTextClassifier();
        }
        return textClassifierSession;
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private void sendSmartSelectionRequest(
            @RequestType int requestType, CharSequence text, int start, int end) {
        TextClassifier classifier = getTextClassificationSession();
        if (classifier == null || classifier == TextClassifier.NO_OP) {
            mHandler.post(mFailureResponseRunnable);
            return;
        }

        if (mClassificationTask != null) {
            mClassificationTask.cancel(false);
            mClassificationTask = null;
        }

        // We checked mWindowAndroid.getContext().get() is not null in getTextClassifier(), so pass
        // the value directly here.
        mClassificationTask =
                new ClassificationTask(
                        classifier,
                        requestType,
                        text,
                        start,
                        end,
                        mWindowAndroid.getContext().get());
        mClassificationTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private class ClassificationTask extends AsyncTask<SelectionClient.Result> {
        private final TextClassifier mTextClassifier;
        private final @RequestType int mRequestType;
        private final CharSequence mText;
        private final int mOriginalStart;
        private final int mOriginalEnd;
        private final Context mContext;

        ClassificationTask(
                TextClassifier classifier,
                @RequestType int requestType,
                CharSequence text,
                int start,
                int end,
                Context context) {
            mTextClassifier = classifier;
            mRequestType = requestType;
            mText = text;
            mOriginalStart = start;
            mOriginalEnd = end;
            mContext = context;
        }

        @Override
        protected SelectionClient.Result doInBackground() {
            int start = mOriginalStart;
            int end = mOriginalEnd;

            TextSelection textSelection = null;
            TextClassification textClassification = null;

            try {
                if (mRequestType == RequestType.SUGGEST_AND_CLASSIFY) {
                    textSelection = suggestSelection(start, end);
                    start = Math.max(0, textSelection.getSelectionStartIndex());
                    end = Math.min(mText.length(), textSelection.getSelectionEndIndex());
                    if (isCancelled()) {
                        return new SelectionClient.Result();
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        textClassification = textSelection.getTextClassification();
                    }
                }

                if (textClassification == null) {
                    textClassification =
                            mTextClassifier.classifyText(
                                    mText, start, end, LocaleList.getAdjustedDefault());
                }
                return makeResult(start, end, textClassification, textSelection);
            } catch (IllegalStateException ex) {
                // An IllegalStateException will be thrown if the text classifier session is
                // destroyed. This could happen if the selection is ended before text classifier
                // finishes processing the text.
                Log.e(TAG, "Failed to use text classifier for smart selection", ex);
                return new SelectionClient.Result();
            }
        }

        private TextSelection suggestSelection(int start, int end) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                return mTextClassifier.suggestSelection(
                        new TextSelection.Request.Builder(mText, start, end)
                                .setDefaultLocales(LocaleList.getAdjustedDefault())
                                .setIncludeTextClassification(true)
                                .build());
            }
            return mTextClassifier.suggestSelection(
                    mText, start, end, LocaleList.getAdjustedDefault());
        }

        private SelectionClient.Result makeResult(
                int start, int end, TextClassification tc, TextSelection ts) {
            SelectionClient.Result result = new SelectionClient.Result();

            result.text = mText.toString();
            result.start = start;
            result.end = end;
            result.startAdjust = start - mOriginalStart;
            result.endAdjust = end - mOriginalEnd;
            result.label = tc.getLabel();
            result.icon = tc.getIcon();
            result.intent = tc.getIntent();
            result.onClickListener = tc.getOnClickListener();
            result.textSelection = ts;
            result.textClassification = tc;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                result.additionalIcons = loadIconDrawables(mContext, result.textClassification);
            }

            return result;
        }

        // Because Icon#loadDrawable() should not be called on UI thread, we pre-load the icons on
        // background thread right after we get the text classification result in
        // SmartSelectionProvider. TextClassification#getActions() is only available on P and above,
        // so
        @RequiresApi(Build.VERSION_CODES.P)
        private List<Drawable> loadIconDrawables(Context context, TextClassification tc) {
            if (context == null || tc == null) return null;

            ArrayList<Drawable> res = new ArrayList<>();
            for (RemoteAction action : tc.getActions()) {
                res.add(action.getIcon().loadDrawable(context));
            }
            return res;
        }

        @Override
        protected void onPostExecute(SelectionClient.Result result) {
            mResultCallback.onClassified(result);
        }
    }
}
