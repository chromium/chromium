// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Subclass of WebContentsAccessibility for KitKat.
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.KITKAT)
public class KitKatWebContentsAccessibility extends WebContentsAccessibilityImpl {
    private String mSupportedHtmlElementTypes;

    KitKatWebContentsAccessibility(WebContents webContents) {
        super(webContents);
    }

    @Override
    protected void onNativeInit() {
        super.onNativeInit();
        mSupportedHtmlElementTypes =
                WebContentsAccessibilityImplJni.get().getSupportedHtmlElementTypes(
                        mNativeObj, KitKatWebContentsAccessibility.this);
    }

    @Override
    protected void setAccessibilityNodeInfoKitKatAttributes(AccessibilityNodeInfo node,
            boolean isRoot, boolean isEditableText, String role, String roleDescription,
            String hint, int selectionStartIndex, int selectionEndIndex, boolean hasImage,
            boolean contentInvalid, String targetUrl) {
        Bundle bundle = node.getExtras();
        bundle.putCharSequence("AccessibilityNodeInfo.chromeRole", role);
        bundle.putCharSequence("AccessibilityNodeInfo.roleDescription", roleDescription);
        bundle.putCharSequence("AccessibilityNodeInfo.hint", hint);
        if (!targetUrl.isEmpty()) {
            bundle.putCharSequence("AccessibilityNodeInfo.targetUrl", targetUrl);
        }
        if (hasImage) bundle.putCharSequence("AccessibilityNodeInfo.hasImage", "true");
        if (isRoot) {
            bundle.putCharSequence(
                    "ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES", mSupportedHtmlElementTypes);
        }
        if (isEditableText) {
            node.setEditable(true);
            node.setTextSelection(selectionStartIndex, selectionEndIndex);
        }

        node.setContentInvalid(contentInvalid);
    }

    @Override
    protected int getAccessibilityServiceCapabilitiesMask() {
        int capabilitiesMask = 0;
        for (AccessibilityServiceInfo service :
                mAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK)) {
            capabilitiesMask |= service.getCapabilities();
        }
        return capabilitiesMask;
    }
}
