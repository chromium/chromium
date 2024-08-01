// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.textclassifier.TextClassifier;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionEventProcessor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class that controls Smart Text selection. Smart Text selection automatically augments the
 * selected boundaries and classifies the selected text based on the context. This class requests
 * the selection together with its surrounding text from the focused frame and sends it to
 * SmartSelectionProvider which does the classification itself.
 */
@JNINamespace("content")
public class SmartSelectionClient implements SelectionClient, UserData {
    @IntDef({RequestType.CLASSIFY, RequestType.SUGGEST_AND_CLASSIFY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface RequestType {
        // Request to obtain the type (e.g. phone number, e-mail address) and the most
        // appropriate operation for the selected text.
        int CLASSIFY = 0;

        // Request to obtain the type (e.g. phone number, e-mail address), the most
        // appropriate operation for the selected text and a better selection boundaries.
        int SUGGEST_AND_CLASSIFY = 1;
    }

    // The maximal number of characters on the left and on the right from the current selection.
    // Used for surrounding text request.
    private static final int NUM_EXTRA_CHARS = 240;

    private long mNativeSmartSelectionClient;
    private SmartSelectionProvider mProvider;
    private ResultCallback mCallback;
    private SmartSelectionEventProcessor mSmartSelectionEventProcessor;

    /** Observer list for surrounding text received. */
    private final ObserverList<SurroundingTextCallback> mSurroundingTextReceivedListeners =
            new ObserverList<>();

    /**
     * Creates the SmartSelectionClient if not present. Returns null in case SmartSelectionProvider
     * does not exist in the system.
     */
    public static SmartSelectionClient fromWebContents(
            ResultCallback callback, WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O || windowAndroid == null) return null;

        // Don't do Smart Selection when device is not provisioned or in incognito mode.
        if (!isDeviceProvisioned(windowAndroid.getContext().get()) || webContents.isIncognito()) {
            return null;
        }

        SmartSelectionClient client =
                ((WebContentsImpl) webContents)
                        .getOrSetUserData(SmartSelectionClient.class, SmartSelectionClient::new);
        client.setCallback(callback, webContents);
        return client;
    }

    private SmartSelectionClient(WebContents webContents) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mSmartSelectionEventProcessor = SmartSelectionEventProcessor.create(webContents);
        }
        mNativeSmartSelectionClient =
                SmartSelectionClientJni.get().init(SmartSelectionClient.this, webContents);
    }

    private void setCallback(ResultCallback callback, WebContents webContents) {
        mCallback = callback;
        mProvider =
                new SmartSelectionProvider(callback, webContents, mSmartSelectionEventProcessor);
    }

    @CalledByNative
    private void onNativeSideDestroyed(long nativeSmartSelectionClient) {
        assert nativeSmartSelectionClient == mNativeSmartSelectionClient;
        mNativeSmartSelectionClient = 0;
        mProvider.cancelAllRequests();
    }

    // SelectionClient implementation
    @Override
    public void onSelectionChanged(String selection) {}

    @Override
    public void onSelectionEvent(@SelectionEventType int eventType, float posXPix, float posYPix) {}

    @Override
    public void selectAroundCaretAck(@Nullable SelectAroundCaretResult result) {}

    @Override
    public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
        requestSurroundingText(
                shouldSuggest ? RequestType.SUGGEST_AND_CLASSIFY : RequestType.CLASSIFY);
        return true;
    }

    @Override
    public void cancelAllRequests() {
        if (mNativeSmartSelectionClient != 0) {
            SmartSelectionClientJni.get()
                    .cancelAllRequests(mNativeSmartSelectionClient, SmartSelectionClient.this);
        }

        mProvider.cancelAllRequests();
    }

    @Override
    public SelectionEventProcessor getSelectionEventProcessor() {
        return mSmartSelectionEventProcessor;
    }

    @Override
    public void setTextClassifier(TextClassifier textClassifier) {
        mProvider.setTextClassifier(textClassifier);
    }

    @Override
    public TextClassifier getTextClassifier() {
        return mProvider.getTextClassifier();
    }

    @Override
    public TextClassifier getCustomTextClassifier() {
        return mProvider.getCustomTextClassifier();
    }

    private void requestSurroundingText(@RequestType int callbackData) {
        if (mNativeSmartSelectionClient == 0) {
            onSurroundingTextReceived(callbackData, "", 0, 0);
            return;
        }

        SmartSelectionClientJni.get()
                .requestSurroundingText(
                        mNativeSmartSelectionClient,
                        SmartSelectionClient.this,
                        NUM_EXTRA_CHARS,
                        callbackData);
    }

    @Override
    public void addSurroundingTextReceivedListeners(SurroundingTextCallback observer) {
        mSurroundingTextReceivedListeners.addObserver(observer);
    }

    @Override
    public void removeSurroundingTextReceivedListeners(SurroundingTextCallback observer) {
        mSurroundingTextReceivedListeners.removeObserver(observer);
    }

    @CalledByNative
    private void onSurroundingTextReceived(
            @RequestType int callbackData, String text, int start, int end) {
        for (SurroundingTextCallback observer : mSurroundingTextReceivedListeners) {
            observer.onSurroundingTextReceived(text, start, end);
        }

        if (!textHasValidSelection(text, start, end)) {
            mCallback.onClassified(new Result());
            return;
        }

        switch (callbackData) {
            case RequestType.SUGGEST_AND_CLASSIFY:
                mProvider.sendSuggestAndClassifyRequest(text, start, end);
                break;

            case RequestType.CLASSIFY:
                mProvider.sendClassifyRequest(text, start, end);
                break;

            default:
                assert false : "Unexpected callback data";
                break;
        }
    }

    private static boolean isDeviceProvisioned(Context context) {
        if (context == null || context.getContentResolver() == null) return true;
        // Returns false when device is not provisioned, i.e. before a new device went through
        // signup process.
        return Settings.Global.getInt(
                        context.getContentResolver(), Settings.Global.DEVICE_PROVISIONED, 0)
                != 0;
    }

    private boolean textHasValidSelection(String text, int start, int end) {
        return !TextUtils.isEmpty(text) && 0 <= start && start < end && end <= text.length();
    }

    @NativeMethods
    interface Natives {
        long init(SmartSelectionClient caller, WebContents webContents);

        void requestSurroundingText(
                long nativeSmartSelectionClient,
                SmartSelectionClient caller,
                int numExtraCharacters,
                int callbackData);

        void cancelAllRequests(long nativeSmartSelectionClient, SmartSelectionClient caller);
    }
}
