// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.base.annotations.JNINamespace;

/**
 * Subclass of WebContentsAccessibility for R
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.R)
public class RWebContentsAccessibility extends PieWebContentsAccessibility {
    RWebContentsAccessibility(AccessibilityDelegate delegate) {
        super(delegate);
    }

    @Override
    protected void setAccessibilityNodeInfoText(AccessibilityNodeInfo node, String text,
            boolean annotateAsLink, boolean isEditableText, String language, int[] suggestionStarts,
            int[] suggestionEnds, String[] suggestions, String stateDescription) {
        super.setAccessibilityNodeInfoText(node, text, annotateAsLink, isEditableText, language,
                suggestionStarts, suggestionEnds, suggestions, stateDescription);

        // For Android R and higher, we will not rely on concatenating text and stateDescription,
        // and will instead revert text to original content and set stateDescription separately.
        if (stateDescription != null && !stateDescription.isEmpty()) {
            CharSequence computedText = computeText(
                    text, annotateAsLink, language, suggestionStarts, suggestionEnds, suggestions);

            node.setText(computedText);
            node.setStateDescription(stateDescription);
        }
    }
}
