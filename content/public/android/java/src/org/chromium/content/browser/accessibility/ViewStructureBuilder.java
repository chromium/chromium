// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_HEIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_LEFT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_WIDTH;

import android.app.assist.AssistStructure.ViewNode;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.ViewStructure;

import org.jni_zero.CalledByNative;

import org.chromium.content.browser.RenderCoordinatesImpl;

import java.util.ArrayList;
import java.util.Arrays;

/**
 */
public class ViewStructureBuilder {
    private RenderCoordinatesImpl mRenderCoordinates;

    public static ViewStructureBuilder create(RenderCoordinatesImpl renderCoordinates) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return new OViewStructureBuilder(renderCoordinates);
        }
        return new ViewStructureBuilder(renderCoordinates);
    }

    protected ViewStructureBuilder(RenderCoordinatesImpl renderCoordinates) {
        this.mRenderCoordinates = renderCoordinates;
    }

    @CalledByNative
    private void populateViewStructureNode(ViewStructure node, String text, boolean hasSelection,
            int selStart, int selEnd, int color, int bgcolor, float size, boolean bold,
            boolean italic, boolean underline, boolean lineThrough, String className,
            int childCount) {
        node.setClassName(className);
        node.setChildCount(childCount);

        if (hasSelection) {
            node.setText(text, selStart, selEnd);
        } else {
            node.setText(text);
        }

        // if size is smaller than 0, then style information does not exist.
        if (size >= 0.0) {
            int style = (bold ? ViewNode.TEXT_STYLE_BOLD : 0)
                    | (italic ? ViewNode.TEXT_STYLE_ITALIC : 0)
                    | (underline ? ViewNode.TEXT_STYLE_UNDERLINE : 0)
                    | (lineThrough ? ViewNode.TEXT_STYLE_STRIKE_THRU : 0);
            node.setTextStyle(size, color, bgcolor, style);
        }
    }

    @CalledByNative
    private void setViewStructureNodeBounds(ViewStructure node, boolean isRootNode,
            int parentRelativeLeft, int parentRelativeTop, int width, int height, int unclippedLeft,
            int unclippedTop, int unclippedWidth, int unclippedHeight) {
        int left = (int) mRenderCoordinates.fromLocalCssToPix(parentRelativeLeft);
        int top = (int) mRenderCoordinates.fromLocalCssToPix(parentRelativeTop);
        width = (int) mRenderCoordinates.fromLocalCssToPix(width);
        height = (int) mRenderCoordinates.fromLocalCssToPix(height);
        Rect boundsInParent = new Rect(left, top, left + width, top + height);
        if (isRootNode) {
            // Offset of the web content relative to the View.
            boundsInParent.offset(0, (int) mRenderCoordinates.getContentOffsetYPix());
        }

        node.setDimens(boundsInParent.left, boundsInParent.top, 0, 0, width, height);

        // Add unclipped bounds in the Bundle extras for services interested in these values.
        int unclippedLeftCSS = (int) mRenderCoordinates.fromLocalCssToPix(unclippedLeft);
        int unclippedTopCSS = (int) mRenderCoordinates.fromLocalCssToPix(unclippedTop);
        int unclippedWidthCSS = (int) mRenderCoordinates.fromLocalCssToPix(unclippedWidth);
        int unclippedHeightCSS = (int) mRenderCoordinates.fromLocalCssToPix(unclippedHeight);

        Bundle extras = node.getExtras();
        extras.putInt(EXTRAS_KEY_UNCLIPPED_LEFT, unclippedLeftCSS);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_TOP, unclippedTopCSS);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_WIDTH, unclippedWidthCSS);
        extras.putInt(EXTRAS_KEY_UNCLIPPED_HEIGHT, unclippedHeightCSS);
    }

    @CalledByNative
    protected void setViewStructureNodeHtmlInfo(
            ViewStructure node, String htmlTag, String cssDisplay, String[][] htmlAttributes) {
        Bundle extras = node.getExtras();
        extras.putCharSequence("htmlTag", htmlTag);
        extras.putCharSequence("display", cssDisplay);
        for (String[] attr : htmlAttributes) {
            extras.putCharSequence(attr[0], attr[1]);
        }
    }

    @CalledByNative
    protected void setViewStructureNodeHtmlMetadata(ViewStructure node, String[] metadataStrings) {
        Bundle extras = node.getExtras();
        extras.putStringArrayList(
                "metadata", new ArrayList<String>(Arrays.asList(metadataStrings)));
    }

    @CalledByNative
    private void commitViewStructureNode(ViewStructure node) {
        node.asyncCommit();
    }

    @CalledByNative
    private ViewStructure addViewStructureNodeChild(ViewStructure node, int index) {
        return node.asyncNewChild(index);
    }
}
