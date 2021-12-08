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
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.base.annotations.JNINamespace;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Subclass of WebContentsAccessibility for O
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.O)
public class OWebContentsAccessibility extends WebContentsAccessibilityImpl {
    // static instances of the two types of actions that can be added to nodes as the array is not
    // node-specific and this will save on recreation of many lists per page.
    private static List<String> sTextCharacterLocation =
            Arrays.asList(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);

    private static List<String> sRequestImageData =
            Arrays.asList(EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY);

    // Set of all nodes that have received a request to populate image data. The request only needs
    // to be run once per node, and it completes asynchronously. We track which nodes have already
    // started the async request so that if downstream apps request the same node multiple times
    // we can avoid doing the extra work.
    private final Set<Integer> mImageDataRequestedNodes = new HashSet<Integer>();

    OWebContentsAccessibility(AccessibilityDelegate delegate) {
        super(delegate);
    }

    @Override
    public void clearNodeInfoCacheForGivenId(int virtualViewId) {
        if (mImageDataRequestedNodes != null) {
            mImageDataRequestedNodes.remove(virtualViewId);
        }
        super.clearNodeInfoCacheForGivenId(virtualViewId);
    }

    @Override
    protected void setAccessibilityNodeInfoOAttributes(AccessibilityNodeInfo node,
            boolean hasCharacterLocations, boolean hasImage, String hint) {
        node.setHintText(hint);

        if (hasCharacterLocations) {
            node.setAvailableExtraData(sTextCharacterLocation);
        } else if (hasImage) {
            node.setAvailableExtraData(sRequestImageData);
        }
    }

    @Override
    public void addExtraDataToAccessibilityNodeInfo(
            int virtualViewId, AccessibilityNodeInfo info, String extraDataKey, Bundle arguments) {
        switch (extraDataKey) {
            case EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY:
                getExtraDataTextCharacterLocations(virtualViewId, info, arguments);
                break;
            case EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY:
                getImageData(virtualViewId, info);
                break;
        }
    }

    private void getExtraDataTextCharacterLocations(
            int virtualViewId, AccessibilityNodeInfo info, Bundle arguments) {
        if (!areInlineTextBoxesLoaded(virtualViewId)) {
            loadInlineTextBoxes(virtualViewId);
        }

        int positionInfoStartIndex =
                arguments.getInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, -1);
        int positionInfoLength =
                arguments.getInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, -1);
        if (positionInfoLength <= 0 || positionInfoStartIndex < 0) return;

        int[] coords = getCharacterBoundingBoxes(
                virtualViewId, positionInfoStartIndex, positionInfoLength);
        if (coords == null) return;
        assert coords.length == positionInfoLength * 4;

        RectF[] boundingRects = new RectF[positionInfoLength];
        for (int i = 0; i < positionInfoLength; i++) {
            Rect rect = new Rect(
                    coords[4 * i + 0], coords[4 * i + 1], coords[4 * i + 2], coords[4 * i + 3]);
            convertWebRectToAndroidCoordinates(rect, info.getExtras());
            boundingRects[i] = new RectF(rect);
        }

        info.getExtras().putParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, boundingRects);
    }

    private void getImageData(int virtualViewId, AccessibilityNodeInfo info) {
        boolean hasSentPreviousRequest = mImageDataRequestedNodes.contains(virtualViewId);
        // If the below call returns true, then image data has been set on the node.
        if (!WebContentsAccessibilityImplJni.get().getImageData(mNativeObj,
                    OWebContentsAccessibility.this, info, virtualViewId, hasSentPreviousRequest)) {
            // If the above call returns false, then the data was missing. The native-side code
            // will have started the asynchronous process to populate the image data if no previous
            // request has been sent. Add this |virtualViewId| to the list of requested nodes.
            mImageDataRequestedNodes.add(virtualViewId);
        }
    }
}
