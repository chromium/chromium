// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.view.accessibility.AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.autofill.AutofillManager;

import org.chromium.base.annotations.JNINamespace;

/**
 * Subclass of WebContentsAccessibility for P
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.P)
public class PieWebContentsAccessibility extends OWebContentsAccessibility {
    PieWebContentsAccessibility(AccessibilityDelegate delegate) {
        super(delegate);
        AutofillManager autofillManager = mContext.getSystemService(AutofillManager.class);
        if (autofillManager != null && autofillManager.isEnabled()) {
            // Native accessibility is usually initialized when getAccessibilityNodeProvider is
            // called, but the Autofill compatibility bridge only calls that method after it has
            // received the first accessibility events. To solve the chicken-and-egg problem,
            // always initialize the native parts when the user has an Autofill service enabled.
            refreshState();
            getAccessibilityNodeProvider();
        }
    }

    @Override
    protected void handleDialogModalOpened(int virtualViewId) {
        if (isAccessibilityEnabled()) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
            if (event == null) return;

            event.setContentChangeTypes(CONTENT_CHANGE_TYPE_PANE_APPEARED);
            event.setSource(mView, virtualViewId);
            super.requestSendAccessibilityEvent(event);
        }
    }

    @Override
    protected void setAccessibilityNodeInfoPaneTitle(AccessibilityNodeInfo node, String title) {
        node.setPaneTitle(title);
    }
}
