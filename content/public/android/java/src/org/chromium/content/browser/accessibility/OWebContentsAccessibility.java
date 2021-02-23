// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;

/**
 * Subclass of WebContentsAccessibility for O
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.O)
public class OWebContentsAccessibility extends WebContentsAccessibilityImpl {
    OWebContentsAccessibility(WebContents webContents) {
        super(webContents);
    }

    @Override
    protected void setAccessibilityNodeInfoOAttributes(
            AccessibilityNodeInfo node, boolean hasCharacterLocations, String hint) {
        if (hasCharacterLocations) {
            node.setAvailableExtraData(Arrays.asList(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY));
        }

        node.setHintText(hint);
    }

    @Override
    public void addExtraDataToAccessibilityNodeInfo(
            int virtualViewId, AccessibilityNodeInfo info, String extraDataKey, Bundle arguments) {
        if (!extraDataKey.equals(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY)) return;

        if (!WebContentsAccessibilityImplJni.get().areInlineTextBoxesLoaded(
                    mNativeObj, OWebContentsAccessibility.this, virtualViewId)) {
            WebContentsAccessibilityImplJni.get().loadInlineTextBoxes(
                    mNativeObj, OWebContentsAccessibility.this, virtualViewId);
        }

        int positionInfoStartIndex =
                arguments.getInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, -1);
        int positionInfoLength =
                arguments.getInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, -1);
        if (positionInfoLength <= 0 || positionInfoStartIndex < 0) return;

        int[] coords = WebContentsAccessibilityImplJni.get().getCharacterBoundingBoxes(mNativeObj,
                OWebContentsAccessibility.this, virtualViewId, positionInfoStartIndex,
                positionInfoLength);
        if (coords == null) return;
        assert coords.length == positionInfoLength * 4;

        RectF[] boundingRects = new RectF[positionInfoLength];
        for (int i = 0; i < positionInfoLength; i++) {
            Rect rect = new Rect(
                    coords[4 * i + 0], coords[4 * i + 1], coords[4 * i + 2], coords[4 * i + 3]);
            convertWebRectToAndroidCoordinates(rect);
            boundingRects[i] = new RectF(rect);
        }

        info.getExtras().putParcelableArray(extraDataKey, boundingRects);
    }

    @Override
    protected void createVirtualStructure(ViewStructure viewNode, AccessibilitySnapshotNode node,
            final boolean ignoreScrollOffset) {
        // Store the tag name in HtmlInfo.
        ViewStructure.HtmlInfo.Builder htmlBuilder = viewNode.newHtmlInfoBuilder(node.htmlTag);
        viewNode.setHtmlInfo(htmlBuilder.build());

        super.createVirtualStructure(viewNode, node, ignoreScrollOffset);
    }
}
