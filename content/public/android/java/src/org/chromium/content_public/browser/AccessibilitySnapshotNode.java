// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import java.util.ArrayList;

/**
 * A light-weight data structure to encode key information from the accessibility
 * tree for operations that need a quick snapshot of the web content. This is different
 * from WebContentsAccessibility.java, which maintains a persistent Android
 * accessibility tree that can be queried synchronously by the Android framework.
 */
public class AccessibilitySnapshotNode {
    public int x;
    public int y;
    public int width;
    public int height;
    public boolean isRootNode;
    public float textSize;
    public String text;
    public String className;
    // True if the style information exists, false if not.
    public boolean hasStyle;
    // Style parameters, valid only if hasStyle is true.
    public int color;
    public int bgcolor;
    public boolean bold;
    public boolean italic;
    public boolean underline;
    public boolean lineThrough;

    public boolean hasSelection;
    public int startSelection;
    public int endSelection;

    public String htmlTag;
    public String cssDisplay;

    // HTML attributes: array of [lowercase HTML attribute name, value].
    public String[][] htmlAttributes;

    public ArrayList<AccessibilitySnapshotNode> children =
            new ArrayList<AccessibilitySnapshotNode>();

    public AccessibilitySnapshotNode(String text, String className) {
        this.text = text;
        this.className = className;
    }

    public void setStyle(int color, int bgcolor, float textSize, boolean bold, boolean italic,
            boolean underline, boolean lineThrough) {
        this.color = color;
        this.bgcolor = bgcolor;
        this.textSize = textSize;
        this.bold = bold;
        this.italic = italic;
        this.underline = underline;
        this.lineThrough = lineThrough;
        hasStyle = true;
    }

    public void setSelection(int start, int end) {
        hasSelection = true;
        startSelection = start;
        endSelection = end;
    }

    public void setLocationInfo(int x, int y, int width, int height, boolean isRootNode) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
        this.isRootNode = isRootNode;
    }

    public void setHtmlInfo(String htmlTag, String cssDisplay, String[][] htmlAttributes) {
        this.htmlTag = htmlTag;
        this.cssDisplay = cssDisplay;
        this.htmlAttributes = htmlAttributes;
    }

    public void addChild(AccessibilitySnapshotNode node) {
        children.add(node);
    }
}
