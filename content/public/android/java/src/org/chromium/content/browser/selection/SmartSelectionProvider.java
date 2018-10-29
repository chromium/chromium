// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.LocaleList;
import android.support.annotation.IntDef;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextSelection;

import org.chromium.base.task.AsyncTask;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * Controls Smart Text selection. Talks to the Android TextClassificationManager API.
 */
public class SmartSelectionProvider {
    private static final String TAG = "SmartSelProvider";

    @IntDef({CLASSIFY, SUGGEST_AND_CLASSIFY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface RequestType {}

    private static final int CLASSIFY = 0;
    private static final int SUGGEST_AND_CLASSIFY = 1;

    private SelectionClient.ResultCallback mResultCallback;
    private WindowAndroid mWindowAndroid;
    private ClassificationTask mClassificationTask;
    private TextClassifier mTextClassifier;

    private Handler mHandler;
    private Runnable mFailureResponseRunnable;

    public SmartSelectionProvider(
            SelectionClient.ResultCallback callback, WindowAndroid windowAndroid) {
        mResultCallback = callback;
        mWindowAndroid = windowAndroid;
        mHandler = new Handler();
        mFailureResponseRunnable = new Runnable() {
            @Override
            public void run() {
                mResultCallback.onClassified(new SelectionClient.Result());
            }
        };
    }

    public void sendSuggestAndClassifyRequest(
            CharSequence text, int start, int end, Locale[] locales) {
        sendSmartSelectionRequest(SUGGEST_AND_CLASSIFY, text, start, end, locales);
    }

    public void sendClassifyRequest(CharSequence text, int start, int end, Locale[] locales) {
        sendSmartSelectionRequest(CLASSIFY, text, start, end, locales);
    }

    public void cancelAllRequests() {
        if (mClassificationTask != null) {
            mClassificationTask.cancel(false);
            mClassificationTask = null;
        }
    }

    public void setTextClassifier(TextClassifier textClassifier) {
        mTextClassifier = textClassifier;
    }

    // TODO(wnwen): Remove this suppression once the constant is added to lint.
    @SuppressLint("WrongConstant")
    @TargetApi(Build.VERSION_CODES.O)
    public TextClassifier getTextClassifier() {
        if (mTextClassifier != null) return mTextClassifier;

        Context context = mWindowAndroid.getContext().get();
        if (context == null) return null;

        return ((TextClassificationManager) context.getSystemService(
                        Context.TEXT_CLASSIFICATION_SERVICE))
                .getTextClassifier();
    }

    public TextClassifier getCustomTextClassifier() {
        return mTextClassifier;
    }

    @TargetApi(Build.VERSION_CODES.O)
    private void sendSmartSelectionRequest(
            @RequestType int requestType, CharSequence text, int start, int end, Locale[] locales) {
        TextClassifier classifier = getTextClassifier();
        if (classifier == null || classifier == TextClassifier.NO_OP) {
            mHandler.post(mFailureResponseRunnable);
            return;
        }

        if (mClassificationTask != null) {
            mClassificationTask.cancel(false);
            mClassificationTask = null;
        }

        mClassificationTask =
                new ClassificationTask(classifier, requestType, text, start, end, locales);
        mClassificationTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @TargetApi(Build.VERSION_CODES.O)
    private class ClassificationTask extends AsyncTask<SelectionClient.Result> {
        private final TextClassifier mTextClassifier;
        private final int mRequestType;
        private final CharSequence mText;
        private final int mOriginalStart;
        private final int mOriginalEnd;
        private final Locale[] mLocales;

        ClassificationTask(TextClassifier classifier, @RequestType int requestType,
                CharSequence text, int start, int end, Locale[] locales) {
            mTextClassifier = classifier;
            mRequestType = requestType;
            mText = text;
            mOriginalStart = start;
            mOriginalEnd = end;
            mLocales = locales;
        }

        @Override
        protected SelectionClient.Result doInBackground() {
            int start = mOriginalStart;
            int end = mOriginalEnd;

            TextSelection textSelection = null;

            if (mRequestType == SUGGEST_AND_CLASSIFY) {
                textSelection = mTextClassifier.suggestSelection(
                        mText, start, end, makeLocaleList(mLocales));
                start = Math.max(0, textSelection.getSelectionStartIndex());
                end = Math.min(mText.length(), textSelection.getSelectionEndIndex());
                if (isCancelled()) return new SelectionClient.Result();
            }

            TextClassification tc =
                    mTextClassifier.classifyText(mText, start, end, makeLocaleList(mLocales));
            return makeResult(start, end, tc, textSelection);
        }

        @SuppressLint("NewApi")
        private LocaleList makeLocaleList(Locale[] locales) {
            if (locales == null || locales.length == 0) return null;
            return new LocaleList(locales);
        }

        private SelectionClient.Result makeResult(
                int start, int end, TextClassification tc, TextSelection ts) {
            SelectionClient.Result result = new SelectionClient.Result();

            result.startAdjust = start - mOriginalStart;
            result.endAdjust = end - mOriginalEnd;
            result.label = tc.getLabel();
            result.icon = tc.getIcon();
            result.intent = tc.getIntent();
            result.onClickListener = tc.getOnClickListener();
            result.textSelection = ts;
            result.textClassification = tc;

            return result;
        }

        @Override
        protected void onPostExecute(SelectionClient.Result result) {
            mResultCallback.onClassified(result);
        }
    }
}
