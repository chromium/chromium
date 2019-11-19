// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ReceiverCallNotAllowedException;
import android.os.Build;
import android.text.SpannableString;
import android.text.style.LocaleSpan;
import android.text.style.SuggestionSpan;
import android.util.SparseArray;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

import java.util.Locale;

/**
 * Subclass of WebContentsAccessibility for Lollipop.
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class LollipopWebContentsAccessibility extends KitKatWebContentsAccessibility {
    private static SparseArray<AccessibilityAction> sAccessibilityActionMap =
            new SparseArray<AccessibilityAction>();
    private String mSystemLanguageTag;
    private BroadcastReceiver mBroadcastReceiver;

    LollipopWebContentsAccessibility(WebContents webContents) {
        super(webContents);
    }

    @Override
    protected void onNativeInit() {
        super.onNativeInit();
        mBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mSystemLanguageTag = Locale.getDefault().toLanguageTag();
            }
        };

        // Register a broadcast receiver for locale change for Lollipop or higher version.
        if (mView.isAttachedToWindow()) registerLocaleChangeReceiver();
    }

    @Override
    protected void setAccessibilityNodeInfoLollipopAttributes(AccessibilityNodeInfo node,
            boolean canOpenPopup, boolean contentInvalid, boolean dismissable, boolean multiLine,
            int inputType, int liveRegion, String errorMessage) {
        node.setCanOpenPopup(canOpenPopup);
        node.setContentInvalid(contentInvalid);
        node.setDismissable(contentInvalid);
        node.setMultiLine(multiLine);
        node.setInputType(inputType);
        node.setLiveRegion(liveRegion);
        node.setError(errorMessage);
    }

    @Override
    protected void setAccessibilityNodeInfoCollectionInfo(
            AccessibilityNodeInfo node, int rowCount, int columnCount, boolean hierarchical) {
        node.setCollectionInfo(
                AccessibilityNodeInfo.CollectionInfo.obtain(rowCount, columnCount, hierarchical));
    }

    @Override
    protected void setAccessibilityNodeInfoCollectionItemInfo(AccessibilityNodeInfo node,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading) {
        node.setCollectionItemInfo(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                rowIndex, rowSpan, columnIndex, columnSpan, heading));
    }

    @Override
    protected void setAccessibilityNodeInfoRangeInfo(
            AccessibilityNodeInfo node, int rangeType, float min, float max, float current) {
        node.setRangeInfo(AccessibilityNodeInfo.RangeInfo.obtain(rangeType, min, max, current));
    }

    @Override
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfo node, String viewIdResourceName) {
        node.setViewIdResourceName(viewIdResourceName);
    }

    @Override
    protected void setAccessibilityEventLollipopAttributes(AccessibilityEvent event,
            boolean canOpenPopup, boolean contentInvalid, boolean dismissable, boolean multiLine,
            int inputType, int liveRegion) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventCollectionInfo(
            AccessibilityEvent event, int rowCount, int columnCount, boolean hierarchical) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventHeadingFlag(AccessibilityEvent event, boolean heading) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventCollectionItemInfo(
            AccessibilityEvent event, int rowIndex, int rowSpan, int columnIndex, int columnSpan) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventRangeInfo(
            AccessibilityEvent event, int rangeType, float min, float max, float current) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void addAction(AccessibilityNodeInfo node, int actionId) {
        // The Lollipop SDK requires us to call AccessibilityNodeInfo.addAction with an
        // AccessibilityAction argument, but to simplify things and share more code with
        // the pre-L SDK, we just cache a set of AccessibilityActions mapped by their ID.
        AccessibilityAction action = sAccessibilityActionMap.get(actionId);
        if (action == null) {
            action = new AccessibilityAction(actionId, null);
            sAccessibilityActionMap.put(actionId, action);
        }
        node.addAction(action);
    }

    @Override
    protected CharSequence computeText(String text, boolean annotateAsLink, String language,
            int[] suggestionStarts, int[] suggestionEnds, String[] suggestions) {
        CharSequence charSequence = super.computeText(
                text, annotateAsLink, language, suggestionStarts, suggestionEnds, suggestions);
        if (!language.isEmpty() && !language.equals(mSystemLanguageTag)) {
            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }
            Locale locale = Locale.forLanguageTag(language);
            spannable.setSpan(new LocaleSpan(locale), 0, spannable.length(), 0);
            charSequence = spannable;
        }

        if (suggestionStarts != null && suggestionStarts.length > 0) {
            assert suggestionEnds != null;
            assert suggestionEnds.length == suggestionStarts.length;
            assert suggestions != null;
            assert suggestions.length == suggestionStarts.length;

            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }

            for (int i = 0; i < suggestionStarts.length; i++) {
                String[] suggestionArray = new String[1];
                suggestionArray[0] = suggestions[i];
                int flags = SuggestionSpan.FLAG_MISSPELLED;
                SuggestionSpan suggestionSpan =
                        new SuggestionSpan(mContext, suggestionArray, flags);
                spannable.setSpan(suggestionSpan, suggestionStarts[i], suggestionEnds[i], 0);
            }
            charSequence = spannable;
        }

        return charSequence;
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (!isNativeInitialized()) return;
        ContextUtils.getApplicationContext().unregisterReceiver(mBroadcastReceiver);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        registerLocaleChangeReceiver();
    }

    private void registerLocaleChangeReceiver() {
        if (!isNativeInitialized()) return;
        try {
            IntentFilter filter = new IntentFilter(Intent.ACTION_LOCALE_CHANGED);
            ContextUtils.getApplicationContext().registerReceiver(mBroadcastReceiver, filter);
        } catch (ReceiverCallNotAllowedException e) {
            // WebView may be running inside a BroadcastReceiver, in which case registerReceiver is
            // not allowed.
        }
        mSystemLanguageTag = Locale.getDefault().toLanguageTag();
    }
}
